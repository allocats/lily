#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "modules/modules.h"
#include "modules/types.h"
#include "string_interner/types.h"
#include "symbols/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>

#define LOAD_FACTOR 0.75

extern LilyCtx driver_ctx;

static SymbolId scope_add_sym(Resolver* r, AstNodeId node_id, StringId name, SymbolKind kind);
static SymbolId scope_get_sym(Resolver* r, StringId name, u32 hash);
static SymbolId scope_get_sym_scope_id(Resolver* r, ScopeId scope_id, StringId name, u32 hash);

static SymbolId table_get_sym(Resolver* r, StringId name);

static ScopeId scope_enter(Resolver* r);
static ScopeId scope_exit(Resolver* r);

static void scope_resize(Scope* scope);
static void table_symbols_resize(SymbolTable* table);

static void sym_add_func(Resolver* r, AstNode* node, AstNodeId node_id);
static void sym_add_struct(Resolver* r, AstNode* node, AstNodeId node_id);
static void sym_add_union(Resolver* r, AstNode* node, AstNodeId node_id);
static void sym_add_enum(Resolver* r, AstNode* node, AstNodeId node_id);
static void sym_add_const(Resolver* r, AstNode* node, AstNodeId node_id);

void scope_init(Scope* scope) {
    arena_init(&scope -> arena, 512, ALIGN_8);
    debug_printf("Module Registry: Init module's scope arena with 512B\n");

    scope -> str_ids = arena_alloc_array(&scope -> arena, StringId, 32);
    scope -> ids = arena_alloc_array(&scope -> arena, SymbolId, 32);
    scope -> count = 0;
    scope -> capacity = 32;

    arena_memset(scope -> ids, 0xff, 32 * sizeof(SymbolId));
}

void symbols_register(ModuleId id) {
    Module* module = MODULE_ID_LOOKUP_REF(id);
    Ast* ast = &module -> ast;
    u32 count = ast -> count;

    Resolver r = {
        .current_module_id = id,
        .current_scope_id = 0,
        .table = &module -> symbol_table
    };

    for (u32 i = 0; i < count; i++) {
        AstNode* node = &ast -> nodes[i];

        switch (node -> kind) {
            case AST_FUNCTION:
                sym_add_func(&r, node, i);
                break;

            case AST_STRUCT:
                sym_add_struct(&r, node, i);
                break;

            case AST_UNION:
                sym_add_union(&r, node, i);
                break;

            case AST_ENUM:
                sym_add_enum(&r, node, i);
                break;

            case AST_CONST:
                sym_add_const(&r, node, i);
                break;

            default:
                break;
        }
    }
}

static SymbolId scope_add_sym(Resolver* r, AstNodeId node_id, StringId name, SymbolKind kind) {
    i32 scope_id = r -> current_scope_id;

    Scope* scope = &r -> table-> scopes[scope_id];

    u32 hash  = hash_fnv1a_u32(name);
    u32 mask  = scope -> capacity - 1;
    u32 index = hash & mask; 

    while (scope -> ids[index] != SYMBOL_ID_NONE) {
        if (scope -> str_ids[index] == name) {
            return scope -> ids[index];
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(scope -> count >= scope -> capacity * LOAD_FACTOR)) {
        scope_resize(scope);
        index = (index + 1) & (scope -> capacity - 1);
    }

    if (UNLIKELY(r -> table -> symbol_count >= r -> table -> symbol_capacity)) {
        table_symbols_resize(r -> table);
    }

    SymbolId sym_id = r -> table -> symbol_count++;
    Symbol* sym = &r -> table -> symbols[sym_id];

    scope -> ids[index] = sym_id;
    scope -> str_ids[index] = name;

    sym -> declaration = node_id;
    sym -> name = name;
    sym -> scope = scope_id;
    sym -> kind = kind;
    sym -> id = sym_id;

    debug_printf("Symbols: Added symbol %d to scope %d\n", sym_id, scope_id);

    return sym_id;
}

static SymbolId scope_get_sym(Resolver* r, StringId name, u32 hash) {
    i32 scope_id = r -> current_scope_id;

    Scope* scope = &r -> table-> scopes[scope_id];

    u32 mask  = scope -> capacity - 1;
    u32 index = hash & mask; 

    while (scope -> ids[index] != SYMBOL_ID_NONE) {
        if (scope -> str_ids[index] == name) {
            return scope -> ids[index];
        }

        index = (index + 1) & mask;
    }

    return SYMBOL_ID_NONE;
}

static SymbolId scope_get_sym_scope_id(Resolver* r, ScopeId scope_id, StringId name, u32 hash) {
    Scope* scope = &r -> table-> scopes[scope_id];

    u32 mask  = scope -> capacity - 1;
    u32 index = hash & mask; 

    while (scope -> ids[index] != SYMBOL_ID_NONE) {
        if (scope -> str_ids[index] == name) {
            return scope -> ids[index];
        }

        index = (index + 1) & mask;
    }

    return SYMBOL_ID_NONE;
}

static SymbolId table_get_sym(Resolver* r, StringId name) {
    i32 scope_id = r -> current_scope_id;

    u32 hash = hash_fnv1a_u32(name);

    while (scope_id >= 0) {
        SymbolId id = scope_get_sym_scope_id(r, scope_id, name, hash);

        if (id != SYMBOL_ID_NONE) return id;

        scope_id--;
    }

    return SYMBOL_ID_NONE;
}

static ScopeId scope_enter(Resolver* r) {
    SymbolTable* table = r -> table;
    table -> scope_count++;
    r -> current_scope_id++;

    if (UNLIKELY(table -> scope_count >= table -> scope_capacity)) {
        u64 old_size = table -> scope_capacity * sizeof(Scope);
        u64 new_size = old_size * 2;

        table -> scopes = arena_realloc(&table -> arena, table -> scopes, old_size, new_size);
        table -> scope_capacity *= 2;

        debug_printf("Symbol Table: Realloc scopes array from %ld -> %ld bytes\n", old_size, new_size);

        for (u32 i = table -> scope_count; i < table -> scope_capacity; i++) {
            Scope* scope = &table -> scopes[i];
            scope_init(scope);
        }
    }

    return table -> scope_count;
}

static ScopeId scope_exit(Resolver* r) {
    SymbolTable* table = r -> table;
    assert(table -> scope_count > 0);

    ScopeId id = table -> scope_count--;
    r -> current_scope_id--;

    Scope* scope = &table -> scopes[id];

    scope -> count = 0;

    arena_reset(&scope -> arena);
    arena_memset(scope -> ids, 0xff, sizeof(SymbolId) * scope -> capacity);

    return id;
}


static void scope_resize(Scope* scope) {
    u32 old_cap = scope->capacity;
    u32 new_cap = old_cap * 2;

    StringId* new_str_ids = arena_alloc_array(&scope->arena, StringId, new_cap);
    SymbolId* new_ids = arena_alloc_array(&scope->arena, SymbolId, new_cap);

    arena_memset(new_ids, 0xff, new_cap * sizeof(SymbolId));

    u32 new_mask = new_cap - 1;

    for (u32 i = 0; i < old_cap; i++) {
        SymbolId id = scope->ids[i];

        if (id == SYMBOL_ID_NONE) continue;

        StringId str_id = scope->str_ids[i];

        u32 hash = hash_fnv1a_u32(str_id);
        u32 index = hash & new_mask;

        while (new_ids[index] != SYMBOL_ID_NONE) {
            index = (index + 1) & new_mask;
        }

        new_ids[index] = id;
        new_str_ids[index] = str_id;
    }

    scope -> ids = new_ids;
    scope -> str_ids = new_str_ids;
    scope -> capacity = new_cap;

    debug_printf("Scope: Resized symbol table %u -> %u entries\n", old_cap, new_cap);
}

static void table_symbols_resize(SymbolTable* table) {
    u64 old_size = table -> symbol_capacity * sizeof(Symbol);
    u64 new_size = 2;

    table -> symbols = arena_realloc(&table -> arena, table -> symbols, old_size, new_size);
    table -> symbol_capacity *= 2;
}

static void sym_add_func(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.func_decl.name_id;
    SymbolId sym_id = scope_get_sym(r, name, hash_fnv1a_u32(name));

    if (sym_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            sym_id,
            node_id
        );

        return;
    }

    sym_id = scope_add_sym(r, node_id, node -> as.func_decl.name_id, SYM_FUNCTION);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 param_count = node -> as.func_decl.param_count;

    if (param_count == 0) {
        symbol -> as.function.count = 0;
        return;
    }

    scope_enter(r);

    symbol -> as.function.params = arena_alloc(&r -> table -> arena, param_count * sizeof(SymbolId));
    symbol -> as.function.count = param_count;

    Ast* ast = &module -> ast;

    for (u32 i = 0; i < param_count; i++) {
        AstNodeId param_node_id = node -> as.func_decl.params[i];
        AstNode* param_node = &ast -> nodes[param_node_id];

        SymbolId param_id = table_get_sym(
            r,
            param_node -> as.param_decl.name_id
        );

        if (param_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                param_id,
                param_node_id
            );

            continue;
        }
        
        symbol -> as.function.params[i] = scope_add_sym(
            r,
            param_node_id,
            param_node -> as.param_decl.name_id,
            SYM_PARAMETER
        );
    }

    scope_exit(r);
}

static void sym_add_struct(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.struct_decl.name_id;
    SymbolId sym_id = scope_get_sym(r, name, hash_fnv1a_u32(name));

    if (sym_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            sym_id,
            node_id
        );

        return;
    }

    sym_id = scope_add_sym(r, node_id, node -> as.struct_decl.name_id, SYM_STRUCT);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 field_count = node -> as.struct_decl.field_count;

    if (field_count == 0) {
        symbol -> as.structs.count = 0;
        return;
    }

    scope_enter(r);

    symbol -> as.structs.fields = arena_alloc(&r -> table -> arena, field_count * sizeof(SymbolId));
    symbol -> as.structs.count = field_count;

    Ast* ast = &module -> ast;

    for (u32 i = 0; i < field_count; i++) {
        AstNodeId field_node_id = node -> as.struct_decl.fields[i];
        AstNode* field_node = &ast -> nodes[field_node_id];

        SymbolId field_id = scope_get_sym(
            r,
            field_node -> as.field_decl.name_id,
            hash_fnv1a_u32(field_node -> as.field_decl.name_id)
        );

        if (field_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                field_id,
                field_node_id
            );

            continue;
        }
        
        symbol -> as.structs.fields[i] = scope_add_sym(
            r,
            field_node_id,
            field_node -> as.field_decl.name_id,
            SYM_FIELD
        );
    }

    scope_exit(r);
}

static void sym_add_union(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.union_decl.name_id;
    SymbolId sym_id = scope_get_sym(r, name, hash_fnv1a_u32(name));

    if (sym_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            sym_id,
            node_id
        );

        return;
    }

    sym_id = scope_add_sym(r, node_id, node -> as.union_decl.name_id, SYM_UNION);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 field_count = node -> as.union_decl.field_count;

    if (field_count == 0) {
        symbol -> as.unions.count = 0;
        return;
    }

    scope_enter(r);

    symbol -> as.unions.fields = arena_alloc(&r -> table -> arena, field_count * sizeof(SymbolId));
    symbol -> as.unions.count = field_count;

    Ast* ast = &module -> ast;

    for (u32 i = 0; i < field_count; i++) {
        AstNodeId field_node_id = node -> as.union_decl.fields[i];
        AstNode* field_node = &ast -> nodes[field_node_id];

        SymbolId field_id = scope_get_sym(
            r,
            field_node -> as.field_decl.name_id,
            hash_fnv1a_u32(field_node -> as.field_decl.name_id)
        );

        if (field_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                field_id,
                field_node_id
            );

            continue;
        }
        
        symbol -> as.unions.fields[i] = scope_add_sym(
            r,
            field_node_id,
            field_node -> as.field_decl.name_id,
            SYM_FIELD
        );
    }

    scope_exit(r);
}

static void sym_add_enum(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.enum_decl.name_id;
    SymbolId sym_id = scope_get_sym(r, name, hash_fnv1a_u32(name));

    if (sym_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            sym_id,
            node_id
        );

        return;
    }

    sym_id = scope_add_sym(r, node_id, node -> as.enum_decl.name_id, SYM_ENUM);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 variant_count = node -> as.enum_decl.variant_count;

    if (variant_count == 0) {
        symbol -> as.enums.count = 0;
        return;
    }

    scope_enter(r);

    symbol -> as.enums.variants = arena_alloc(&r -> table -> arena, variant_count * sizeof(SymbolId));
    symbol -> as.enums.count = variant_count;

    Ast* ast = &module -> ast;

    for (u32 i = 0; i < variant_count; i++) {
        AstNodeId variant_node_id = node -> as.enum_decl.variants[i];
        AstNode* variant_node = &ast -> nodes[variant_node_id];

        SymbolId variant_id = scope_get_sym(
            r,
            variant_node -> as.variant_decl.name_id,
            hash_fnv1a_u32(variant_node -> as.variant_decl.name_id)
        );

        if (variant_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                variant_id,
                variant_node_id
            );

            continue;
        }
        
        symbol -> as.enums.variants[i] = scope_add_sym(
            r,
            variant_node_id,
            variant_node -> as.variant_decl.name_id,
            SYM_VARIANT
        );
    }

    scope_exit(r);
}

static void sym_add_const(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.const_decl.name_id;
    SymbolId sym_id = scope_get_sym(r, name, hash_fnv1a_u32(name));

    if (sym_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            sym_id,
            node_id
        );

        return;
    }

    sym_id = scope_add_sym(r, node_id, node -> as.const_decl.name_id, SYM_CONSTANT);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    symbol -> as.constant.value = node -> as.const_decl.value; 
}
