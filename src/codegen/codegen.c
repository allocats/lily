#include "ast/nodes/types.h"
#include "codegen/codegen.h"
#include "codegen/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "string_interner/types.h"
#include "symbols/resolve/resolve.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "token/types.h"
#include "types/builtins/types.h"
#include "types/entries/entries.h"
#include "types/entries/types.h"
#include "types/table/table.h"
#include "utils/macros.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

extern DriverCtx driver;

#define DUMP_IR(module)                             \
    do {                                            \
        char *ir = LLVMPrintModuleToString(module); \
        puts(ir);                                   \
        LLVMDisposeMessage(ir);                     \
    } while(0)

static void codegen_file(CodegenCtx* cg, FileId id);
static bool codegen_ast(CodegenCtx* cg);

static LLVMValueRef codegen_function_signature_from_call(CodegenCtx* cg, AstNode* node);
static LLVMValueRef codegen_function_signature_from_symbol(CodegenCtx* cg, Symbol* symbol);

static bool codegen_function_decl(CodegenCtx* cg, AstNode* node);

static bool codegen_block(CodegenCtx* cg, AstNodeId block_id);

static bool codegen_statement(CodegenCtx* cg, AstNodeId stmt_id, bool* terminates);
static bool codegen_variable_declaration(CodegenCtx* cg, AstNode* stmt);
static bool codegen_return(CodegenCtx* cg, AstNode* stmt);

static LLVMValueRef codegen_expression(CodegenCtx* cg, AstNodeId id);

// Helpers
static bool codegen_va_start(CodegenCtx* cg, VaListCtx* va_ctx);
static bool codegen_va_end(CodegenCtx* cg, VaListCtx* va_ctx);

static LLVMValueRef get_or_insert_string(CodegenCtx* cg, StringId id);

// Types
static LLVMTypeRef type_to_llvm(CodegenCtx* cg, TypeId id);

void codegen() {
    CodegenCtx cg = {0};

    arena_init(&cg.arena, ARENA_KB(4), ALIGN_DEFAULT);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    u32 symbol_count = driver.symbol_table.symbol_count;
    cg.symbol_values = calloc(symbol_count, sizeof(LLVMValueRef));

    u32 string_count = driver.string_interner.count;
    cg.string_values = calloc(string_count, sizeof(LLVMValueRef));

    u32 file_count = driver.file_interner.count;

    for (u32 i = 0; i < file_count; i++) {
        codegen_file(&cg, i);

        arena_reset(&cg.arena);
        memset(cg.symbol_values, 0, symbol_count * sizeof(LLVMValueRef));
        memset(cg.string_values, 0, string_count * sizeof(LLVMValueRef));
    }

    free(cg.symbol_values);
    arena_destroy(&cg.arena);
}

static void codegen_file(CodegenCtx* cg, FileId id) {
    cg -> file = file_lookup_id(id);

    cg -> ctx     = LLVMContextCreate();
    cg -> module  = LLVMModuleCreateWithNameInContext(cg -> file -> path.ptr, cg -> ctx);
    cg -> builder = LLVMCreateBuilderInContext(cg -> ctx);

    // dispatch on the AstNode
    if (!codegen_ast(cg)) {
        goto cleanup;
    }

    // DUMP_IR(cg -> module);

    char* msg = null;

    if (LLVMVerifyModule(cg -> module, LLVMReturnStatusAction, &msg) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: module verification failed: %s", msg);
        LLVMDisposeMessage(msg);
        goto cleanup;
    }

    char* host_triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(cg -> module, host_triple);

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

    if (LLVMTargetMachineEmitToFile(target_machine, cg -> module, cg -> file -> object_path, LLVMObjectFile, &msg) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: %s", msg);
        LLVMDisposeMessage(msg);
        goto cleanup;
    }

    LLVMDisposeTargetMachine(target_machine);

cleanup:
    LLVMDisposeBuilder(cg -> builder);
    LLVMDisposeModule(cg -> module);
    LLVMContextDispose(cg -> ctx);
}

static bool codegen_ast(CodegenCtx* cg) {
    u32 node_count = cg -> file -> ast.count;

    for (u32 i = 0; i < node_count; i++) {
        AstNode* node = &cg -> file -> ast.nodes[i];

        bool result = true;

        switch (node -> kind) {
            case AST_FUNCTION_DECL:
                result = codegen_function_decl(cg, node);
                break;

            default:
                break;
        }

        if (!result) {
            return false;
        }
    }

    return true;
}

static LLVMValueRef codegen_function_signature_from_symbol(CodegenCtx* cg, Symbol* symbol) {
    assert(symbol -> kind == SYMBOL_FUNCTION);

    LLVMTypeRef* param_types = null;

    u32 n = symbol -> as.function_symbol.parameter_count;
    u32 param_count = symbol -> flags & AST_FLAGS_IS_VARIADIC ? n - 1 : n;

    if (param_count != 0) {
        param_types = arena_alloc(&cg -> arena, param_count * sizeof(LLVMTypeRef));
    }

    for (u32 i = 0; i < param_count; i++) {
        SymbolId param_symbol_id = symbol -> as.function_symbol.parameters[i];
        Symbol* param_symbol = SYMBOL_ID_LOOKUP_REF(param_symbol_id);

        param_types[i] = type_to_llvm(cg, param_symbol -> as.parameter_symbol.type_id);
    }

    LLVMTypeRef ret_type = type_to_llvm(cg, symbol -> as.function_symbol.return_type_id);

    str8 name = STRING_ID_LOOKUP(symbol -> name_id).str;

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types, param_count, symbol -> flags & AST_FLAGS_IS_VARIADIC);

    return LLVMGetOrInsertFunction(cg -> module, name.ptr, name.len, fn_type);
}

static LLVMValueRef codegen_function_signature_from_call(CodegenCtx* cg, AstNode* node) {
    assert(node -> kind == AST_FUNCTION_CALL);

    LLVMTypeRef* param_types = null;

    u32 n = node -> as.function_call.arguments.count;
    u32 param_count = node -> flags & AST_FLAGS_IS_VARIADIC ? n - 1 : n;

    if (param_count != 0) {
        param_types = arena_alloc(&cg -> arena, param_count * sizeof(LLVMTypeRef));
    }

    for (u32 i = 0; i < param_count; i++) {
        AstNodeId param_id  = node -> as.function_call.arguments.ids[i];
        AstNode* param_node = &cg -> file -> ast.nodes[param_id];

        param_types[i] = type_to_llvm(cg, param_node -> resolved_type);
    }

    LLVMTypeRef ret_type = type_to_llvm(cg, node -> resolved_type);

    SymbolId symbol_id = resolve_name_expr(cg -> file, node -> as.function_call.identifier);
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    StringEntry name_entry = STRING_ID_LOOKUP(symbol -> name_id);
    str8 name = name_entry.str;

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types, param_count, node -> flags & AST_FLAGS_IS_VARIADIC);
    LLVMValueRef fn = LLVMGetOrInsertFunction(cg -> module, name.ptr, name.len, fn_type);

    cg -> symbol_values[symbol_id] = fn;

    return fn;
}

static bool codegen_function_decl(CodegenCtx* cg, AstNode* node) {
    Ast* ast = &cg -> file -> ast;

    LLVMTypeRef* param_types = null;

    u32 n = node -> as.function_decl.parameters.count;
    u32 param_count = node -> flags & AST_FLAGS_IS_VARIADIC ? n - 1 : n;

    if (param_count != 0) {
        param_types = arena_alloc(&cg -> arena, param_count * sizeof(LLVMTypeRef));
    }

    for (u32 i = 0; i < param_count; i++) {
        AstNodeId param_id  = node -> as.function_decl.parameters.ids[i];
        AstNode* param_node = &ast -> nodes[param_id];

        param_types[i] = type_to_llvm(cg, param_node -> resolved_type);
    }

    LLVMTypeRef ret_type = type_to_llvm(cg, node -> resolved_type);

    str8 name = STRING_ID_LOOKUP(node -> as.function_decl.name).str;

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types, param_count, node -> flags & AST_FLAGS_IS_VARIADIC);
    LLVMValueRef fn = LLVMGetOrInsertFunction(cg -> module, name.ptr, name.len, fn_type);

    cg -> symbol_values[node -> as.function_decl.symbol_id] = fn;

    if (node -> flags & AST_FLAGS_IS_EXTERNAL) {
        return true;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(cg -> ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(cg -> builder, entry);

    bool is_variadic = node -> flags & AST_FLAGS_IS_VARIADIC;

    VaListCtx va_ctx = {
        .state = is_variadic ? VA_LIST_START : VA_LIST_NONE,
        .ap = null
    };

    cg -> va_ctx = &va_ctx;

    if (!codegen_block(cg, node -> as.function_decl.block)) {
        return false;
    }

    return true;
}

static bool codegen_block(CodegenCtx* cg, AstNodeId block_id) {
    AstNode* block = &cg -> file -> ast.nodes[block_id];

    if (cg -> va_ctx -> state == VA_LIST_START) {
        codegen_va_start(cg, cg -> va_ctx);
    }

    u32 stmt_count = block -> as.block.statements.count;

    bool terminates = false;

    for (u32 i = 0; i < stmt_count; i++) {
        AstNodeId id = block -> as.block.statements.ids[i];

        if (!codegen_statement(cg, id, &terminates)) {
            return false;
        }
    }

    if (cg -> va_ctx -> state == VA_LIST_END) {
        codegen_va_end(cg, cg -> va_ctx);
    }

    if (!terminates) {
        LLVMBuildRetVoid(cg -> builder);
    }

    return true;
}

static bool codegen_statement(CodegenCtx* cg, AstNodeId stmt_id, bool* terminates) {
    AstNode* stmt = &cg -> file -> ast.nodes[stmt_id];

    switch (stmt -> kind) {
        case AST_VARIABLE_DECL:
            codegen_variable_declaration(cg, stmt);
            break;

        case AST_RETURN_STMT:
            codegen_return(cg, stmt);
            *terminates = true;
            break;

        default:
            codegen_expression(cg, stmt_id);
            break;
    }

    return true;
}

static bool codegen_variable_declaration(CodegenCtx* cg, AstNode* stmt) {
    StringEntry entry = STRING_ID_LOOKUP(stmt -> as.variable_decl.name);
    str8 str = entry.str;

    char putback = *(str.ptr + str.len);

    *(str.ptr + str.len) = 0;

    LLVMTypeRef type = type_to_llvm(cg, stmt -> resolved_type);
    LLVMValueRef var = LLVMBuildAlloca(cg -> builder, type, str.ptr);

    *(str.ptr + str.len) = putback;

    cg -> symbol_values[stmt -> as.variable_decl.symbol] = var;

    if (stmt -> as.variable_decl.value_expr == AST_NODE_ID_NONE) {
        return true;
    }

    LLVMValueRef value = codegen_expression(cg, stmt -> as.variable_decl.value_expr);

    LLVMBuildStore(cg -> builder, value, var);

    return true;
}

static bool codegen_return(CodegenCtx* cg, AstNode* stmt) {
    if (cg -> va_ctx -> state == VA_LIST_END) {
        codegen_va_end(cg, cg -> va_ctx);
    }

    if (stmt -> resolved_type == driver.type_table.builtins.type_void) {
        LLVMBuildRetVoid(cg -> builder);
    } else {
        LLVMValueRef value = codegen_expression(cg, stmt -> as.return_stmt.expr);
        LLVMValueRef ret = LLVMBuildLoad2(cg -> builder, type_to_llvm(cg, stmt -> resolved_type), value, "result");

        LLVMBuildRet(cg -> builder, ret);
    }

    return true;
}

static bool codegen_va_start(CodegenCtx* cg, VaListCtx* va_ctx) {
    LLVMTypeRef va_list_type = type_to_llvm(cg, driver.type_table.builtins.type_va_list);

    va_ctx -> ap = LLVMBuildAlloca(cg -> builder, va_list_type, "ap");

    u32 id = LLVMLookupIntrinsicID("llvm.va_start", 13);

    if (id == 0) {
        return false;
    }

    LLVMTypeRef va_start_overload_types[1] = { LLVMPointerTypeInContext(cg -> ctx, 0) };
    LLVMValueRef va_start_fn = LLVMGetIntrinsicDeclaration(cg -> module, id, va_start_overload_types, 1);

    LLVMTypeRef param_types[] = { LLVMPointerTypeInContext(cg -> ctx, 0) };
    LLVMTypeRef va_start_type = LLVMFunctionType(LLVMVoidTypeInContext(cg -> ctx), param_types, 1, 0);

    LLVMValueRef args[] = { va_ctx -> ap };

    LLVMBuildCall2(cg -> builder, va_start_type, va_start_fn, args, 1, "");

    va_ctx -> state = VA_LIST_END;

    return true;
}

static bool codegen_va_end(CodegenCtx* cg, VaListCtx* va_ctx) {
    u32 id = LLVMLookupIntrinsicID("llvm.va_end", 11);

    if (id == 0) {
        return false;
    }

    LLVMTypeRef va_end_overload_types[1] = { LLVMPointerTypeInContext(cg -> ctx, 0) };
    LLVMValueRef va_end_fn = LLVMGetIntrinsicDeclaration(cg -> module, id, va_end_overload_types, 1);

    LLVMTypeRef param_types[] = { LLVMPointerTypeInContext(cg -> ctx, 0) };
    LLVMTypeRef va_end_type = LLVMFunctionType(LLVMVoidTypeInContext(cg -> ctx), param_types, 1, 0);

    LLVMValueRef args[] = { va_ctx -> ap };

    LLVMBuildCall2(cg -> builder, va_end_type, va_end_fn, args, 1, "");

    va_ctx -> state = VA_LIST_DONE;

    return true;
}

static LLVMValueRef codegen_expression(CodegenCtx* cg, AstNodeId id) {
    AstNode* node = &cg -> file -> ast.nodes[id];

    switch (node -> kind) {
        case AST_IDENTIFIER: {
            SymbolId symbol_id = node -> as.identifier.symbol;

            if (cg -> symbol_values[symbol_id] != null) {
                return cg -> symbol_values[symbol_id];
            }

            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

            LLVMValueRef value = null;

            switch (symbol -> kind) {
                case SYMBOL_PARAMETER:
                    if (symbol -> as.parameter_symbol.type_id == driver.type_table.builtins.type_va_list) {
                        // LLVMTypeRef va_list_type = type_to_llvm(cg, driver.type_table.builtins.type_va_list);
                        // value = LLVMBuildLoad2(cg -> builder, va_list_type, cg -> va_ctx -> ap, "ap");

                        value = cg -> va_ctx -> ap;
                    } else {
                        LLVMValueRef fn = cg -> symbol_values[symbol -> as.parameter_symbol.function_id];
                        value = LLVMGetParam(fn, symbol -> as.parameter_symbol.index);
                    }

                    break;

                case SYMBOL_FUNCTION:
                    value = codegen_function_signature_from_symbol(cg, symbol);
                    break;

                default:
                    printf("Found %u\n", symbol -> kind);
                    UNREACHABLE("codegen_expression() :: AST_IDENTIFIER");
            }

            cg -> symbol_values[symbol_id] = value;

            return value;
        }

        case AST_LITERAL: {
            switch (node -> as.literal.kind) {
                case LITERAL_INTEGER:
                    return LLVMConstInt(type_to_llvm(cg, node -> resolved_type), node -> as.literal.as.integer, 0);

                case LITERAL_BOOL:
                    return LLVMConstInt(type_to_llvm(cg, node -> resolved_type), node -> as.literal.as.boolean, 0);

                case LITERAL_CHAR:
                    return LLVMConstInt(type_to_llvm(cg, node -> resolved_type), node -> as.literal.as.character, 0); 

                case LITERAL_FLOAT:
                    return LLVMConstReal(type_to_llvm(cg, node -> resolved_type), node -> as.literal.as.floating);

                case LITERAL_NULL:
                    return LLVMConstPointerNull(type_to_llvm(cg, node -> resolved_type));

                case LITERAL_STRING:
                    return get_or_insert_string(cg, node -> as.literal.as.string);
            }
        }

        case AST_FUNCTION_CALL: {
            LLVMValueRef fn = codegen_expression(cg, node -> as.function_call.identifier);
            
            if (fn == null) {
                fn = codegen_function_signature_from_call(cg, node);
            }

            LLVMTypeRef fn_type = LLVMGlobalGetValueType(fn);

            u32 arg_count = node -> as.function_call.arguments.count;

            LLVMValueRef* args = arena_alloc(&cg -> arena, arg_count * sizeof(LLVMValueRef));

            for (u32 i = 0; i < arg_count; i++) {
                AstNodeId arg_id  = node -> as.function_call.arguments.ids[i];
                AstNode* arg_node = &cg -> file -> ast.nodes[arg_id]; 

                LLVMValueRef arg = codegen_expression(cg, arg_id);

                LLVMTypeRef type = type_to_llvm(cg, arg_node -> resolved_type);

                if (
                    LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMPointerTypeKind && 
                    LLVMGetTypeKind(type) != LLVMPointerTypeKind
                ) {
                    arg = LLVMBuildLoad2(cg -> builder, type, arg, "arg");
                }

                args[i] = arg;
            }

            return LLVMBuildCall2(cg -> builder, fn_type, fn, args, arg_count, "result");
        }

        case AST_BINARY_OP: {
            // might need to use the types, kinda just DCE rn
            AstNode* lhs_node = &cg -> file -> ast.nodes[node -> as.binary_op.left];
            AstNode* rhs_node = &cg -> file -> ast.nodes[node -> as.binary_op.right];

            TypeId lhs_type = lhs_node -> resolved_type;
            TypeId rhs_type = rhs_node -> resolved_type;

            LLVMValueRef lhs = codegen_expression(cg, node -> as.binary_op.left);
            LLVMValueRef rhs = codegen_expression(cg, node -> as.binary_op.right);

            switch (node -> as.binary_op.op) {
                case TOK_PLUS:
                    return LLVMBuildAdd(cg -> builder, lhs, rhs, "add");

                case TOK_MINUS:
                    return LLVMBuildSub(cg -> builder, lhs, rhs, "sub");

                case TOK_STAR:
                    return LLVMBuildMul(cg -> builder, lhs, rhs, "mul");

                case TOK_SLASH:
                    if (is_type_float(lhs_type) || is_type_float(rhs_type)) {
                        return LLVMBuildFDiv(cg -> builder, lhs, rhs, "fdiv");
                    } else if (is_type_signed_int(lhs_type) || is_type_signed_int(rhs_type)) {
                        return LLVMBuildSDiv(cg -> builder, lhs, rhs, "idiv");
                    } else {
                        return LLVMBuildUDiv(cg -> builder, lhs, rhs, "udiv");
                    }

                case TOK_PERCENT:
                    if (is_type_float(lhs_type) || is_type_float(rhs_type)) {
                        return LLVMBuildFRem(cg -> builder, lhs, rhs, "frem");
                    } else if (is_type_signed_int(lhs_type) || is_type_signed_int(rhs_type)) {
                        return LLVMBuildSRem(cg -> builder, lhs, rhs, "irem");
                    } else {
                        return LLVMBuildURem(cg -> builder, lhs, rhs, "urem");
                    }


                case TOK_AMP:
                    return LLVMBuildAnd(cg -> builder, lhs, rhs, "and");

                case TOK_PIPE:
                    return LLVMBuildOr(cg -> builder, lhs, rhs, "or");

                case TOK_CARET:
                    return LLVMBuildXor(cg -> builder, lhs, rhs, "xor");

                case TOK_SHL:
                    return LLVMBuildShl(cg -> builder, lhs, rhs, "shl");

                case TOK_SHR:
                    if (is_type_signed_int(lhs_type)) {
                        return LLVMBuildAShr(cg -> builder, lhs, rhs, "shr");
                    } else {
                        return LLVMBuildLShr(cg -> builder, lhs, rhs, "shr");
                    }


                case TOK_EQ:
                    return LLVMBuildStore(cg -> builder, rhs, lhs);

                case TOK_PLUS_EQ: {
                    LLVMTypeRef type = type_to_llvm(cg, lhs_type);
                    LLVMValueRef value = LLVMBuildLoad2(cg -> builder, type, lhs, "value");
                    LLVMValueRef result = LLVMBuildAdd(cg -> builder, value, LLVMConstInt(type, 1, 0), "result");
                    return LLVMBuildStore(cg -> builder, result, lhs);
                }



                case TOK_EQ_EQ:
                    if (is_type_int(lhs_type)) {
                        return LLVMBuildICmp(cg -> builder, LLVMIntEQ, lhs, rhs, "eq");
                    } else {
                        return LLVMBuildFCmp(cg -> builder, LLVMRealOEQ, lhs, rhs, "eq");
                    }

                case TOK_BANG_EQ:
                    if (is_type_int(lhs_type)) {
                        return LLVMBuildICmp(cg -> builder, LLVMIntNE, lhs, rhs, "neq");
                    } else {
                        return LLVMBuildFCmp(cg -> builder, LLVMRealONE, lhs, rhs, "neq");
                    }

                case TOK_LT:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(cg -> builder, LLVMIntSLT, lhs, rhs, "lt");
                        } else {
                            return LLVMBuildICmp(cg -> builder, LLVMIntULT, lhs, rhs, "lt");
                        }
                    } else {
                        return LLVMBuildFCmp(cg -> builder, LLVMRealOLT, lhs, rhs, "lt");
                    }

                case TOK_LT_EQ:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(cg -> builder, LLVMIntSLE, lhs, rhs, "lteq");
                        } else {
                            return LLVMBuildICmp(cg -> builder, LLVMIntULE, lhs, rhs, "lteq");
                        }
                    } else {
                        return LLVMBuildFCmp(cg -> builder, LLVMRealOLE, lhs, rhs, "lteq");
                    }

                case TOK_GT:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(cg -> builder, LLVMIntSGT, lhs, rhs, "gt");
                        } else {
                            return LLVMBuildICmp(cg -> builder, LLVMIntUGT, lhs, rhs, "gt");
                        }
                    } else {
                        return LLVMBuildFCmp(cg -> builder, LLVMRealOGT, lhs, rhs, "gt");
                    }

                case TOK_GT_EQ:
                    if (is_type_int(lhs_type)) {
                        if (is_type_signed_int(lhs_type)) {
                            return LLVMBuildICmp(cg -> builder, LLVMIntSGE, lhs, rhs, "gteq");
                        } else {
                            return LLVMBuildICmp(cg -> builder, LLVMIntUGE, lhs, rhs, "gteq");
                        }
                    } else {
                        return LLVMBuildFCmp(cg -> builder, LLVMRealOGE, lhs, rhs, "gteq");
                    }

                // TODO: logical, assignment
                default:
                    UNREACHABLE("binary_op");
            }

        }

        case AST_UNARY_OP: {
            LLVMValueRef operand = codegen_expression(cg, node -> as.unary_op.operand);

            switch (node -> as.unary_op.op) {
                case TOK_MINUS:
                    return LLVMBuildNeg(cg -> builder, operand, "neg");

                case TOK_BANG: {
                    LLVMTypeRef operand_type = LLVMTypeOf(operand);
                    LLVMValueRef zero = LLVMConstInt(operand_type, 0, 0);
                    return LLVMBuildICmp(cg -> builder, LLVMIntEQ, operand, zero, "not");
                }

                case TOK_TILDE:
                    return LLVMBuildNot(cg -> builder, operand, "not");

                case TOK_AMP:
                    return operand;

                case TOK_STAR:
                    LLVMTypeRef base_type = type_to_llvm(cg, node -> resolved_type);
                    return LLVMBuildLoad2(cg -> builder, base_type, operand, "deref");

                default:
                    UNREACHABLE("unary_op");
            }
        }

        default:
            UNREACHABLE("codegen_expression()");
    }
}

static LLVMValueRef get_or_insert_string(CodegenCtx* cg, StringId id) {
    if (cg -> string_values[id] != null) {
        return cg -> string_values[id];
    }

    StringEntry entry = STRING_ID_LOOKUP(id);
    str8 str = entry.str;

    char putback = *(str.ptr + str.len);

    *(str.ptr + str.len) = 0;

    char name[64] = {0};

    snprintf(name, sizeof(name), "str_%u", id);

    LLVMValueRef value = LLVMBuildGlobalString(cg -> builder, str.ptr, name);

    *(str.ptr + str.len) = putback;

    cg -> string_values[id] = value;

    return value;
}

static LLVMTypeRef base_to_llvm(CodegenCtx* cg, TypeId id, TypeEntry* entry) {
    TypeBuiltinIds ids = driver.type_table.builtins;
    LLVMContextRef ctx = cg -> ctx;

    if (id == ids.type_void) {
        return LLVMVoidTypeInContext(ctx);
    }

    if (id == ids.type_u8 || id == ids.type_i8 || id == ids.type_bool || id == ids.type_char) {
        return LLVMInt8TypeInContext(ctx);
    }

    if (id == ids.type_u16 || id == ids.type_i16) {
        return LLVMInt16TypeInContext(ctx);
    }

    if (id == ids.type_u32 || id == ids.type_i32) {
        return LLVMInt32TypeInContext(ctx);
    }

    if (id == ids.type_u64 || id == ids.type_i64) {
        return LLVMInt64TypeInContext(ctx);
    }

    if (id == ids.type_usize || id == ids.type_isize) {
        return LLVMIntTypeInContext(ctx, entry -> size * 8);
    }

    if (id == ids.type_f32) {
        return LLVMFloatTypeInContext(ctx);
    }

    if (id == ids.type_f64) {
        return LLVMDoubleTypeInContext(ctx);
    }

    // TODO: make this compatible with other architectures
    // rn this only supports x86-64 linux
    if (id == ids.type_variadic) {
        LLVMTypeRef types[4] = {
            LLVMInt32TypeInContext(ctx),
            LLVMInt32TypeInContext(ctx),
            LLVMPointerTypeInContext(ctx, 0),
            LLVMPointerTypeInContext(ctx, 0)
        };

        return LLVMStructTypeInContext(ctx, types, 4, false);
    }

    if (id == ids.type_va_list) {
        return LLVMPointerTypeInContext(ctx, 0);
    }

    printf("Found: %.*s\n", STR8_FMT(STRING_ID_LOOKUP(entry -> as.base_type.name).str));
    UNREACHABLE("base_to_llvm()");
}

static LLVMTypeRef struct_to_llvm(CodegenCtx* cg, TypeEntry* entry) {
    u32 field_count = entry -> as.struct_type.field_count;

    LLVMTypeRef* field_types = arena_alloc(&cg -> arena, field_count * sizeof(LLVMTypeRef));

    for (u32 i = 0; i < field_count; i++) {
        field_types[i] = type_to_llvm(cg, entry -> as.struct_type.fields[i]);
    }

    return LLVMStructTypeInContext(cg -> ctx, field_types, field_count, false);
}

static LLVMTypeRef type_to_llvm(CodegenCtx* cg, TypeId id) {
    TypeEntry* entry = TYPE_ID_LOOKUP_REF(id);

    switch (entry -> kind) {
        case TYPE_BASE:
            return base_to_llvm(cg, id, entry);

        case TYPE_POINTER:
            return LLVMPointerType(type_to_llvm(cg, entry -> as.pointer_type.base), 0);

        case TYPE_STRUCT:
            return struct_to_llvm(cg, entry);

        case TYPE_ENUM:
            return type_to_llvm(cg, entry -> as.enum_type.underlying_type);

        default:
            UNREACHABLE("type_to_llvm()");
    }
}
