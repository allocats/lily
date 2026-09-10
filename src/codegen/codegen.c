#include "ast/nodes/types.h"
#include "codegen/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "token/types.h"
#include "types/entries/entries.h"
#include "types/entries/types.h"
#include "types/table/table.h"
#include "utils/macros.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern DriverCtx driver;

static void codegen_file(CodegenCtx* ctx, FileId id);
static bool codegen_ast(CodegenCtx* ctx);

static LLVMValueRef codegen_function_signature(CodegenCtx* ctx, SymbolId id);
static LLVMValueRef codegen_function_declaration(CodegenCtx* ctx, AstNode* node);
static bool         codegen_function_body(CodegenCtx* ctx, AstNode* node, Symbol* symbol, bool* did_fn_return);

static bool         codegen_block(CodegenCtx* ctx, AstNodeId id);

static LLVMValueRef codegen_variable_declaration(CodegenCtx* ctx, AstNode* node);
static bool         codegen_for_loop(CodegenCtx* ctx, AstNode* node);
static bool         codegen_while_loop(CodegenCtx* ctx, AstNode* node);
static bool         codegen_return_statement(CodegenCtx* ctx, AstNode* node);

static LLVMValueRef codegen_lvalue(CodegenCtx* ctx, AstNodeId id);
static LLVMValueRef codegen_expression(CodegenCtx* ctx, AstNodeId id);

static LLVMValueRef codegen_binary_arithmetic(
    CodegenCtx* ctx,
    TokenKind op,
    LLVMValueRef lhs,
    LLVMValueRef rhs,
    TypeId lhs_type,
    TypeId rhs_type
);

static bool         codegen_va_end(CodegenCtx* ctx);

static LLVMTypeRef  type_id_to_llvm(CodegenCtx* ctx, TypeId id);

static LLVMValueRef get_or_insert_string(CodegenCtx* ctx, StringId id);

static bool         is_compound_assignment_op(TokenKind op);
static TokenKind    get_compound_assignment_base_op(TokenKind op);

void codegen() {
    CodegenCtx ctx = {0};

    arena_init(&ctx.arena, ARENA_KB(4), ALIGN_DEFAULT);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    u32 symbol_count = driver.symbol_table.symbol_count;
    ctx.symbol_values = arena_calloc(&ctx.arena, symbol_count * sizeof(LLVMValueRef));

    u32 string_count = driver.string_interner.count;
    ctx.string_values = arena_calloc(&ctx.arena, string_count * sizeof(LLVMValueRef));

    u32 file_count = driver.file_interner.count;

    for (u32 i = 0; i < file_count; i++) {
        codegen_file(&ctx, i);

        arena_reset(&ctx.arena);

        arena_memset(ctx.symbol_values, 0, symbol_count * sizeof(LLVMValueRef));
        arena_memset(ctx.string_values, 0, string_count * sizeof(LLVMValueRef));
    }

    arena_destroy(&ctx.arena);
}

static void codegen_file(CodegenCtx* ctx, FileId id) {
    ctx -> file = file_lookup_id(id); 

    ctx -> ctx     = LLVMContextCreate();
    ctx -> module  = LLVMModuleCreateWithNameInContext(ctx -> file -> path.ptr, ctx -> ctx);
    ctx -> builder = LLVMCreateBuilderInContext(ctx -> ctx);

    assert(ctx -> ctx != null);
    assert(ctx -> module != null);
    assert(ctx -> builder != null);

    if (!codegen_ast(ctx)) {
        goto cleanup;
    }

    char* msg = null;

    if (LLVMVerifyModule(ctx -> module, LLVMReturnStatusAction, &msg) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: module verification failed: %s", msg);
        LLVMDisposeMessage(msg);
        goto cleanup;
    }

    char* host_triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(ctx -> module, host_triple);

    LLVMTargetRef target = null;

    if (LLVMGetTargetFromTriple(host_triple, &target, &msg) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: %s", msg);
        LLVMDisposeMessage(msg);
        goto cleanup;
    }

    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
        target,
        host_triple,
        "generic",
        "",
        LLVMCodeGenLevelDefault,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );

    LLVMPassBuilderOptionsRef pb_options = LLVMCreatePassBuilderOptions();

    LLVMErrorRef error = LLVMRunPasses(
        ctx -> module,
        "globaldce,dce,adce,mem2reg,default<O2>",
        target_machine,
        pb_options
    );

    if (error != null) {
        msg = LLVMGetErrorMessage(error);

        diagnostic_add_generic(DIAG_ERROR, "LLVM: %s", msg); 

        LLVMDisposeErrorMessage(msg);
        LLVMDisposePassBuilderOptions(pb_options);

        goto cleanup;
    }

    LLVMDisposePassBuilderOptions(pb_options);

    if (LLVMTargetMachineEmitToFile(
        target_machine,
        ctx -> module,
        ctx -> file -> object_path,
        LLVMObjectFile,
        &msg
    ) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: %s", msg);
        LLVMDisposeMessage(msg);
        goto cleanup;
    }

    LLVMDisposeTargetMachine(target_machine);

cleanup:
    LLVMDisposeBuilder(ctx -> builder);
    LLVMDisposeModule(ctx -> module);
    LLVMContextDispose(ctx -> ctx);
}

static bool codegen_ast(CodegenCtx* ctx) {
    Ast* ast = &ctx -> file -> ast;

    for (u32 i = 0; i < ast -> count; i++) {
        AstNode* node = &ast -> nodes[i];

        switch (node -> kind) {
            case AST_FUNCTION_DECL:
                if (!codegen_function_declaration(ctx, node)) return false;
                break;

            default:
                break;
        }
    }

    if (driver.flags & DRIVER_FLAGS_EMIT_LLVM_IR) {
        char path[64] = {0};

        snprintf(path, sizeof(path), "file_%u.ll", ctx -> file -> id);

        char* ir = LLVMPrintModuleToString(ctx -> module);

        FILE* file = fopen(path, "w+");
        if (!file) {
            diagnostic_add_generic(DIAG_ERROR, "Unable to dump LLVM IR to %s", path);
            return false;
        }

        fprintf(file, "%s", ir);

        fclose(file);
    }

    return true;
}

static LLVMValueRef codegen_function_signature(CodegenCtx* ctx, SymbolId id) {
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    LLVMTypeRef* param_types = null;

    bool is_variadic = symbol -> flags & AST_FLAGS_IS_VARIADIC;

    u32 n = symbol -> as.function_symbol.parameter_count;
    u32 param_count = is_variadic ? n - 1 : n;

    if (param_count != 0) {
        param_types = arena_alloc(&ctx -> arena, param_count * sizeof(LLVMTypeRef));
    }

    for (u32 i = 0; i < param_count; i++) {
        SymbolId param_id = symbol -> as.function_symbol.parameters[i];
        Symbol* param = SYMBOL_ID_LOOKUP_REF(param_id);

        param_types[i] = type_id_to_llvm(ctx, param -> as.parameter_symbol.type_id);
    }

    LLVMTypeRef ret_type = type_id_to_llvm(ctx, symbol -> as.function_symbol.return_type_id);
    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types, param_count, is_variadic);

    str8 name = STRING_ID_LOOKUP(symbol -> name_id).str;

    return LLVMGetOrInsertFunction(ctx -> module, name.ptr, name.len, fn_type); 
}

static LLVMValueRef codegen_function_declaration(CodegenCtx* ctx, AstNode* node) {
    SymbolId symbol_id = node -> resolved_symbol;
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    LLVMValueRef fn = codegen_function_signature(ctx, symbol_id);
    assert(fn != null);

    ctx -> symbol_values[symbol_id] = fn;
    ctx -> fn = fn;

    if (node -> flags & AST_FLAGS_IS_EXTERNAL || symbol -> flags & AST_FLAGS_IS_EXTERNAL) {
        return fn;
    }

    bool is_variadic = symbol -> flags & AST_FLAGS_IS_VARIADIC;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx -> ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx -> builder, entry);

    u32 n = symbol -> as.function_symbol.parameter_count;
    u32 param_count = is_variadic ? n - 1 : n;

    for (u32 i = 0; i < param_count; i++) {
        SymbolId param_id = symbol -> as.function_symbol.parameters[i];
        Symbol* param = SYMBOL_ID_LOOKUP_REF(param_id);

        LLVMTypeRef type = type_id_to_llvm(ctx, param -> as.parameter_symbol.type_id);
        assert(type != null);

        LLVMValueRef address = LLVMBuildAlloca(ctx -> builder, type, ""); 
        assert(address != null);

        LLVMValueRef parameter = LLVMGetParam(fn, i);
        assert(parameter != null);

        LLVMBuildStore(ctx -> builder, parameter, address);

        ctx -> symbol_values[param_id] = address;
    }

    if (is_variadic) {
        LLVMTypeRef type = type_id_to_llvm(ctx, driver.type_table.builtins.type_variadic);
        assert(type != null);

        LLVMValueRef ap = LLVMBuildAlloca(ctx -> builder, type, "ap");
        LLVMSetAlignment(ap, 16);

        u32 id = LLVMLookupIntrinsicID("llvm.va_start", 13);
        assert(id != 0);

        LLVMTypeRef va_start_overload_types[] = { LLVMPointerTypeInContext(ctx -> ctx, 0) };
        LLVMValueRef va_start_fn = LLVMGetIntrinsicDeclaration(ctx -> module, id, va_start_overload_types, 1);

        LLVMTypeRef param_types[] = { LLVMPointerTypeInContext(ctx -> ctx, 0) };
        LLVMTypeRef va_start_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx -> ctx), param_types, 1, 0);

        LLVMValueRef args[] = { ap };

        LLVMBuildCall2(ctx -> builder, va_start_type, va_start_fn, args, 1, "");

        ctx -> va_ctx = (VaListCtx) {
            .state = VA_LIST_END,
            .va_list_type = type,
            .ap = ap
        };
    } else {
        ctx -> va_ctx = (VaListCtx) {
            .state = VA_LIST_NONE,
            .va_list_type = null,
            .ap = null
        };
    }

    bool did_fn_return = false;

    if (!codegen_function_body(ctx, node, symbol, &did_fn_return)) {
        return null;
    }

    if (symbol -> name_id == string_lookup_cstr("main")) {
        if (!did_fn_return) {
            LLVMBuildRet(ctx -> builder, LLVMConstInt(LLVMInt32TypeInContext(ctx -> ctx), 0, 0));

            did_fn_return = true;
        }
    }

    if (!did_fn_return && is_type_void(symbol -> as.function_symbol.return_type_id)) {
        if (ctx -> va_ctx.state == VA_LIST_END) {
            codegen_va_end(ctx);
        }

        LLVMBuildRetVoid(ctx -> builder);
    }

    return fn;
}

static bool codegen_function_body(CodegenCtx* ctx, AstNode* node, Symbol* symbol, bool* did_fn_return) {
    UNUSED(symbol);

    AstNode* block = &ctx -> file -> ast.nodes[node -> as.function_decl.block];

    u32 stmt_count = block -> as.block.statements.count;

    for (u32 i = 0; i < stmt_count; i++) {
        AstNode* stmt = &ctx -> file -> ast.nodes[block -> as.block.statements.ids[i]];

        switch (stmt -> kind) {
            case AST_VARIABLE_DECL: {
                codegen_variable_declaration(ctx, stmt);
            } break;

            case AST_FOR_LOOP: {
                codegen_for_loop(ctx, stmt);
            } break;

            case AST_WHILE_LOOP: {
                codegen_while_loop(ctx, stmt);
            } break;

            case AST_RETURN_STMT: {
                codegen_return_statement(ctx, stmt);
                *did_fn_return = true;
            } break;

            case AST_UNARY_OP: 
            case AST_BINARY_OP: 
            case AST_FUNCTION_CALL: {
                codegen_expression(ctx, stmt -> id);
            } break;

            default: {
            } break;
        }
    }

    return true;
}

static bool codegen_block(CodegenCtx* ctx, AstNodeId id) {
    AstNode* node = &ctx -> file -> ast.nodes[id];

    u32 stmt_count = node -> as.block.statements.count;
    
    for (u32 i = 0; i < stmt_count; i++) {
        AstNode* stmt = &ctx -> file -> ast.nodes[node -> as.block.statements.ids[i]];

        switch (stmt -> kind) {
            case AST_VARIABLE_DECL: {
                codegen_variable_declaration(ctx, stmt);
            } break;

            case AST_FOR_LOOP: {
                codegen_for_loop(ctx, stmt);
            } break;

            case AST_WHILE_LOOP: {
                codegen_while_loop(ctx, stmt);
            } break;

            case AST_RETURN_STMT: {
                codegen_return_statement(ctx, stmt);
            } break;

            case AST_UNARY_OP: 
            case AST_BINARY_OP: 
            case AST_FUNCTION_CALL: {
                codegen_expression(ctx, stmt -> id);
            } break;

            default: {
            } break;
        }
    }

    return true;
}

static LLVMValueRef codegen_variable_declaration(CodegenCtx* ctx, AstNode* node) {
    LLVMTypeRef type = type_id_to_llvm(ctx, node -> resolved_type);
    LLVMValueRef address = LLVMBuildAlloca(ctx -> builder, type, "");
    assert(address != null);

    ctx -> symbol_values[node -> resolved_symbol] = address;

    if (node -> as.variable_decl.value_expr == AST_NODE_ID_NONE) {
        return address;
    }

    LLVMValueRef value = codegen_expression(ctx, node -> as.variable_decl.value_expr);
    LLVMBuildStore(ctx -> builder, value, address);

    return address;
}

static bool codegen_for_loop(CodegenCtx* ctx, AstNode* node) {
    AstNode* init = &ctx -> file -> ast.nodes[node -> as.for_loop.init];

    LLVMValueRef iterator = codegen_variable_declaration(ctx, init);
    UNUSED(iterator);

    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef step_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");

    // Condition
    LLVMBuildBr(ctx -> builder, cond_block);
    LLVMPositionBuilderAtEnd(ctx -> builder, cond_block);

    LLVMValueRef condition = codegen_expression(ctx, node -> as.for_loop.cond);
    LLVMBuildCondBr(ctx -> builder, condition, body_block, exit_block);

    // Body
    LLVMPositionBuilderAtEnd(ctx -> builder, body_block);
    codegen_block(ctx, node -> as.for_loop.block);
    LLVMBuildBr(ctx -> builder, step_block);

    // Step
    LLVMPositionBuilderAtEnd(ctx -> builder, step_block);
    LLVMValueRef step = codegen_expression(ctx, node -> as.for_loop.step);
    LLVMBuildBr(ctx -> builder, cond_block);

    UNUSED(step);

    // Exit
    LLVMPositionBuilderAtEnd(ctx -> builder, exit_block);

    return true;
}

static bool codegen_while_loop(CodegenCtx* ctx, AstNode* node) {
    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");

    // Condition
    LLVMBuildBr(ctx -> builder, cond_block);
    LLVMPositionBuilderAtEnd(ctx -> builder, cond_block);

    LLVMValueRef condition = codegen_expression(ctx, node -> as.while_loop.cond);
    LLVMBuildCondBr(ctx -> builder, condition, body_block, exit_block);

    // Body
    LLVMPositionBuilderAtEnd(ctx -> builder, body_block);
    codegen_block(ctx, node -> as.while_loop.block);
    LLVMBuildBr(ctx -> builder, cond_block);

    // Exit
    LLVMPositionBuilderAtEnd(ctx -> builder, exit_block);

    return true;
}

static bool codegen_return_statement(CodegenCtx* ctx, AstNode* node) {
    if (ctx -> va_ctx.state == VA_LIST_END) {
        codegen_va_end(ctx);
    }

    if (node -> resolved_type == driver.type_table.builtins.type_void) {
        LLVMBuildRetVoid(ctx -> builder);
        return true;
    }

    LLVMValueRef value = codegen_expression(ctx, node -> as.return_stmt.expr);
    LLVMBuildRet(ctx -> builder, value);
    return true;
}

static LLVMValueRef codegen_lvalue(CodegenCtx* ctx, AstNodeId id) {
    AstNode* node = &ctx -> file -> ast.nodes[id];
    
    switch (node -> kind) {
        case AST_IDENTIFIER: {
            SymbolId symbol_id = node -> resolved_symbol;
            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

            switch (symbol -> kind) {
                case SYMBOL_PARAMETER:
                case SYMBOL_VARIABLE: {
                    LLVMValueRef address = ctx -> symbol_values[symbol_id];
                    assert(address != null);

                    return address;
                } break;

                default:
                    UNREACHABLE("codegen_lvalue() | switch on symbol -> kind");
            }
        } break;

        case AST_UNARY_OP: {
            if (node -> as.unary_op.op == TOK_STAR) {
                return codegen_expression(ctx, node -> as.unary_op.operand);
            }
        } break;

        default:
            break;
    }

    printf("Found: %s\n", AST_NODE_KIND_STRINGS[node -> kind]);
    UNREACHABLE("codegen_lvalue()");
}

static LLVMValueRef codegen_expression(CodegenCtx* ctx, AstNodeId id) {
    AstNode* node = &ctx -> file -> ast.nodes[id];

    switch (node -> kind) {
        case AST_IDENTIFIER: {
            SymbolId symbol_id = node -> resolved_symbol;
            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

            switch (symbol -> kind) {
                case SYMBOL_VARIABLE: {
                    LLVMValueRef address = ctx -> symbol_values[symbol_id];
                    assert(address != null);

                    LLVMTypeRef type = type_id_to_llvm(ctx, symbol -> as.variable_symbol.type_id);

                    return LLVMBuildLoad2(ctx -> builder, type, address, "");
                } break;

                case SYMBOL_PARAMETER: {
                    if (symbol -> as.parameter_symbol.type_id == driver.type_table.builtins.type_va_list) {
                        return ctx -> va_ctx.ap;
                    }

                    LLVMValueRef address = ctx -> symbol_values[symbol_id];
                    assert(address != null);

                    LLVMTypeRef type = type_id_to_llvm(ctx, symbol -> as.parameter_symbol.type_id);

                    return LLVMBuildLoad2(ctx -> builder, type, address, "");
                } break;

                case SYMBOL_FUNCTION: {
                    if (ctx -> symbol_values[symbol_id] != null) {
                        return ctx -> symbol_values[symbol_id];
                    }

                    return codegen_function_signature(ctx, symbol_id);
                } break;

                default:
                    UNREACHABLE("codegen_expression() | switch on symbol -> kind");
            }
        } break;

        case AST_LITERAL: {
            switch (node -> as.literal.kind) {
                case LITERAL_INTEGER:
                    return LLVMConstInt(
                        type_id_to_llvm(ctx, node -> resolved_type),
                        node -> as.literal.as.integer,
                        0
                    );

                case LITERAL_BOOL:
                    return LLVMConstInt(
                        type_id_to_llvm(ctx, node -> resolved_type),
                        node -> as.literal.as.boolean,
                        0
                    );

                case LITERAL_CHAR:
                    return LLVMConstInt(
                        type_id_to_llvm(ctx, node -> resolved_type),
                        node -> as.literal.as.character,
                        0
                    ); 

                case LITERAL_FLOAT:
                    return LLVMConstReal(
                        type_id_to_llvm(ctx, node -> resolved_type),
                        node -> as.literal.as.floating
                    );

                case LITERAL_NULL:
                    return LLVMConstPointerNull(
                        type_id_to_llvm(ctx, node -> resolved_type)
                    );

                case LITERAL_STRING:
                    return get_or_insert_string(ctx, node -> as.literal.as.string);
            }
        }

        case AST_FUNCTION_CALL: {
            LLVMValueRef fn = codegen_expression(ctx, node -> as.function_call.identifier);

            if (fn == null) {
                fn = codegen_function_signature(ctx, node -> resolved_symbol);
            }

            assert(fn != null);

            LLVMTypeRef fn_type = LLVMGlobalGetValueType(fn);

            u32 arg_count = node -> as.function_call.arguments.count;

            LLVMValueRef* args = arg_count != 0 ? arena_alloc(&ctx -> arena, sizeof(LLVMValueRef) * arg_count) : null;

            for (u32 i = 0; i < arg_count; i++) {
                AstNodeId arg_id = node -> as.function_call.arguments.ids[i];

                args[i] = codegen_expression(ctx, arg_id);
            }

            return LLVMBuildCall2(ctx -> builder, fn_type, fn, args, arg_count, "");
        } break;

        case AST_UNARY_OP: {
            AstNodeId operand = node -> as.unary_op.operand;

            switch (node -> as.unary_op.op) {
                case TOK_AMP: {
                    return codegen_lvalue(ctx, operand);
                } break;

                case TOK_STAR: {
                    LLVMValueRef address = codegen_expression(ctx, operand);
                    LLVMTypeRef base_type = type_id_to_llvm(ctx, node -> resolved_type);

                    return LLVMBuildLoad2(ctx -> builder, base_type, address, "");
                } break;

                case TOK_MINUS: {
                    LLVMValueRef value = codegen_expression(ctx, operand);
                    
                    return LLVMBuildNeg(ctx -> builder, value, "");
                } break;

                case TOK_PLUS: {
                    return codegen_expression(ctx, operand);
                } break;

                case TOK_TILDE: {
                    LLVMValueRef value = codegen_expression(ctx, operand);

                    return LLVMBuildNot(ctx -> builder, value, "");
                } break;

                case TOK_BANG: {
                    LLVMValueRef value = codegen_expression(ctx, operand);
                    LLVMTypeRef type = LLVMTypeOf(value);

                    LLVMValueRef zero = LLVMConstInt(type, 0, 0);

                    return LLVMBuildICmp(ctx -> builder, LLVMIntEQ, value, zero, "");
                } break;

                default:
                    UNREACHABLE("codegen_expression() | unary_op")
            }
        } break;

        case AST_BINARY_OP: {
            AstNode* lhs_node = &ctx -> file -> ast.nodes[node -> as.binary_op.left];
            AstNode* rhs_node = &ctx -> file -> ast.nodes[node -> as.binary_op.right];

            TypeId lhs_type = lhs_node -> resolved_type;
            TypeId rhs_type = rhs_node -> resolved_type;

            TokenKind op = node -> as.binary_op.op;
            bool needs_lvalue = (op == TOK_EQ) || (is_compound_assignment_op(op));

            LLVMValueRef lhs;
            LLVMValueRef rhs;

            if (needs_lvalue) {
                lhs = codegen_lvalue(ctx, node -> as.binary_op.left);
                rhs = codegen_expression(ctx, node -> as.binary_op.right);
            } else {
                lhs = codegen_expression(ctx, node -> as.binary_op.left);
                rhs = codegen_expression(ctx, node -> as.binary_op.right);
            }

            if (op == TOK_EQ) {
                return LLVMBuildStore(ctx -> builder, rhs, lhs);
            }

            if (is_compound_assignment_op(op)) {
                LLVMTypeRef  type    = type_id_to_llvm(ctx, lhs_type);
                LLVMValueRef current = LLVMBuildLoad2(ctx -> builder, type, lhs, "");

                TokenKind base_op = get_compound_assignment_base_op(op);
                LLVMValueRef result = codegen_binary_arithmetic(ctx, base_op, current, rhs, lhs_type, rhs_type);

                return LLVMBuildStore(ctx -> builder, result, lhs);
            }

            switch (node -> as.binary_op.op) {
                case TOK_PLUS:
                case TOK_MINUS:
                case TOK_STAR:
                case TOK_SLASH:
                case TOK_PERCENT:
                case TOK_AMP:
                case TOK_PIPE:
                case TOK_CARET:
                case TOK_SHL:
                case TOK_SHR:
                    return codegen_binary_arithmetic(ctx, op, lhs, rhs, lhs_type, rhs_type);

                case TOK_EQ:
                    return LLVMBuildStore(ctx -> builder, rhs, lhs);

                case TOK_EQ_EQ:
                    if (is_type_int(lhs_type)) {
                        return LLVMBuildICmp(ctx -> builder, LLVMIntEQ, lhs, rhs, "");
                    } else {
                        return LLVMBuildFCmp(ctx -> builder, LLVMRealOEQ, lhs, rhs, "");
                    }

                case TOK_BANG_EQ:
                    if (is_type_int(lhs_type)) {
                        return LLVMBuildICmp(ctx -> builder, LLVMIntNE, lhs, rhs, "");
                    } else {
                        return LLVMBuildFCmp(ctx -> builder, LLVMRealONE, lhs, rhs, "");
                    }

                case TOK_LT:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntSLT, lhs, rhs, "");
                        } else {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntULT, lhs, rhs, "");
                        }
                    } else {
                        return LLVMBuildFCmp(ctx -> builder, LLVMRealOLT, lhs, rhs, "");
                    }

                case TOK_LT_EQ:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntSLE, lhs, rhs, "");
                        } else {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntULE, lhs, rhs, "");
                        }
                    } else {
                        return LLVMBuildFCmp(ctx -> builder, LLVMRealOLE, lhs, rhs, "");
                    }

                case TOK_GT:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntSGT, lhs, rhs, "");
                        } else {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntUGT, lhs, rhs, "");
                        }
                    } else {
                        return LLVMBuildFCmp(ctx -> builder, LLVMRealOGT, lhs, rhs, "");
                    }

                case TOK_GT_EQ:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntSGE, lhs, rhs, "");
                        } else {
                            return LLVMBuildICmp(ctx -> builder, LLVMIntUGE, lhs, rhs, "");
                        }
                    } else {
                        return LLVMBuildFCmp(ctx -> builder, LLVMRealOGE, lhs, rhs, "");
                    }

                default:
                    UNREACHABLE("binary_op");
            }

            // TODO: Logical, not sure about how this will link into blocks tho
        } break;

        default:
            UNREACHABLE("codegen_expression() not yet added")
    }
}

static LLVMValueRef codegen_binary_arithmetic(
    CodegenCtx* ctx,
    TokenKind op,
    LLVMValueRef lhs,
    LLVMValueRef rhs,
    TypeId lhs_type,
    TypeId rhs_type
) {
    switch (op) {
        case TOK_PLUS: {
            if (is_type(lhs_type, TYPE_POINTER) && !is_type(rhs_type, TYPE_POINTER)) {
                TypeEntry* entry = TYPE_ID_LOOKUP_REF(lhs_type);
                LLVMTypeRef elem_type = type_id_to_llvm(ctx, entry -> as.pointer_type.base);

                LLVMValueRef indices[1] = { rhs };
                return LLVMBuildGEP2(ctx -> builder, elem_type, lhs, indices, 1, "");
            }

            if (is_type(rhs_type, TYPE_POINTER) && !is_type(lhs_type, TYPE_POINTER)) {
                TypeEntry* entry = TYPE_ID_LOOKUP_REF(rhs_type);
                LLVMTypeRef elem_type = type_id_to_llvm(ctx, entry -> as.pointer_type.base);

                LLVMValueRef indices[1] = { lhs };
                return LLVMBuildGEP2(ctx -> builder, elem_type, rhs, indices, 1, "");
            }

            return LLVMBuildAdd(ctx -> builder, lhs, rhs, "add");
        }

        case TOK_MINUS: {
            if (is_type(lhs_type, TYPE_POINTER) && is_type(rhs_type, TYPE_POINTER)) {
                LLVMTypeRef isize_type = type_id_to_llvm(ctx, driver.type_table.builtins.type_isize);

                LLVMValueRef lhs_int = LLVMBuildPtrToInt(ctx -> builder, lhs, isize_type, "");
                LLVMValueRef rhs_int = LLVMBuildPtrToInt(ctx -> builder, rhs, isize_type, "");

                return LLVMBuildSub(ctx -> builder, lhs_int, rhs_int, "");
            }

            if (is_type(lhs_type, TYPE_POINTER) && !is_type(rhs_type, TYPE_POINTER)) {
                TypeEntry* entry = TYPE_ID_LOOKUP_REF(lhs_type);
                LLVMTypeRef elem_type = type_id_to_llvm(ctx, entry -> as.pointer_type.base);

                LLVMValueRef neg_rhs = LLVMBuildNeg(ctx -> builder, rhs, "");
                LLVMValueRef indices[1] = { neg_rhs };

                return LLVMBuildGEP2(ctx -> builder, elem_type, lhs, indices, 1, "");
            }

            return LLVMBuildSub(ctx -> builder, lhs, rhs, "sub");
        }

        case TOK_STAR:
            return LLVMBuildMul(ctx -> builder, lhs, rhs, "mul");

        case TOK_SLASH:
            if (is_type_float(lhs_type) || is_type_float(rhs_type)) {
                return LLVMBuildFDiv(ctx -> builder, lhs, rhs, "");
            } else if (is_type_signed_int(lhs_type) || is_type_signed_int(rhs_type)) {
                return LLVMBuildSDiv(ctx -> builder, lhs, rhs, "");
            } else {
                return LLVMBuildUDiv(ctx -> builder, lhs, rhs, "");
            }

        case TOK_PERCENT:
            if (is_type_float(lhs_type) || is_type_float(rhs_type)) {
                return LLVMBuildFRem(ctx -> builder, lhs, rhs, "");
            } else if (is_type_signed_int(lhs_type) || is_type_signed_int(rhs_type)) {
                return LLVMBuildSRem(ctx -> builder, lhs, rhs, "");
            } else {
                return LLVMBuildURem(ctx -> builder, lhs, rhs, "");
            }

        case TOK_AMP:
            return LLVMBuildAnd(ctx -> builder, lhs, rhs, "");

        case TOK_PIPE:
            return LLVMBuildOr(ctx -> builder, lhs, rhs, "");

        case TOK_CARET:
            return LLVMBuildXor(ctx -> builder, lhs, rhs, "");

        case TOK_SHL:
            return LLVMBuildShl(ctx -> builder, lhs, rhs, "");

        case TOK_SHR:
            if (is_type_signed_int(lhs_type)) {
                return LLVMBuildAShr(ctx -> builder, lhs, rhs, "");
            } else {
                return LLVMBuildLShr(ctx -> builder, lhs, rhs, "");
            }

        default:
            UNREACHABLE("codegen_binary_arith_op()");
    }
}

static bool codegen_va_end(CodegenCtx* ctx) {
    u32 id = LLVMLookupIntrinsicID("llvm.va_end", 11);
    assert(id != 0);

    LLVMTypeRef va_end_overload_types[1] = { LLVMPointerTypeInContext(ctx -> ctx, 0) };
    LLVMValueRef va_end_fn = LLVMGetIntrinsicDeclaration(ctx -> module, id, va_end_overload_types, 1);

    LLVMTypeRef param_types[] = { LLVMPointerTypeInContext(ctx -> ctx, 0) };
    LLVMTypeRef va_end_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx -> ctx), param_types, 1, 0);

    LLVMValueRef args[] = { ctx -> va_ctx.ap };

    LLVMBuildCall2(ctx -> builder, va_end_type, va_end_fn, args, 1, "");

    ctx -> va_ctx.state = VA_LIST_DONE;

    return true;
}


static LLVMTypeRef base_to_llvm(CodegenCtx* ctx, TypeId id, TypeEntry* entry) {
    TypeBuiltinIds ids = driver.type_table.builtins;
    LLVMContextRef c = ctx -> ctx;

    if (id == ids.type_void) {
        return LLVMVoidTypeInContext(c);
    }

    if (id == ids.type_u8 || id == ids.type_i8 || id == ids.type_bool || id == ids.type_char) {
        return LLVMInt8TypeInContext(c);
    }

    if (id == ids.type_u16 || id == ids.type_i16) {
        return LLVMInt16TypeInContext(c);
    }

    if (id == ids.type_u32 || id == ids.type_i32) {
        return LLVMInt32TypeInContext(c);
    }

    if (id == ids.type_u64 || id == ids.type_i64) {
        return LLVMInt64TypeInContext(c);
    }

    if (id == ids.type_usize || id == ids.type_isize) {
        return LLVMIntTypeInContext(c, entry -> size * 8);
    }

    if (id == ids.type_f32) {
        return LLVMFloatTypeInContext(c);
    }

    if (id == ids.type_f64) {
        return LLVMDoubleTypeInContext(c);
    }

    // TODO: make this compatible with other architectures
    // rn this only supports x86-64 linux
    if (id == ids.type_variadic) {
        LLVMTypeRef types[4] = {
            LLVMInt32TypeInContext(c),
            LLVMInt32TypeInContext(c),
            LLVMPointerTypeInContext(c, 0),
            LLVMPointerTypeInContext(c, 0)
        };

        return LLVMStructTypeInContext(c, types, 4, false);
    }

    if (id == ids.type_va_list) {
        return LLVMPointerTypeInContext(c, 0);
    }

    UNREACHABLE("base_to_llvm()");
}

static LLVMTypeRef struct_to_llvm(CodegenCtx* ctx, TypeEntry* entry) {
    u32 field_count = entry -> as.struct_type.field_count;

    LLVMTypeRef* field_types = arena_alloc(&ctx -> arena, field_count * sizeof(LLVMTypeRef));

    for (u32 i = 0; i < field_count; i++) {
        field_types[i] = type_id_to_llvm(ctx, entry -> as.struct_type.fields[i]);
    }

    return LLVMStructTypeInContext(ctx -> ctx, field_types, field_count, false);
}

static LLVMTypeRef type_id_to_llvm(CodegenCtx* ctx, TypeId id) {
    TypeEntry* entry = TYPE_ID_LOOKUP_REF(id);

    switch (entry -> kind) {
        case TYPE_BASE:
            return base_to_llvm(ctx, id, entry);

        case TYPE_POINTER:
            // return LLVMPointerTypeInContext(ctx -> ctx, 0);
            return LLVMPointerType(type_id_to_llvm(ctx, entry -> as.pointer_type.base), 0);

        case TYPE_STRUCT:
            return struct_to_llvm(ctx, entry);

        case TYPE_ENUM:
            return type_id_to_llvm(ctx, entry -> as.enum_type.underlying_type);

        default:
            UNREACHABLE("type_id_to_llvm()");
    }
}

static LLVMValueRef get_or_insert_string(CodegenCtx* ctx, StringId id) {
    if (ctx -> string_values[id] != null) {
        return ctx -> string_values[id];
    }

    StringEntry entry = STRING_ID_LOOKUP(id);
    str8 str = entry.str;

    // Make an owning copy for the thread (when we get to multithreaded)
    char* copy = arena_alloc(&ctx -> arena, str.len + 1);
    memcpy(copy, str.ptr, str.len);
    copy[str.len] = 0;

    char name[64] = {0};

    snprintf(name, sizeof(name), "str_%u", id);

    LLVMValueRef value = LLVMBuildGlobalString(ctx -> builder, copy, name);

    ctx -> string_values[id] = value;

    return value;
}

static bool is_compound_assignment_op(TokenKind op) {
    switch (op) {
        case TOK_PLUS_EQ:
        case TOK_MINUS_EQ:
        case TOK_STAR_EQ:
        case TOK_SLASH_EQ:
        case TOK_PERCENT_EQ:
        case TOK_AMP_EQ:
        case TOK_PIPE_EQ:
        case TOK_CARET_EQ:
        case TOK_SHL_EQ:
        case TOK_SHR_EQ:
            return true;
        
        default:
            return false;
    }
}

static TokenKind get_compound_assignment_base_op(TokenKind op) {
    switch (op) {
        case TOK_PLUS_EQ:    return TOK_PLUS;
        case TOK_MINUS_EQ:   return TOK_MINUS;
        case TOK_STAR_EQ:    return TOK_STAR;
        case TOK_SLASH_EQ:   return TOK_SLASH;
        case TOK_PERCENT_EQ: return TOK_PERCENT;
        case TOK_AMP_EQ:     return TOK_AMP;
        case TOK_PIPE_EQ:    return TOK_PIPE;
        case TOK_CARET_EQ:   return TOK_CARET;
        case TOK_SHL_EQ:     return TOK_SHL;
        case TOK_SHR_EQ:     return TOK_SHR;

        default:
            UNREACHABLE("get_compound_assignment_base_op()");
    }
}
