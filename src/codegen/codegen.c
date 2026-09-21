#include "ast/nodes/types.h"
#include "codegen/llvm_intrinsics/intrinsics.h"
#include "codegen/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "string_interner/types.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "token/token.h"
#include "token/types.h"
#include "types/entries/entries.h"
#include "types/entries/types.h"
#include "types/table/table.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <dwarf.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern DriverCtx driver;

static void codegen_file(CodegenCtx* ctx, FileId id);
static bool codegen_ast(CodegenCtx* ctx);

static LLVMValueRef codegen_global_variable(CodegenCtx* ctx, AstNode* node);

static LLVMValueRef codegen_intrinsic(CodegenCtx* ctx, IntrinsicId id, LLVMTypeRef* params, u32 param_count);
static LLVMValueRef codegen_function_signature(CodegenCtx* ctx, SymbolId id);
static LLVMValueRef codegen_function_declaration(CodegenCtx* ctx, AstNode* node);

static CodegenResult codegen_block(CodegenCtx* ctx, AstNodeId id);
static CodegenResult codegen_statement(CodegenCtx* ctx, AstNode* node);

static LLVMValueRef  codegen_variable_declaration(CodegenCtx* ctx, AstNode* node);

static CodegenResult codegen_if_statement(CodegenCtx* ctx, AstNode* node);
static CodegenResult codegen_for_loop(CodegenCtx* ctx, AstNode* node);
static CodegenResult codegen_while_loop(CodegenCtx* ctx, AstNode* node);
static CodegenResult codegen_return_statement(CodegenCtx* ctx, AstNode* node);

static LLVMValueRef codegen_lvalue(CodegenCtx* ctx, AstNodeId id);
static LLVMValueRef codegen_auto_deref(CodegenCtx* ctx, AstNodeId object_id, TypeId* type);
static LLVMValueRef codegen_expression(CodegenCtx* ctx, AstNodeId id);

static LLVMValueRef codegen_binary_logical(CodegenCtx* ctx, AstNode* node);
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
static LLVMMetadataRef type_id_to_dwarf(CodegenCtx* ctx, TypeId id);

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

    intrinsic_table_init(&ctx.intrinsics);

    u32 symbol_count = driver.symbol_table.symbol_count;
    u32 string_count = driver.string_interner.count;
    u32 type_count = driver.type_table.entry_count;

    u32 dwarf_type_size = type_count * sizeof(LLVMMetadataRef);
    u32 symbol_size = symbol_count * sizeof(LLVMValueRef);
    u32 string_size = string_count * sizeof(LLVMValueRef);
    u32 type_size = type_count * sizeof(LLVMTypeRef);

    arena_init(&ctx.map_arena, symbol_size + string_size + type_size + dwarf_type_size, ALIGN_DEFAULT);
    arena_init(&ctx.scratch, ARENA_KB(2), ALIGN_DEFAULT);
    arena_init(&ctx.defer_list.arena, ARENA_KB(1), ALIGN_DEFAULT);

    debug_printf("Init arena Map Arena (%p)", &ctx.map_arena);
    debug_printf("Init arena Scratch Arena (%p)", &ctx.scratch);
    debug_printf("Init arena Defer List Arena (%p)", &ctx.defer_list.arena);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    ctx.dwarf_type_map = arena_calloc(&ctx.map_arena, dwarf_type_size);
    ctx.symbol_map = arena_calloc(&ctx.map_arena, symbol_size);
    ctx.string_map = arena_calloc(&ctx.map_arena, string_size);
    ctx.type_map = arena_calloc(&ctx.map_arena, type_size);

    u32 file_count = driver.file_interner.count;

    for (u32 i = 0; i < file_count; i++) {
        codegen_file(&ctx, i);

        arena_reset(&ctx.scratch);
        arena_reset(&ctx.defer_list.arena);

        ctx.defer_list.stack = null;

        // reset to get rid of dangling pointers
        arena_memset(ctx.dwarf_type_map, 0, dwarf_type_size);
        arena_memset(ctx.symbol_map, 0, symbol_size);
        arena_memset(ctx.string_map, 0, string_size);
        arena_memset(ctx.type_map, 0, type_size);
    }

    arena_destroy(&ctx.scratch);
    arena_destroy(&ctx.map_arena);
    arena_destroy(&ctx.defer_list.arena);

    intrinsic_table_destroy(&ctx.intrinsics);
}

static void codegen_file(CodegenCtx* ctx, FileId id) {
    File* file = file_lookup_id(id);

    ctx -> file = file;

    ctx -> ctx         = LLVMContextCreate();
    ctx -> module      = LLVMModuleCreateWithNameInContext(ctx -> file -> path.ptr, ctx -> ctx);
    ctx -> builder     = LLVMCreateBuilderInContext(ctx -> ctx);
    ctx -> target_data = LLVMGetModuleDataLayout(ctx -> module);

    ctx -> defer_list.stack = null;
    ctx -> loop_ctx = null;

    ctx -> is_release_mode = driver.flags & DRIVER_FLAGS_RELEASE_MODE;

    LLVMAddModuleFlag(
        ctx -> module,
        LLVMModuleFlagBehaviorWarning,
        "DWARF Version",
        sizeof("DWARF Version") - 1,
        LLVMValueAsMetadata(LLVMConstInt(LLVMInt32TypeInContext(ctx -> ctx), 5, 0))
    );

    LLVMAddModuleFlag(
        ctx -> module,
        LLVMModuleFlagBehaviorWarning,
        "Debug Info Version",
        sizeof("Debug Info Version") - 1,
        LLVMValueAsMetadata(LLVMConstInt(LLVMInt32TypeInContext(ctx -> ctx), LLVMDebugMetadataVersion(), 0))
    );

    ctx -> debug_builder = LLVMCreateDIBuilder(ctx -> module);
    
    const char* last_slash = strrchr(file -> path.ptr, '/');
    assert(last_slash);

    u32 directory_len = last_slash - file -> path.ptr;
    u32 file_len = file -> path.len - directory_len - 1;

    ctx -> debug_file_metadata  = LLVMDIBuilderCreateFile(
        ctx -> debug_builder,
        last_slash + 1,
        file_len,
        file -> path.ptr,
        directory_len
    );

    ctx -> debug_unit = LLVMDIBuilderCreateCompileUnit(
        ctx -> debug_builder,
        LLVMDWARFSourceLanguageC99,
        ctx -> debug_file_metadata,
        "lilyc", sizeof("lilyc") - 1,
        ctx -> is_release_mode,
        "", 0, // string, sizeof(string) - 1;
        0,
        "" , 0,
        LLVMDWARFEmissionFull,
        0,
        0,
        0,
        "",0,
        "", 0
    );

    ctx -> debug_scope = ctx -> debug_unit;

    // IR
    assert(ctx -> ctx != null);
    assert(ctx -> module != null);
    assert(ctx -> builder != null);

    // Debug 
    assert(ctx -> debug_file_metadata != null);
    assert(ctx -> debug_builder != null);
    assert(ctx -> debug_unit != null);

    if (!codegen_ast(ctx)) {
        diagnostic_add_generic(DIAG_ERROR, "Failed to create LLVM IR");
        goto cleanup;
    }

    LLVMDIBuilderFinalize(ctx -> debug_builder);

    char* msg = null;

#ifdef DEBUG_MODE
    printf("Verifying module\n");
    if (LLVMVerifyModule(ctx -> module, LLVMReturnStatusAction, &msg) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: module verification failed: %s", msg);
        LLVMDisposeMessage(msg);
        goto cleanup;
    }
#endif //DEBUG_MODE

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
        ctx -> is_release_mode ? LLVMCodeGenLevelDefault : LLVMCodeGenLevelNone,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );

    LLVMPassBuilderOptionsRef pb_options = LLVMCreatePassBuilderOptions();

    LLVMErrorRef error = LLVMRunPasses(
        ctx -> module,
        ctx -> is_release_mode ? "default<O2>" : "default<O0>",
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

    // printf("\n\n\n=== MODULE ===\n\n");
    // printf("%s",LLVMPrintModuleToString(ctx -> module));

    LLVMDisposeTargetMachine(target_machine);

cleanup:
    LLVMDisposeBuilder(ctx -> builder);
    LLVMDisposeModule(ctx -> module);
    LLVMContextDispose(ctx -> ctx);
}

static bool codegen_ast(CodegenCtx* ctx) {
    Ast* ast = &ctx -> file -> ast;

    for (u32 i = 0; i < ast -> node_count; i++) {
        AstNode* node = &ast -> nodes[i];

        if (!(node -> flags & AST_FLAGS_IS_TOP_DECL)) {
            continue;
        } 

        bool result = true;

        switch (node -> kind) {
            case AST_FUNCTION_DECL:
                if (!codegen_function_declaration(ctx, node)) result = false;
                break;

            case AST_VARIABLE_DECL:
                if (!codegen_global_variable(ctx, node)) result = false;
                break;

            default:
                break;
        }

        arena_reset(&ctx -> defer_list.arena);

        ctx -> defer_list.stack = null;
        ctx -> fn = null;

        if (!result) {
            return false;
        }
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

    if (node -> flags & AST_FLAGS_IS_FOREIGN) {
        LLVMSetLinkage(var, LLVMExternalLinkage);
    }

    ctx -> symbol_map[node -> resolved_symbol] = var;

    return var;
}

static LLVMValueRef codegen_intrinsic(CodegenCtx* ctx, IntrinsicId id, LLVMTypeRef* params, u32 param_count) {
    IntrinsicEntry entry = ctx -> intrinsics.entries[id];

    str8 base_name = STRING_ID_LOOKUP(entry.intrinsic_name).str;

    u32 llvm_id = LLVMLookupIntrinsicID(base_name.ptr, base_name.len);
    assert(llvm_id != 0);

    LLVMTypeRef overload_types[3] = {0};

    u32 overload_count = 0;

    for (u32 i = 0; i < param_count; i++) {
        overload_types[overload_count++] = params[i];
    }

    return LLVMGetIntrinsicDeclaration(ctx -> module, llvm_id, overload_types, overload_count);
}

static LLVMValueRef codegen_function_signature(CodegenCtx* ctx, SymbolId id) {
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    LLVMTypeRef* param_types = null;
    TypeId* param_type_ids = null;

    TypeId ret_type_id = symbol -> as.function_symbol.return_type_id;

    bool is_variadic = symbol -> flags & AST_FLAGS_IS_VARIADIC;

    u32 n = symbol -> as.function_symbol.parameter_count;
    u32 param_count = is_variadic ? n - 1 : n;

    if (param_count != 0) {
        param_types    = arena_alloc(&ctx -> scratch, param_count * sizeof(LLVMTypeRef));
        param_type_ids = arena_alloc(&ctx -> scratch, param_count * sizeof(TypeId));
    }

    for (u32 i = 0; i < param_count; i++) {
        SymbolId param_id = symbol -> as.function_symbol.parameters[i];
        Symbol* param = SYMBOL_ID_LOOKUP_REF(param_id);

        TypeId param_type_id = param -> as.parameter_symbol.type_id;

        param_types[i] = type_id_to_llvm(ctx, param_type_id);
        param_type_ids[i] = param_type_id;
    }

    if (symbol -> flags & AST_FLAGS_IS_INTRINSIC) {
        IntrinsicId intrinsic_id = intrinsic_lookup(
            &ctx -> intrinsics,
            symbol -> name_id,
            ret_type_id,
            param_count,
            param_type_ids
        );

        if (intrinsic_id != INTRINSIC_ID_NONE) {
            return codegen_intrinsic(ctx, intrinsic_id, param_types, param_count);
        }

        return null;
    }

    LLVMTypeRef ret_type = type_id_to_llvm(ctx, ret_type_id);
    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types, param_count, is_variadic);

    str8 name = STRING_ID_LOOKUP(symbol -> name_id).str;

    LLVMValueRef fn = LLVMGetOrInsertFunction(ctx -> module, name.ptr, name.len, fn_type); 

    if (symbol -> flags & AST_FLAGS_IS_INLINE) {
        u32 attr_index = LLVMGetEnumAttributeKindForName("alwaysinline", 12);
        assert(attr_index != 0);

        LLVMAttributeRef attr = LLVMCreateEnumAttribute(ctx -> ctx, attr_index, 0);

        LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex, attr);
    } else if (symbol -> flags & AST_FLAGS_IS_NOINLINE) {
        u32 attr_index = LLVMGetEnumAttributeKindForName("noinline", 8);
        assert(attr_index != 0);

        LLVMAttributeRef attr = LLVMCreateEnumAttribute(ctx -> ctx, attr_index, 0);

        LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex, attr);
    }

    return fn;
}

static LLVMValueRef codegen_function_declaration(CodegenCtx* ctx, AstNode* node) {
    SymbolId symbol_id = node -> resolved_symbol;
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    LLVMValueRef fn = codegen_function_signature(ctx, symbol_id);
    assert(fn != null);

    ctx -> symbol_map[symbol_id] = fn;
    ctx -> fn = fn;

    ctx -> loop_ctx = null;
    ctx -> defer_list.stack = null;

    // no body just return
    if (symbol -> flags & AST_FLAGS_IS_INTRINSIC || symbol -> flags & AST_FLAGS_IS_FOREIGN) {
        ctx -> debug_scope = ctx -> debug_unit;
        ctx -> debug_fn = null;
        return fn;
    }

    bool is_variadic = symbol -> flags & AST_FLAGS_IS_VARIADIC;

    u32 n = symbol -> as.function_symbol.parameter_count;
    u32 param_count = is_variadic ? n - 1 : n;


    // Emit debug information


    LLVMMetadataRef dwarf_ret_type = type_id_to_dwarf(ctx, symbol -> as.function_symbol.return_type_id);

    u32 dwarf_param_count = param_count;

    LLVMMetadataRef* dwarf_param_types = arena_alloc(
        &ctx -> scratch,
        (dwarf_param_count + 1 + (is_variadic ? 1: 0)) * sizeof(LLVMMetadataRef)
    );

    dwarf_param_types[0] = dwarf_ret_type;

    for (u32 i = 0; i < param_count; i++) {
        SymbolId param_id = symbol -> as.function_symbol.parameters[i];
        Symbol* param = SYMBOL_ID_LOOKUP_REF(param_id);

        dwarf_param_types[i + 1] = type_id_to_dwarf(ctx, param -> as.parameter_symbol.type_id); 
    }

    if (is_variadic) {
        dwarf_param_types[param_count + 1] = null;
        dwarf_param_count += 1;
    }

    LLVMMetadataRef fn_dwarf_type = LLVMDIBuilderCreateSubroutineType(
        ctx -> debug_builder,
        ctx -> debug_file_metadata,
        dwarf_param_types,
        dwarf_param_count + 1,
        LLVMDIFlagZero
    );

    SourceLocation location = token_get_source_location(ctx -> file, node -> tokens.start);

    str8 name = STRING_ID_LOOKUP(symbol -> name_id).str;

    LLVMMetadataRef sp = LLVMDIBuilderCreateFunction(
        ctx -> debug_builder,
        ctx -> debug_unit,
        name.ptr,
        name.len,
        name.ptr,
        name.len,
        ctx -> debug_file_metadata,
        location.line,
        fn_dwarf_type,
        0,
        1,
        location.line,
        LLVMDIFlagZero,
        ctx -> is_release_mode ? 1 : 0
    );

    LLVMSetSubprogram(fn, sp);
    ctx -> debug_fn = sp;
    ctx -> debug_scope = sp;


    // End of debug information


    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx -> ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx -> builder, entry);

    LLVMSetCurrentDebugLocation2(ctx -> builder, null);

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

        ctx -> symbol_map[param_id] = address;

        // Emit debug information for parameters
        str8 param_name = STRING_ID_LOOKUP(param -> name_id).str;

        AstNode* param_node = &ctx -> file -> ast.nodes[param -> ast_node_id];
        SourceLocation param_source_loc = token_get_source_location(ctx -> file, param_node -> tokens.start);

        LLVMMetadataRef debug_param = LLVMDIBuilderCreateParameterVariable(
            ctx -> debug_builder,
            ctx -> debug_scope,
            param_name.ptr,
            param_name.len,
            i,
            ctx -> debug_file_metadata,
            param_source_loc.line,
            dwarf_param_types[i + 1],
            1,
            LLVMDIFlagZero
        );

        LLVMMetadataRef empty_expr = LLVMDIBuilderCreateExpression(ctx -> debug_builder, null, 0);
        LLVMMetadataRef debug_loc = LLVMDIBuilderCreateDebugLocation(
            ctx -> ctx,
            param_source_loc.line,
            param_source_loc.col,
            ctx -> debug_scope,
            null
        );

        LLVMDIBuilderInsertDeclareRecordAtEnd(
            ctx -> debug_builder,
            address,
            debug_param,
            empty_expr,
            debug_loc,
            LLVMGetLastBasicBlock(ctx -> fn)
        );
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

    LLVMDIBuilderFinalizeSubprogram(ctx -> debug_builder, ctx -> debug_fn);
    ctx -> debug_scope = ctx -> debug_unit;

    return fn;
}

static CodegenResult codegen_block(CodegenCtx* ctx, AstNodeId id) {
    AstNode* node = &ctx -> file -> ast.nodes[id];

    // Debug info
    SourceLocation location = token_get_source_location(ctx -> file, node -> tokens.start);

    LLVMMetadataRef debug_block = LLVMDIBuilderCreateLexicalBlock(
        ctx -> debug_builder,
        ctx -> debug_scope,
        ctx -> debug_file_metadata,
        location.line,
        location.col
    );

    LLVMMetadataRef previous_debug_scope = ctx -> debug_scope;

    ctx -> debug_scope = debug_block;

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

    ctx -> debug_scope = previous_debug_scope;

    return result;
}

static CodegenResult codegen_statement(CodegenCtx* ctx, AstNode* node) {
    SourceLocation location = token_get_source_location(ctx -> file, node -> tokens.start);

    LLVMMetadataRef debug_location = LLVMDIBuilderCreateDebugLocation(
        ctx -> ctx,
        location.line,
        location.col,
        ctx -> debug_scope,
        null
    );

    LLVMSetCurrentDebugLocation2(ctx -> builder, debug_location);

    switch (node -> kind) {
        case AST_BLOCK:
            return codegen_block(ctx, node -> id);

        case AST_VARIABLE_DECL:
            return codegen_variable_declaration(ctx, node) != null ? CODEGEN_FALLTHROUGH : CODEGEN_ERROR;

        case AST_IF_STMT:
            return codegen_if_statement(ctx, node);

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

    ctx -> symbol_map[node -> resolved_symbol] = address;

    // Emit debug information

    str8 var_name = STRING_ID_LOOKUP(node -> as.variable_decl.name).str;
    SourceLocation source_loc = token_get_source_location(ctx -> file, node -> tokens.start);

    LLVMMetadataRef debug_variable = LLVMDIBuilderCreateAutoVariable(
        ctx -> debug_builder,
        ctx -> debug_scope,
        var_name.ptr,
        var_name.len,
        ctx -> debug_file_metadata,
        source_loc.line,
        type_id_to_dwarf(ctx, node -> resolved_type),
        true,
        LLVMDIFlagZero,
        0
    );

    LLVMMetadataRef empty_expr = LLVMDIBuilderCreateExpression(ctx -> debug_builder, null, 0);
    LLVMMetadataRef debug_loc = LLVMDIBuilderCreateDebugLocation(
        ctx -> ctx,
        source_loc.line,
        source_loc.col,
        ctx -> debug_scope,
        null
    );

    LLVMDIBuilderInsertDeclareRecordAtEnd(
        ctx -> debug_builder,
        address,
        debug_variable,
        empty_expr,
        debug_loc,
        LLVMGetLastBasicBlock(ctx -> fn)
    );

    // End of debug information

    if (node -> as.variable_decl.value_expr == AST_NODE_ID_NONE) {
        if (is_type(node -> resolved_type, TYPE_POINTER)) {
            LLVMValueRef value = LLVMConstPointerNull(type_id_to_llvm(ctx, node -> resolved_type));

            if (value == null) {
                return null;
            }

            LLVMBuildStore(ctx -> builder, value, address);
        }

        return address;
    }

    LLVMValueRef value = codegen_expression(ctx, node -> as.variable_decl.value_expr);

    if (value == null) {
        return null;
    }

    LLVMBuildStore(ctx -> builder, value, address);

    return address;
}

static CodegenResult codegen_if_statement(CodegenCtx* ctx, AstNode* node) {
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "if.exit");
    LLVMBasicBlockRef else_block = null;
    
    if (node -> as.if_stmt.else_block != AST_NODE_ID_NONE) {
        else_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    }

    u32 branch_count = node -> as.if_stmt.branches.count;

    LLVMBasicBlockRef* cond_blocks = calloc(branch_count, sizeof(LLVMBasicBlockRef));
    LLVMBasicBlockRef* body_blocks = calloc(branch_count, sizeof(LLVMBasicBlockRef));

    for (u32 i = 0; i < branch_count; i++) {
        cond_blocks[i] = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "if.cond");
        body_blocks[i] = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "if.body");
    }

    LLVMBuildBr(ctx -> builder, cond_blocks[0]);

    for (u32 i = 0; i < branch_count; i++) {
        AstNodeId branch_id  = node -> as.if_stmt.branches.ids[i];
        AstNode* branch_node = &ctx -> file -> ast.nodes[branch_id];

        LLVMBasicBlockRef cond_block = cond_blocks[i];
        LLVMBasicBlockRef body_block = body_blocks[i];

        LLVMPositionBuilderAtEnd(ctx -> builder, cond_block);

        LLVMValueRef condition = codegen_expression(ctx, branch_node -> as.branch.condition);

        if (condition == null) {
            free(cond_blocks);
            free(body_blocks);

            return CODEGEN_ERROR;
        }

        LLVMBasicBlockRef next_block = exit_block;

        if (i + 1 < branch_count) {
            next_block = cond_blocks[i + 1];
        } else if (else_block != null) {
            next_block = else_block;
        }

        LLVMBuildCondBr(ctx -> builder, condition, body_block, next_block);

        // Body
        LLVMPositionBuilderAtEnd(ctx -> builder, body_block);

        CodegenResult result = codegen_block(ctx, branch_node -> as.branch.block);

        if (result == CODEGEN_ERROR) {
            free(cond_blocks);
            free(body_blocks);

            return CODEGEN_ERROR;
        }

        if (result == CODEGEN_FALLTHROUGH) {
            LLVMBuildBr(ctx -> builder, exit_block);
        }
    }

    if (else_block != null) {
        LLVMPositionBuilderAtEnd(ctx -> builder, else_block);

        CodegenResult result = codegen_block(ctx, node -> as.if_stmt.else_block);

        if (result == CODEGEN_ERROR) {
            free(cond_blocks);
            free(body_blocks);

            return CODEGEN_ERROR;
        }

        if (result == CODEGEN_FALLTHROUGH) {
            LLVMBuildBr(ctx -> builder, exit_block);
        }
    }

    // Exit
    LLVMPositionBuilderAtEnd(ctx -> builder, exit_block);

    free(cond_blocks);
    free(body_blocks);

    return CODEGEN_FALLTHROUGH;
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
                    LLVMValueRef address = ctx -> symbol_map[symbol_id];
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

        case AST_INDEX: {
            AstNode* object_node = &ctx -> file -> ast.nodes[node -> as.index.object];

            LLVMValueRef user_index = codegen_expression(ctx, node -> as.index.index_expr); 

            // TODO: decide whether I want this or not

            if (is_type(object_node -> resolved_type, TYPE_POINTER)) {
                TypeEntry* entry = TYPE_ID_LOOKUP_REF(object_node -> resolved_type);

                LLVMTypeRef element_type = type_id_to_llvm(ctx, entry -> as.pointer_type.base);

                LLVMValueRef indices[] = { user_index };

                LLVMValueRef object = codegen_expression(ctx, node -> as.index.object);

                return LLVMBuildGEP2(ctx -> builder, element_type, object, indices, 1, "");
            }

            LLVMTypeRef array_type = type_id_to_llvm(ctx, object_node -> resolved_type); 

            LLVMValueRef zero_index = LLVMConstInt(LLVMInt64TypeInContext(ctx -> ctx), 0, 0); 
            LLVMValueRef indices[] = { zero_index, user_index };

            LLVMValueRef object = codegen_lvalue(ctx, node -> as.index.object);

            return LLVMBuildGEP2(ctx -> builder, array_type, object, indices, 2, ""); 
        } break;

        case AST_MEMBER_ACCESS: {
            AstNodeId object_id = node -> as.member_access.object;

            Symbol* field = SYMBOL_ID_LOOKUP_REF(node -> resolved_symbol);
            assert(field -> kind == SYMBOL_FIELD);

            TypeId object_type_id = TYPE_ID_NONE;
            LLVMValueRef object = codegen_auto_deref(ctx, object_id, &object_type_id);

            if (is_type(object_type_id, TYPE_STRUCT)) {
                LLVMTypeRef object_type = type_id_to_llvm(ctx, object_type_id);
                LLVMTypeRef i32_type = LLVMInt32TypeInContext(ctx -> ctx);

                LLVMValueRef indices[2] = {
                    LLVMConstInt(i32_type, 0, 0),
                    LLVMConstInt(i32_type, field -> as.field_symbol.index, 0),
                };

                return LLVMBuildGEP2(ctx -> builder, object_type, object, indices, 2, "");
            }

            if (is_type(object_type_id, TYPE_UNION))  {
                return object;
            }

            UNREACHABLE("codegen_lvalue() | case AST_MEMBER_ACCESS");
        } break;

        default:
            break;
    }

    printf("Found: %s\n", AST_NODE_KIND_STRINGS[node -> kind]);
    UNREACHABLE("codegen_lvalue()");
}

static LLVMValueRef codegen_auto_deref(CodegenCtx* ctx, AstNodeId object_id, TypeId* type) {
    AstNode* object_node = &ctx -> file -> ast.nodes[object_id];

    *type = object_node -> resolved_type;

    if (!is_type(*type, TYPE_POINTER)) {
        return codegen_lvalue(ctx, object_id);
    }

    LLVMValueRef address = codegen_expression(ctx, object_id);

    *type = TYPE_ID_LOOKUP_REF(*type) -> as.pointer_type.base;

    while (is_type(*type, TYPE_POINTER)) {
        LLVMTypeRef ptr_type = type_id_to_llvm(ctx, *type);

        address = LLVMBuildLoad2(ctx -> builder, ptr_type, address, "");

        *type = TYPE_ID_LOOKUP_REF(*type) -> as.pointer_type.base;
    }

    return address;
}

static LLVMValueRef codegen_expression(CodegenCtx* ctx, AstNodeId id) {
    AstNode* node = &ctx -> file -> ast.nodes[id];

    switch (node -> kind) {
        case AST_IDENTIFIER: {
            SymbolId symbol_id = node -> resolved_symbol;
            assert(symbol_id != SYMBOL_ID_NONE);
            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

            switch (symbol -> kind) {
                case SYMBOL_VARIABLE: {
                    LLVMValueRef address = ctx -> symbol_map[symbol_id];
                    assert(address != null);

                    LLVMTypeRef type = type_id_to_llvm(ctx, symbol -> as.variable_symbol.type_id);

                    return LLVMBuildLoad2(ctx -> builder, type, address, "");
                } break;

                case SYMBOL_PARAMETER: {
                    if (symbol -> as.parameter_symbol.type_id == driver.type_table.builtins.type_va_list) {
                        return ctx -> va_ctx.ap;
                    }

                    LLVMValueRef address = ctx -> symbol_map[symbol_id];
                    assert(address != null);

                    LLVMTypeRef type = type_id_to_llvm(ctx, symbol -> as.parameter_symbol.type_id);

                    return LLVMBuildLoad2(ctx -> builder, type, address, "");
                } break;

                case SYMBOL_FUNCTION: {
                    if (ctx -> symbol_map[symbol_id] != null) {
                        return ctx -> symbol_map[symbol_id];
                    }

                    LLVMValueRef fn = codegen_function_signature(ctx, symbol_id);

                    ctx -> symbol_map[symbol_id] = fn;

                    return fn;
                } break;

                case SYMBOL_VARIANT: {
                    if (ctx -> symbol_map[symbol_id] != null) {
                        return ctx -> symbol_map[symbol_id];
                    }

                    TypeEntry* entry = TYPE_ID_LOOKUP_REF(symbol -> as.variant_symbol.type_id);
                    assert(entry -> kind == TYPE_ENUM);

                    LLVMTypeRef type = type_id_to_llvm(ctx, entry -> as.enum_type.underlying_type);
                    LLVMValueRef value = LLVMConstInt(type, symbol -> as.variant_symbol.value, 0);

                    ctx -> symbol_map[symbol_id] = value;

                    return value;
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

        case AST_MEMBER_ACCESS: {
            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(node -> resolved_symbol);

            switch (symbol -> kind) {
                case SYMBOL_FIELD: {
                    LLVMValueRef address = codegen_lvalue(ctx, id);
                    LLVMTypeRef type = type_id_to_llvm(ctx, node -> resolved_type);

                    return LLVMBuildLoad2(ctx -> builder, type, address, "");
                } break;

                case SYMBOL_FUNCTION:
                case SYMBOL_VARIANT:
                    return codegen_expression(ctx, node -> as.member_access.member);

                default:
                    UNREACHABLE("codegen_expression() | AST_MEMBER_ACCESS symbol kind");
            }
        } break;

        case AST_INDEX: {
            LLVMValueRef address = codegen_lvalue(ctx, id);
            LLVMTypeRef element_type = type_id_to_llvm(ctx, node -> resolved_type);

            return LLVMBuildLoad2(ctx -> builder, element_type, address, ""); 
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
            TokenKind op = node -> as.binary_op.op;

            if (op == TOK_AMP_AMP || op == TOK_PIPE_PIPE) {
                return codegen_binary_logical(ctx, node);
            }

            AstNode* lhs_node = &ctx -> file -> ast.nodes[node -> as.binary_op.left];
            AstNode* rhs_node = &ctx -> file -> ast.nodes[node -> as.binary_op.right];

            TypeId lhs_type = lhs_node -> resolved_type;
            TypeId rhs_type = rhs_node -> resolved_type;

            if (is_type(lhs_type, TYPE_ENUM)) {
                TypeEntry* entry = TYPE_ID_LOOKUP_REF(lhs_type);

                lhs_type = entry -> as.enum_type.underlying_type;
            }

            if (is_type(rhs_type, TYPE_ENUM)) {
                TypeEntry* entry = TYPE_ID_LOOKUP_REF(rhs_type);

                rhs_type = entry -> as.enum_type.underlying_type;
            }

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

            if (!is_type(lhs_type, TYPE_POINTER) && !is_type(rhs_type, TYPE_POINTER)) {
                if (lhs_type > rhs_type) {
                    rhs = LLVMBuildZExt(ctx -> builder, rhs, type_id_to_llvm(ctx, lhs_type), "");
                } else if (lhs_type < rhs_type) {
                    lhs = LLVMBuildZExt(ctx -> builder, lhs, type_id_to_llvm(ctx, rhs_type), "");
                }
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
                    if (is_type_int(lhs_type) || is_type(lhs_type, TYPE_POINTER)) {
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
        } break;

        default:
            UNREACHABLE("codegen_expression() not yet added")
    }
}

static LLVMValueRef codegen_binary_logical(CodegenCtx* ctx, AstNode* node) {
    bool is_and = node -> as.binary_op.op == TOK_AMP_AMP;

    LLVMValueRef lhs = codegen_expression(ctx, node -> as.binary_op.left);

    if (lhs == null) {
        return null;
    }

    // get where LHS ended because LHS itself could be nested blocks
    LLVMBasicBlockRef lhs_exit_block = LLVMGetInsertBlock(ctx -> builder);

    LLVMBasicBlockRef rhs_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(ctx -> ctx, ctx -> fn, "");

    if (is_and) {
        LLVMBuildCondBr(ctx -> builder, lhs, rhs_block, merge_block);
    } else {
        LLVMBuildCondBr(ctx -> builder, lhs, merge_block, rhs_block);
    }

    LLVMPositionBuilderAtEnd(ctx -> builder, rhs_block);

    LLVMValueRef rhs = codegen_expression(ctx, node -> as.binary_op.right);

    if (rhs == null) {
        return null;
    }

    // get where RHS ended because RHS itself could be nested blocks
    LLVMBasicBlockRef rhs_exit_block = LLVMGetInsertBlock(ctx -> builder);

    LLVMBuildBr(ctx -> builder, merge_block);
    LLVMPositionBuilderAtEnd(ctx -> builder, merge_block);

    LLVMTypeRef bool_type = LLVMInt1TypeInContext(ctx -> ctx);
    LLVMValueRef phi = LLVMBuildPhi(ctx -> builder, bool_type, "");

    // if it was AND and we came from LHS it means LHS failed, therefore false
    // inverse for OR, if we came from LHS in an OR it means it was true and the
    // LHS expression succeeded
    LLVMValueRef short_circuit_value = LLVMConstInt(bool_type, is_and ? false : true, 0);

    LLVMValueRef        incoming_values[2] = { short_circuit_value, rhs };
    LLVMBasicBlockRef   incoming_blocks[2] = { lhs_exit_block, rhs_exit_block };

    // THIS IS SO FREAKING COOL, i love programming PHI is really cool :3
    LLVMAddIncoming(phi, incoming_values, incoming_blocks, 2);

    return phi;
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

static LLVMTypeRef union_to_llvm(CodegenCtx* ctx, TypeEntry* entry) {
    u32 field_count = entry -> as.union_type.field_count;

    if (field_count == 0) {
        return LLVMStructTypeInContext(ctx -> ctx, NULL, 0, false);
    }

    TypeId largest_field_id = entry -> as.union_type.largest_field_id;
    TypeEntry* largest_field_entry = TYPE_ID_LOOKUP_REF(largest_field_id);

    LLVMTypeRef body[2];
    u32 body_count = 0;

    body[body_count++] = type_id_to_llvm(ctx, largest_field_id);

    u64 padding = entry -> size - largest_field_entry -> size;

    if (padding > 0) {
        body[body_count++] = LLVMArrayType2(LLVMInt8TypeInContext(ctx -> ctx), padding);
    }

    LLVMTypeRef result = LLVMStructTypeInContext(ctx -> ctx, body, body_count, false);

    assert(LLVMABISizeOfType(ctx -> target_data, result) == entry -> size);
    assert(LLVMABIAlignmentOfType(ctx -> target_data, result) == entry -> alignment);

    return result;
}

static LLVMTypeRef array_to_llvm(CodegenCtx* ctx, TypeEntry* entry) {
    LLVMTypeRef element_type = type_id_to_llvm(ctx, entry -> as.array_type.element);
    return LLVMArrayType2(element_type, entry -> as.array_type.size); 
}

static LLVMTypeRef type_id_to_llvm(CodegenCtx* ctx, TypeId id) {
    if (ctx -> type_map[id] != null) {
        return ctx -> type_map[id];
    }

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(id);

    switch (entry -> kind) {
        case TYPE_BASE:
            return (ctx -> type_map[id] = base_to_llvm(ctx, id, entry));

        case TYPE_POINTER:
            return (ctx -> type_map[id] = LLVMPointerType(type_id_to_llvm(ctx, entry -> as.pointer_type.base), 0));

        case TYPE_STRUCT:
            return (ctx -> type_map[id] = struct_to_llvm(ctx, entry));

        case TYPE_UNION:
            return (ctx -> type_map[id] = union_to_llvm(ctx, entry));

        case TYPE_ENUM:
            return (ctx -> type_map[id] = type_id_to_llvm(ctx, entry -> as.enum_type.underlying_type));

        case TYPE_ARRAY:
            return (ctx -> type_map[id] = array_to_llvm(ctx, entry));

        default:
            UNREACHABLE("type_id_to_llvm()");
    }
}

static LLVMMetadataRef base_to_dwarf(CodegenCtx* ctx, TypeId id, TypeEntry* entry) {
    TypeBuiltinIds ids = driver.type_table.builtins;

    StringEntry name_entry = STRING_ID_LOOKUP(entry -> as.base_type.name);
    str8 name = name_entry.str;

    if (id == ids.type_void) {
        return null;
    }

    if (id == ids.type_bool) {
        return LLVMDIBuilderCreateBasicType(
            ctx -> debug_builder,
            name.ptr,
            name.len,
            8,
            DW_ATE_boolean,
            LLVMDIFlagZero
        );
    }

    if (is_type_signed_int(id)) {
        return LLVMDIBuilderCreateBasicType(
            ctx -> debug_builder,
            name.ptr,
            name.len,
            entry -> size * 8,
            DW_ATE_signed,
            LLVMDIFlagZero
        );
    }

    if (is_type_unsigned_int(id)) {
        return LLVMDIBuilderCreateBasicType(
            ctx -> debug_builder,
            name.ptr,
            name.len,
            entry -> size * 8,
            DW_ATE_unsigned,
            LLVMDIFlagZero
        );
    }

    if (is_type_float(id)) {
        return LLVMDIBuilderCreateBasicType(
            ctx -> debug_builder,
            name.ptr,
            name.len,
            entry -> size * 8,
            DW_ATE_float,
            LLVMDIFlagZero
        );
    }

    return null;
}

static LLVMMetadataRef pointer_to_dwarf(CodegenCtx* ctx, TypeEntry* entry) {
    TypeId base_type = entry -> as.pointer_type.base;

    LLVMMetadataRef base_dwarf_type = null;

    if (!is_type_void(base_type)) {
        base_dwarf_type = type_id_to_dwarf(ctx, base_type);
    }

    return LLVMDIBuilderCreatePointerType(
        ctx -> debug_builder,
        base_dwarf_type,
        entry -> size * 8,
        0,
        0,
        null,
        0
    );
}

static LLVMMetadataRef array_to_dwarf(CodegenCtx* ctx, TypeEntry* entry) {
    TypeId element_id = entry -> as.array_type.element;
    u64 size = entry -> as.array_type.size;

    LLVMMetadataRef element_dwarf_type = type_id_to_dwarf(ctx, element_id);
    LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(ctx -> debug_builder, 0, size);

    return LLVMDIBuilderCreateArrayType(
        ctx -> debug_builder,
        size,
        entry -> alignment * 8,
        element_dwarf_type,
        &subrange,
        1
    );
}

static LLVMMetadataRef type_id_to_dwarf(CodegenCtx* ctx, TypeId id) {
    if (ctx -> dwarf_type_map[id] != null) {
        return ctx -> dwarf_type_map[id];
    }

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(id);

    switch (entry -> kind) {
        case TYPE_BASE:
            return (ctx -> dwarf_type_map[id] = base_to_dwarf(ctx, id, entry));

        case TYPE_POINTER:
            return (ctx -> dwarf_type_map[id] = pointer_to_dwarf(ctx, entry));

        case TYPE_ENUM:
            return (ctx -> dwarf_type_map[id] = type_id_to_dwarf(ctx, entry -> as.enum_type.underlying_type));

        case TYPE_ARRAY:
            return (ctx -> dwarf_type_map[id] = array_to_dwarf(ctx, entry));

        default:
            return LLVMDIBuilderCreateUnspecifiedType(ctx -> debug_builder, "", 0);
    }
}

static LLVMValueRef get_or_insert_string(CodegenCtx* ctx, StringId id) {
    if (ctx -> string_map[id] != null) {
        return ctx -> string_map[id];
    }

    StringEntry entry = STRING_ID_LOOKUP(id);
    str8 str = entry.str;

    // Make an owning copy for the thread (when we get to multithreaded)
    char* copy = arena_calloc(&ctx -> scratch, str.len + 1);
    char name[64] = {0};

    char* cursor = copy;

    u32 i = 0;

    while (i < str.len) {
        if (str.ptr[i] == '\\') {
            i += 1;

            char c = str.ptr[i];

            // at end or early break, no point in parsing rest of the string
            if (c == '0') {
                *cursor++ = '\0';
                break;
            }

            switch (c) {
                case 'a':  { *cursor++ = '\a'; } break;
                case 'b':  { *cursor++ = '\b'; } break;
                case 'f':  { *cursor++ = '\f'; } break;
                case 'n':  { *cursor++ = '\n'; } break;
                case 'r':  { *cursor++ = '\r'; } break;
                case 't':  { *cursor++ = '\t'; } break;
                case 'v':  { *cursor++ = '\v'; } break;
                case '\\': { *cursor++ = '\\'; } break;
                case '"':  { *cursor++ = '\"'; } break;

                default: {
                    *cursor++ = '\\';
                    *cursor++ = c;
                } break;
            }
        } else {
            *cursor++ = str.ptr[i];
        }

        i++;
    }

    snprintf(name, sizeof(name), "str_%u", id);

    LLVMValueRef value = LLVMBuildGlobalString(ctx -> builder, copy, name);

    ctx -> string_map[id] = value;

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
    DeferStack* new_stack = arena_calloc(&ctx -> defer_list.arena, sizeof(DeferStack));

    new_stack -> previous = current_stack;
    new_stack -> ids = arena_calloc(&ctx -> defer_list.arena, sizeof(AstNodeId) * defer_stack_init_cap);
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
