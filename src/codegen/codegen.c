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

static LLVMValueRef codegen_global_variable(CodegenCtx* ctx, AstNode* node);

static LLVMValueRef codegen_function_signature(CodegenCtx* ctx, SymbolId id);
static LLVMValueRef codegen_function_declaration(CodegenCtx* ctx, AstNode* node);

static CodegenResult codegen_block(CodegenCtx* ctx, AstNodeId id);
static CodegenResult codegen_statement(CodegenCtx* ctx, AstNode* node);

static LLVMValueRef  codegen_variable_declaration(CodegenCtx* ctx, AstNode* node);

static CodegenResult codegen_for_loop(CodegenCtx* ctx, AstNode* node);
static CodegenResult codegen_while_loop(CodegenCtx* ctx, AstNode* node);
static CodegenResult codegen_return_statement(CodegenCtx* ctx, AstNode* node);

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

static bool codegen_va_end(CodegenCtx* ctx);

static LLVMTypeRef type_id_to_llvm(CodegenCtx* ctx, TypeId id);

static LLVMValueRef get_or_insert_string(CodegenCtx* ctx, StringId id);

static bool      is_compound_assignment_op(TokenKind op);
static TokenKind get_compound_assignment_base_op(TokenKind op);

static void defer_stack_enter(CodegenCtx* ctx);
static void defer_stack_exit(CodegenCtx* ctx);
static void defer_stack_append(CodegenCtx* ctx, AstNodeId id);
static bool codegen_all_defers(CodegenCtx* ctx);
static bool codegen_defers_until(CodegenCtx* ctx, DeferStack* boundary);
static bool codegen_block_defers(CodegenCtx* ctx);


void codegen() {
    CodegenCtx ctx = {0};

    u32 symbol_count = driver.symbol_table.symbol_count;
    u32 string_count = driver.string_interner.count;

    u32 symbol_size = symbol_count * sizeof(LLVMValueRef);
    u32 string_size = string_count * sizeof(LLVMValueRef);

    arena_init(&ctx.map_arena, symbol_size + string_size, ALIGN_DEFAULT);
    arena_init(&ctx.scratch, ARENA_KB(2), ALIGN_DEFAULT);
    arena_init(&ctx.defer_list.arena, ARENA_KB(1), ALIGN_DEFAULT);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    ctx.symbol_values = arena_calloc(&ctx.map_arena, symbol_size);
    ctx.string_values = arena_calloc(&ctx.map_arena, string_size);

    u32 file_count = driver.file_interner.count;

    for (u32 i = 0; i < file_count; i++) {
        codegen_file(&ctx, i);

        arena_reset(&ctx.scratch);

        // reset to get rid of dangling pointers
        arena_memset(ctx.symbol_values, 0, symbol_size);
        arena_memset(ctx.string_values, 0, string_size);
    }

    arena_destroy(&ctx.scratch);
    arena_destroy(&ctx.map_arena);
    arena_destroy(&ctx.defer_list.arena);
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
        diagnostic_add_generic(DIAG_ERROR, "Failed to create LLVM IR");
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
        LLVMDisposeTargetMachine(target_machine);
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

        if (!(node -> flags & AST_FLAGS_IS_TOP_DECL)) {
            continue;
        } 

        switch (node -> kind) {
            case AST_FUNCTION_DECL:
                if (!codegen_function_declaration(ctx, node)) return false;
                break;

            case AST_VARIABLE_DECL:
                if (!codegen_global_variable(ctx, node)) return false;
                break;

            default:
                break;
        }

        arena_reset(&ctx -> defer_list.arena);
    }

    if (driver.flags & DRIVER_FLAGS_EMIT_LLVM_IR) {
        char path[64] = {0};

        snprintf(path, sizeof(path), "file_%u.ll", ctx -> file -> id);

        char* ir = LLVMPrintModuleToString(ctx -> module);

        FILE* file = fopen(path, "w+");
        if (!file) {
            diagnostic_add_generic(DIAG_ERROR, "LLVM: Unable to dump LLVM IR to %s", path);
            return false;
        }

        fprintf(stdout, "Emitted LLVM IR to %s (%.*s)\n", path, STR8_FMT(ctx -> file -> path));
        fprintf(file, "%s", ir);

        fclose(file);
    }

    return true;
}

static LLVMValueRef codegen_global_variable(CodegenCtx* ctx, AstNode* node) {
    LLVMTypeRef type = type_id_to_llvm(ctx, node -> resolved_type);
    LLVMValueRef var = LLVMAddGlobal(ctx -> module, type, "");

    // TODO: ADD COMPILE TIME INTERPRETER FOR TO ENABLE FUNCTION CALLS AS VALUES FOR GLOBALS
    if (node -> as.variable_decl.value_expr != AST_NODE_ID_NONE) {
        LLVMSetInitializer(var, codegen_expression(ctx, node -> as.variable_decl.value_expr));
    }

    LLVMSetGlobalConstant(var, node -> flags & AST_FLAGS_IS_CONSTANT);

    if (node -> flags & AST_FLAGS_IS_EXTERNAL) {
        LLVMSetLinkage(var, LLVMExternalLinkage);
    }

    ctx -> symbol_values[node -> resolved_symbol] = var;

    return var;
}

static LLVMValueRef codegen_function_signature(CodegenCtx* ctx, SymbolId id) {
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    LLVMTypeRef* param_types = null;

    bool is_variadic = symbol -> flags & AST_FLAGS_IS_VARIADIC;

    u32 n = symbol -> as.function_symbol.parameter_count;
    u32 param_count = is_variadic ? n - 1 : n;

    if (param_count != 0) {
        param_types = arena_alloc(&ctx -> scratch, param_count * sizeof(LLVMTypeRef));
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

    ctx -> loop_ctx = null;
    ctx -> defer_list.stack = null;

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

    CodegenResult result = codegen_block(ctx, node -> as.function_decl.block);

    if (result == CODEGEN_ERROR) {
        return null;
    }

    if (result == CODEGEN_FALLTHROUGH) {
        bool is_main = symbol -> name_id == string_lookup_cstr("main");
        bool is_void = is_type_void(symbol -> as.function_symbol.return_type_id);

        if (is_main) {
            if (ctx -> va_ctx.state == VA_LIST_END && !codegen_va_end(ctx)) {
                return null;
            }

            LLVMBuildRet(ctx -> builder, LLVMConstInt(LLVMInt32TypeInContext(ctx -> ctx), 0, 0));
        } else if (is_void) {
            if (ctx -> va_ctx.state == VA_LIST_END && !codegen_va_end(ctx)) {
                return null;
            }

            LLVMBuildRetVoid(ctx -> builder);
        } else {
            return null;
        }
    }

    return fn;
}

static CodegenResult codegen_block(CodegenCtx* ctx, AstNodeId id) {
    AstNode* node = &ctx -> file -> ast.nodes[id];

    defer_stack_enter(ctx);

    CodegenResult result = CODEGEN_FALLTHROUGH;

    u32 stmt_count = node -> as.block.statements.count;

    for (u32 i = 0; i < stmt_count; i++) {
        AstNode* stmt = &ctx -> file -> ast.nodes[node -> as.block.statements.ids[i]];

        result = codegen_statement(ctx, stmt);

        arena_reset(&ctx -> scratch);

        if (result != CODEGEN_FALLTHROUGH) {
            break;
        }
    }

    if (result == CODEGEN_FALLTHROUGH) {
        if (!codegen_block_defers(ctx)) {
            result = CODEGEN_ERROR;
        }
    }

    defer_stack_exit(ctx);
    return result;
}

static CodegenResult codegen_statement(CodegenCtx* ctx, AstNode* node) {
    switch (node -> kind) {
        case AST_BLOCK:
            return codegen_block(ctx, node -> id);

        case AST_VARIABLE_DECL:
            return codegen_variable_declaration(ctx, node) != null ? CODEGEN_FALLTHROUGH : CODEGEN_ERROR;

        case AST_FOR_LOOP:
            return codegen_for_loop(ctx, node);

        case AST_WHILE_LOOP:
            return codegen_while_loop(ctx, node);

        case AST_DEFER_STMT: {
            defer_stack_append(ctx, node -> as.defer_stmt.stmt);
            return CODEGEN_FALLTHROUGH;
        }

        case AST_RETURN_STMT:
            return codegen_return_statement(ctx, node);

        case AST_BREAK_STMT: {
            if (!codegen_defers_until(ctx, ctx -> loop_ctx -> defer_boundary)) {
                return CODEGEN_ERROR;
            }

            LLVMBuildBr(ctx -> builder, ctx -> loop_ctx -> break_target);

            return CODEGEN_TERMINATED;
        }

        case AST_CONTINUE_STMT: {
            if (!codegen_defers_until(ctx, ctx -> loop_ctx -> defer_boundary)) {
                return CODEGEN_ERROR;
            }

            LLVMBuildBr(ctx -> builder, ctx -> loop_ctx -> continue_target);

            return CODEGEN_TERMINATED;
        }

        case AST_UNARY_OP:
        case AST_BINARY_OP:
        case AST_FUNCTION_CALL:
            return codegen_expression(ctx, node -> id) != null ? CODEGEN_FALLTHROUGH : CODEGEN_ERROR;

        default:
            return CODEGEN_ERROR;
    }
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

    if (value == null) {
        return null;
    }

    LLVMBuildStore(ctx -> builder, value, address);

    return address;
}

static CodegenResult codegen_for_loop(CodegenCtx* ctx, AstNode* node) {
    AstNode* init = &ctx -> file -> ast.nodes[node -> as.for_loop.init];

    if (codegen_variable_declaration(ctx, init) == null) {
        return CODEGEN_ERROR;
    }

    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef step_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");

    // Condition
    LLVMBuildBr(ctx -> builder, cond_block);
    LLVMPositionBuilderAtEnd(ctx -> builder, cond_block);

    LLVMValueRef condition = codegen_expression(ctx, node -> as.for_loop.cond);

    if (condition == null) {
        return CODEGEN_ERROR;
    }

    LLVMBuildCondBr(ctx -> builder, condition, body_block, exit_block);

    // Body
    LLVMPositionBuilderAtEnd(ctx -> builder, body_block);

    LoopCtx loop_ctx = {
        .break_target = exit_block,
        .continue_target = step_block,
        .defer_boundary = ctx -> defer_list.stack,
        .previous = ctx -> loop_ctx
    };

    ctx -> loop_ctx = &loop_ctx;
    
    CodegenResult result = codegen_block(ctx, node -> as.for_loop.block);
    
    ctx -> loop_ctx = loop_ctx.previous;

    if (result == CODEGEN_ERROR) {
        return CODEGEN_ERROR;
    }

    if (result == CODEGEN_FALLTHROUGH) {
        LLVMBuildBr(ctx -> builder, step_block);
    }

    // Step
    LLVMPositionBuilderAtEnd(ctx -> builder, step_block);
    LLVMValueRef step = codegen_expression(ctx, node -> as.for_loop.step);

    if (step == null) {
        return CODEGEN_ERROR;
    }

    LLVMBuildBr(ctx -> builder, cond_block);

    // Exit
    LLVMPositionBuilderAtEnd(ctx -> builder, exit_block);

    return CODEGEN_FALLTHROUGH;
}

static CodegenResult codegen_while_loop(CodegenCtx* ctx, AstNode* node) {
    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");

    // Condition
    LLVMBuildBr(ctx -> builder, cond_block);
    LLVMPositionBuilderAtEnd(ctx -> builder, cond_block);

    LLVMValueRef condition = codegen_expression(ctx, node -> as.while_loop.cond);

    if (condition == null) {
        return CODEGEN_ERROR;
    }

    LLVMBuildCondBr(ctx -> builder, condition, body_block, exit_block);

    // Body
    LLVMPositionBuilderAtEnd(ctx -> builder, body_block);

    LoopCtx loop_ctx = {
        .break_target = exit_block,
        .continue_target = cond_block,
        .defer_boundary = ctx -> defer_list.stack,
        .previous = ctx -> loop_ctx
    };

    ctx -> loop_ctx = &loop_ctx;
    
    CodegenResult result = codegen_block(ctx, node -> as.while_loop.block);
    
    ctx -> loop_ctx = loop_ctx.previous;

    if (result == CODEGEN_ERROR) {
        return CODEGEN_ERROR;
    }

    if (result == CODEGEN_FALLTHROUGH) {
        LLVMBuildBr(ctx -> builder, cond_block);
    }

    // Exit
    LLVMPositionBuilderAtEnd(ctx -> builder, exit_block);

    return CODEGEN_FALLTHROUGH;
}

static CodegenResult codegen_return_statement(CodegenCtx* ctx, AstNode* node) {
    LLVMValueRef value = null;

    if (node -> resolved_type != driver.type_table.builtins.type_void) {
        value = codegen_expression(ctx, node -> as.return_stmt.expr);

        if (value == null) {
            return CODEGEN_ERROR;
        }
    }

    if (!codegen_all_defers(ctx)) {
        return CODEGEN_ERROR;
    }

    if (ctx -> va_ctx.state == VA_LIST_END) {
        if (!codegen_va_end(ctx)) {
            return CODEGEN_ERROR;
        }
    }

    if (node -> resolved_type == driver.type_table.builtins.type_void) {
        LLVMBuildRetVoid(ctx -> builder);
    } else {
        LLVMBuildRet(ctx -> builder, value);
    }

    return CODEGEN_TERMINATED;
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

            LLVMValueRef* args = arg_count != 0 ? arena_alloc(&ctx -> scratch, sizeof(LLVMValueRef) * arg_count) : null;

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

            return LLVMBuildAdd(ctx -> builder, lhs, rhs, "");
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

            return LLVMBuildSub(ctx -> builder, lhs, rhs, "");
        }

        case TOK_STAR:
            return LLVMBuildMul(ctx -> builder, lhs, rhs, "");

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

    LLVMTypeRef* field_types = arena_alloc(&ctx -> scratch, field_count * sizeof(LLVMTypeRef));

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
    char* copy = arena_alloc(&ctx -> scratch, str.len + 1);
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

static void defer_stack_enter(CodegenCtx* ctx) {
    DeferStack* current_stack = ctx -> defer_list.stack;
    DeferStack* new_stack = arena_alloc(&ctx -> defer_list.arena, sizeof(DeferStack));

    new_stack -> previous = current_stack;
    new_stack -> ids = arena_alloc(&ctx -> defer_list.arena, sizeof(AstNodeId) * defer_stack_init_cap);
    new_stack -> count = 0;
    new_stack -> capacity = defer_stack_init_cap;

    ctx -> defer_list.stack = new_stack;
}

static void defer_stack_exit(CodegenCtx* ctx) {
    DeferStack* stack = ctx -> defer_list.stack;

    assert(stack != null);

    ctx -> defer_list.stack = stack -> previous;
}

static void defer_stack_append(CodegenCtx* ctx, AstNodeId id) {
    DeferStack* stack = ctx -> defer_list.stack;

    assert(stack != null);

    if (UNLIKELY(stack -> count >= stack -> capacity)) {
        u64 old_size = stack -> capacity * sizeof(AstNodeId);
        u64 new_size = old_size * 2;

        stack -> ids = arena_realloc(&ctx -> defer_list.arena, stack -> ids, old_size, new_size);
        stack -> capacity *= 2;
    }

    stack -> ids[stack -> count++] = id;
}

static bool codegen_all_defers(CodegenCtx* ctx) {
    return codegen_defers_until(ctx, null);
}

static bool codegen_defers_until(CodegenCtx* ctx, DeferStack* boundary) {
    for (DeferStack* stack = ctx -> defer_list.stack; stack != boundary; stack = stack -> previous) {
        for (u32 i = stack -> count; i > 0; i--) {
            AstNodeId id = stack -> ids[i - 1];
            AstNode* node = &ctx -> file -> ast.nodes[id];

            CodegenResult result = codegen_statement(ctx, node);

            if (result == CODEGEN_ERROR) {
                return false;
            }

            if (result == CODEGEN_TERMINATED) {
                diagnostic_add_token_span(
                    ctx -> file -> id,
                    DIAG_ERROR,
                    node -> tokens,
                    "control flow statement is not allowed in defers",
                    null
                );

                return false;
            }
        }
    }

    return true;
}

static bool codegen_block_defers(CodegenCtx* ctx) {
    DeferStack* stack = ctx -> defer_list.stack;

    assert(stack != null);

    for (u32 i = stack -> count; i > 0; i--) {
        AstNodeId id = stack -> ids[i - 1];
        AstNode* node = &ctx -> file -> ast.nodes[id];

        CodegenResult result = codegen_statement(ctx, node);

        if (result == CODEGEN_ERROR) {
            return false;
        }

        if (result == CODEGEN_TERMINATED) {
            diagnostic_add_token_span(
                ctx -> file -> id,
                DIAG_ERROR,
                node -> tokens,
                "control flow statement is not allowed in defers",
                null
            );

            return false;
        }
    }

    return true;
}
