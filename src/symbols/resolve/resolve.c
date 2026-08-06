#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "resolver/enums.h"
#include "resolver/resolver.h"
#include "symbols/symbols.h"
#include "symbols/resolve/resolve.h"
#include "symbols/types.h"
#include "types/ty.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <stdio.h>

extern LilyCtx driver_ctx;

#define ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(n) ((n + sizeof(void*) - 1) & (-sizeof(void*)))

static bool resolve_symbol_body(ModuleId module_id, SymbolId id);
static bool resolve_function(Resolver* r, Module* module, SymbolId symbol_id);
static bool resolve_struct(Resolver* r, Module* module, SymbolId symbol_id);
static bool resolve_union(Resolver* r, Module* module, SymbolId symbol_id);
static bool resolve_enum(Resolver* r, Module* module, SymbolId symbol_id);

bool symbols_resolve_by_id(ModuleId module_id, SymbolId id) {
    Module* module = MODULE_ID_LOOKUP_REF(module_id);
    Symbol* symbol = &module -> symbol_table.symbols[id];

    if (symbol -> resolve_state == RESOLVE_RESOLVED) return true;
    if (symbol -> resolve_state == RESOLVE_ERROR) return false;

    ResolveItem resolve_item = {
        .kind = RESOLVE_SYMBOL,
        .module_id = module_id,
        .as.symbol = id
    };

    if (symbol -> resolve_state == RESOLVE_RESOLVING) {
        i32 cycle_start = resolver_stack_find(&driver_ctx.resolver_stack, resolve_item);

        diagnostic_add_resolver_symbol_cycle(
            &driver_ctx.diagnostics,
            cycle_start
        );

        symbol -> resolve_state = RESOLVE_ERROR;
        return false;
    }

    symbol -> resolve_state = RESOLVE_RESOLVING;

    if (!resolver_stack_push(&driver_ctx.resolver_stack, resolve_item)) {
        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_ERROR,
            "reached recursion limit"
        );

        symbol -> resolve_state = RESOLVE_ERROR;
        return null;
    }

    bool resolved_body = resolve_symbol_body(module_id, id);

    resolver_stack_pop(&driver_ctx.resolver_stack);
    
    module -> symbol_table.symbols[id].resolve_state = resolved_body ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return resolved_body;

}

bool resolve_symbol_body(ModuleId module_id, SymbolId id) {
    Module* module = MODULE_ID_LOOKUP_REF(module_id);
    Symbol* symbol = &module -> symbol_table.symbols[id];

    Resolver r = {
        .current_namespace_id = module -> namespace_id,
        .current_module_id = module_id,
        .current_scope_id = 0,
        .builtins = &driver_ctx.builtins,
        .table = &module -> symbol_table
    };

    switch (symbol -> kind) {
        case SYM_FUNCTION:
            return resolve_function(&r, module, id);

        case SYM_STRUCT:
            return resolve_struct(&r, module, id);

        case SYM_UNION:
            return resolve_union(&r, module, id);

        case SYM_ENUM:
            return resolve_enum(&r, module, id);

        default:
            // printf("Hit default case in symbols_resolve_by_id (id=%d)\n", id);
            return false;
    }

    scope_exit(&r);

    return false;
}

bool resolve_struct(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &r -> table -> symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    u32 size = 0;
    u32 align = 0;

    u32 count = node -> as.struct_decl.field_count;
    
    scope_enter(r);

    for (u32 i = 0; i < count; i++) {
        AstNodeId field_id = node -> as.struct_decl.fields[i];
        AstNode* field = &module -> ast.nodes[field_id];

        u32 field_hash = hash_fnv1a_u32(field -> as.field_decl.name_id);
        SymbolId field_sym_id = scope_get_sym(r, field -> as.field_decl.name_id, field_hash);

        if (field_sym_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                field_sym_id,
                field_id
            );

            continue;
        }

        field_sym_id = scope_add_sym(r, field_id, field -> as.field_decl.name_id, SYM_FIELD);

        TypeId field_type = resolve_type(module -> id, field -> as.field_decl.type_expr);
        if (field_type == driver_ctx.type_table.builtins.type_void) {
            printf("Found invalid type: void");
            scope_exit(r);
            return false;
        }

        TypeEntry* entry = resolve_type_entry(module -> id, field_type);
        if (entry == NULL) {
            scope_exit(r);
            return false;
        }

        size += entry -> size;

        Symbol* field_sym = &module -> symbol_table.symbols[field_sym_id];

        field_sym -> as.field.type = field_type;

        module -> symbol_table.symbols[symbol_id].as.structs.fields[i] = field_sym_id;
    }

    scope_exit(r);

    align = ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(size);

    TypeEntry* entry = &driver_ctx.type_table.entries[symbol -> as.structs.type];

    entry -> size = size;
    entry -> align = align;

    return true;
}

bool resolve_union(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &r -> table -> symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    u32 size = 0;
    u32 align = 0;

    u32 count = node -> as.union_decl.field_count;
    
    scope_enter(r);

    for (u32 i = 0; i < count; i++) {
        AstNodeId field_id = node -> as.union_decl.fields[i];
        AstNode* field = &module -> ast.nodes[field_id];

        u32 field_hash = hash_fnv1a_u32(field -> as.field_decl.name_id);
        SymbolId field_sym_id = scope_get_sym(r, field -> as.field_decl.name_id, field_hash);

        if (field_sym_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                field_sym_id,
                field_id
            );

            continue;
        }

        field_sym_id = scope_add_sym(r, field_id, field -> as.field_decl.name_id, SYM_FIELD);

        TypeId field_type = resolve_type(module -> id, field -> as.field_decl.type_expr);
        if (field_type == driver_ctx.type_table.builtins.type_void) {
            printf("Found invalid type: void");
            scope_exit(r);
            return false;
        }

        TypeEntry* entry = resolve_type_entry(module -> id, field_type);
        if (entry == NULL) {
            scope_exit(r);
            return false;
        }

        size = MAX(size, entry -> size);

        Symbol* field_sym = &module -> symbol_table.symbols[field_sym_id];

        field_sym -> as.field.type = field_type;

        module -> symbol_table.symbols[symbol_id].as.unions.fields[i] = field_sym_id;
    }

    scope_exit(r);

    align = ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(size);

    TypeEntry* entry = &driver_ctx.type_table.entries[symbol -> as.unions.type];

    entry -> size = size;
    entry -> align = align;

    return true;
}

bool resolve_enum(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &r -> table -> symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    TypeTable* table = &driver_ctx.type_table;

    TypeId underlying_type = TYPE_ID_NONE;

    if (node -> as.enum_decl.type_expr == AST_NODE_ID_NONE) {
        underlying_type = table -> builtins.type_i32;
    } else {
        underlying_type = resolve_type(module -> id, node -> as.enum_decl.type_expr);

        if (underlying_type == TYPE_ID_NONE) {
            printf("Enum: error");
            return false;
        }
    }

    TypeEntry* type = &table -> entries[underlying_type];
    TypeEntry* sym_type = &table -> entries[symbol -> as.enums.type];

    sym_type -> size = type -> size;
    sym_type -> align = type -> align;

    return true;
}


bool resolve_function(Resolver* r, Module* module, SymbolId symbol_id) {
    Symbol* symbol = &module -> symbol_table.symbols[symbol_id];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    TypeId return_type_id = resolve_type(r -> current_module_id, node -> as.func_decl.return_type_expr);

    if (return_type_id == TYPE_ID_NONE) {
        diagnostic_add_return_type_invalid(
            &driver_ctx.diagnostics,
            module,
            symbol -> id
        );
    }

    module -> symbol_table.symbols[symbol_id].as.function.return_type = return_type_id;

    scope_enter(r);

    for (u32 i = 0; i < node -> as.func_decl.param_count; i++) {
        AstNodeId param_id = node -> as.func_decl.params[i];
        AstNode* param = &module -> ast.nodes[param_id];

        TypeId param_type = resolve_type(r -> current_module_id, param -> as.param_decl.type_expr);

        if (param_type == TYPE_ID_NONE) {
            printf("Invalid type for function parameter!\n");
            continue;
        }

        if (param_type == driver_ctx.type_table.builtins.type_void) {
            printf("Void cannot be used for function parameter!\n");
            continue;
        }

        SymbolId param_symbol_id = table_get_sym(r, param -> as.param_decl.name_id);

        if (param_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                param_symbol_id,
                param_id
            );

            continue;
        }

        param_symbol_id = scope_add_sym(r, param_id, param -> as.param_decl.name_id, SYM_PARAMETER); 

        Symbol* param_symbol = &module -> symbol_table.symbols[param_symbol_id];

        param_symbol -> as.parameter.type = param_type;

        module -> symbol_table.symbols[symbol_id].as.function.params[i] = param_symbol_id;

        debug_printf("Symbols: Added parameter (%d) to function(%d)\n", param_symbol_id, symbol -> id);
    }

    // resolve_block(r, node -> as.func_decl.block);

    scope_exit(r);

    return true;
}
