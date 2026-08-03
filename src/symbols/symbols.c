#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "query/query.h"
#include "symbols/register/register.h"
#include "symbols/symbols.h"
#include "types/ty.h"
#include "types/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <stdio.h>

#define LOAD_FACTOR 0.75

#define ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(n) ((n + sizeof(void*) - 1) & (-sizeof(void*)))

static void scope_resize(Scope* scope);
static void table_symbols_resize(SymbolTable* table);

static void register_symbol(Resolver* r, AstNode* node, AstNodeId node_id);
static void resolve_symbol(Resolver* r, Symbol* sym);
static bool resolve_symbol_body(ModuleId module_id, SymbolId id);

static bool resolve_struct_body(Module* module, Symbol* symbol);
static bool resolve_union_body(Module* module, Symbol* symbol);
static bool resolve_enum_body(Module* module, Symbol* symbol);

void symbol_table_builtins_init(void) {
    SymbolTable* table = &driver_ctx.builtins;

    arena_init(&table -> arena, ARENA_KB(2), ALIGN_8);
    debug_printf("Driver: Init builtins' symbol table arena with 2KB\n");

    table -> symbols = arena_alloc_array(&table -> arena, Symbol, 32);
    table -> symbol_count = 0;
    table -> symbol_capacity = 32;

    table -> scopes = arena_alloc(&table -> arena, sizeof(Scope) * 4);
    table -> scope_count = 0;
    table -> scope_capacity = 4;

    for (u32 i = 0; i < table -> scope_capacity; i++) {
        scope_init(&table -> scopes[i]);
    }
}

SymbolId builtins_register_type(TypeId id) {
    TypeTable* types = &driver_ctx.type_table;
    SymbolTable* table = &driver_ctx.builtins;

    TypeEntry* type = &types -> entries[id];

    Scope* scope = &table -> scopes[0];

    if (UNLIKELY(scope -> count >= scope -> capacity * LOAD_FACTOR)) {
        scope_resize(scope);
    }

    u32 hash  = type -> hash;
    u32 mask  = scope -> capacity - 1;
    u32 index = hash & mask;

    while (scope -> ids[index] != SYMBOL_ID_NONE) {
        if (scope -> str_ids[index] == type -> name) {
            return scope -> ids[index];
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table -> symbol_count >= table -> symbol_capacity)) {
        table_symbols_resize(table);
    }

    SymbolId sym_id = table -> symbol_count++;

    scope -> ids[index] = sym_id;
    scope -> str_ids[index] = type -> name;

    Symbol* symbol = &table -> symbols[sym_id];

    symbol -> kind = SYM_TYPE;

    symbol -> id = sym_id;
    symbol -> name = type -> name;
    symbol -> scope = 0;
    symbol -> as.type = id;

    return sym_id;
}

void scope_init(Scope* scope) {
    arena_init(&scope -> arena, 512, ALIGN_8);
    debug_printf("Module Registry: Init module's scope arena with 512B\n");

    scope -> str_ids = arena_alloc_array(&scope -> arena, StringId, 32);
    scope -> ids = arena_alloc_array(&scope -> arena, SymbolId, 32);
    scope -> count = 0;
    scope -> capacity = 32;

    arena_memset(scope -> ids, 0xff, 32 * sizeof(SymbolId));
}

void symbols_register_top_level_declarations(ModuleId id) {
    Module* module = MODULE_ID_LOOKUP_REF(id);
    Ast* ast = &module -> ast;
    u32 count = ast -> count;

    Resolver r = {
        .current_namespace_id = module -> namespace_id,
        .current_module_id = id,
        .current_scope_id = 0,
        .builtins = &driver_ctx.builtins,
        .table = &module -> symbol_table
    };

    for (u32 i = 0; i < count; i++) {
        AstNode* node = &ast -> nodes[i];

        register_symbol(&r, node, i);
    }
}

void symbols_resolve(ModuleId id) {
    Module* module = MODULE_ID_LOOKUP_REF(id);
    u32 count = module -> symbol_table.symbol_count;

    Resolver r = {
        .current_namespace_id = module -> namespace_id,
        .current_module_id = id,
        .current_scope_id = 0,
        .builtins = &driver_ctx.builtins,
        .table = &module -> symbol_table
    };

    for (u32 i = 0; i < count; i++) {
        Symbol* sym = &r.table -> symbols[i];

        resolve_symbol(&r, sym);
    }
}

static void register_symbol(Resolver* r, AstNode* node, AstNodeId node_id) {
    switch (node -> kind) {
        case AST_FUNCTION:
            // resolve_type(r -> current_module_id, node -> as.func_decl.return_type_expr);
            sym_register_function(r, node, node_id);
            break;

        case AST_MACRO:
            sym_register_macro(r, node, node_id);
            break;

        case AST_STRUCT:
            sym_register_struct(r, node, node_id);
            break;

        case AST_UNION:
            sym_register_union(r, node, node_id);
            break;

        case AST_CONST:
            sym_register_constant(r, node, node_id);
            break;

        case AST_ENUM:
            sym_register_enum(r, node, node_id);
            break;

        default:
            break;
    }
}

static void resolve_symbol(Resolver* r, Symbol* sym) {
    switch (sym -> kind) {
        case SYM_FUNCTION:
            break;

        case SYM_MACRO:
            break;

        case SYM_CONSTANT:
            break;

        case SYM_STRUCT:
            break;

        case SYM_UNION:
            break;

        case SYM_ENUM:
            break;

        default:
            break;
    }
}

bool symbols_resolve_by_id(ModuleId module_id, SymbolId id) {
    Module* module = MODULE_ID_LOOKUP_REF(module_id);
    Symbol* symbol = &module -> symbol_table.symbols[id];

    if (symbol -> resolve_state == RESOLVE_RESOLVED) return true;
    if (symbol -> resolve_state == RESOLVE_ERROR) return false;

    Query query = {
        .kind = QUERY_SYMBOL,
        .module_id = module_id,
        .as.symbol_id = id
    };

    if (symbol -> resolve_state == RESOLVE_RESOLVING) {
        i32 cycle_start = query_stack_find(&driver_ctx.query_stack, query);

        diagnostic_add_query_symbol_cycle(
            &driver_ctx.diagnostics,
            cycle_start
        );

        symbol -> resolve_state = RESOLVE_ERROR;
        return false;
    }

    symbol -> resolve_state = RESOLVE_RESOLVING;

    if (!query_stack_push(&driver_ctx.query_stack, query)) {
        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_ERROR,
            "reached recursion limit"
        );

        symbol -> resolve_state = RESOLVE_ERROR;
        return null;
    }

    bool resolved_body = resolve_symbol_body(module_id, id);

    query_stack_pop(&driver_ctx.query_stack);
    
    symbol -> resolve_state = resolved_body ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return resolved_body;

}

static bool resolve_symbol_body(ModuleId module_id, SymbolId id) {
    Module* module = MODULE_ID_LOOKUP_REF(module_id);
    Symbol* symbol = &module -> symbol_table.symbols[id];

    switch (symbol -> kind) {
        case SYM_STRUCT:
            return resolve_struct_body(module, symbol);

        case SYM_UNION:
            return resolve_union_body(module, symbol);
            break;

        case SYM_ENUM:
            return resolve_enum_body(module, symbol);
            break;

        default:
            printf("Something goofed up here\n Id = %d\n", id);
            return false;
    }

    return false;
}

static bool resolve_struct_body(Module* module, Symbol* symbol) {
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    u32 size = 0;
    u32 align = 0;

    u32 count = node -> as.struct_decl.field_count;

    for (u32 i = 0; i < count; i++) {
        AstNodeId field_id = node -> as.struct_decl.fields[i];
        AstNode* field = &module -> ast.nodes[field_id];

        TypeId field_type = resolve_type(module -> id, field -> as.field_decl.type_expr);

        if (field_type == driver_ctx.type_table.builtins.type_void) {
            printf("Found invalid type: void");
            return false;
        }

        TypeEntry* entry = resolve_type_entry(module -> id, field_type);
        if (entry == NULL) {
            return false;
        }

        size += entry -> size;

        symbol -> as.structs.fields[i] = field -> as.field_decl.name_id;
        symbol -> as.structs.types[i] = field_type;
    }

    align = ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(size);

    TypeEntry* entry = &driver_ctx.type_table.entries[symbol -> as.structs.type_id];

    entry -> size = size;
    entry -> align = align;

    return true;
}

static bool resolve_union_body(Module* module, Symbol* symbol) {
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    u32 size = 0;
    u32 align = 0;

    u32 count = node -> as.union_decl.field_count;

    for (u32 i = 0; i < count; i++) {
        AstNodeId field_id = node -> as.union_decl.fields[i];
        AstNode* field = &module -> ast.nodes[field_id];

        TypeId field_type = resolve_type(module -> id, field -> as.field_decl.type_expr);

        if (field_type == driver_ctx.type_table.builtins.type_void) {
            printf("Found invalid type: void");
            return false;
        }

        TypeEntry* entry = resolve_type_entry(module -> id, field_type);
        if (entry == NULL) {
            return false;
        }

        size += entry -> size;

        symbol -> as.unions.fields[i] = field -> as.field_decl.name_id;
        symbol -> as.unions.types[i] = field_type;
    }

    align = ALIGN_TO_NEXT_MULTIPLE_OF_POINTER_SIZE(size);

    TypeEntry* entry = &driver_ctx.type_table.entries[symbol -> as.unions.type_id];

    entry -> size = size;
    entry -> align = align;

    return true;
}

static bool resolve_enum_body(Module* module, Symbol* symbol) {
    TypeTable* table = &driver_ctx.type_table;
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

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
    TypeEntry* sym_type = &table -> entries[symbol -> as.enums.type_id];

    sym_type -> size = type -> size;
    sym_type -> align = type -> align;

    return true;
}

SymbolId scope_add_sym(Resolver* r, AstNodeId node_id, StringId name, SymbolKind kind) {
    i32 scope_id = r -> current_scope_id;

    Scope* scope = &r -> table-> scopes[scope_id];

    if (UNLIKELY(scope -> count >= scope -> capacity * LOAD_FACTOR)) {
        scope_resize(scope);
    }

    u32 hash  = hash_fnv1a_u32(name);
    u32 mask  = scope -> capacity - 1;
    u32 index = hash & mask; 

    while (scope -> ids[index] != SYMBOL_ID_NONE) {
        if (scope -> str_ids[index] == name) {
            return scope -> ids[index];
        }

        index = (index + 1) & mask;
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

SymbolId scope_get_sym(Resolver* r, StringId name, u32 hash) {
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

SymbolId builtins_get_sym(Resolver* r, StringId name, u32 hash) {
    Scope* scope = r -> builtins -> scopes;

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

SymbolId scope_get_sym_scope_id(Resolver* r, ScopeId scope_id, StringId name, u32 hash) {
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

SymbolId table_get_sym(Resolver* r, StringId name) {
    i32 scope_id = r -> current_scope_id;

    u32 hash = hash_fnv1a_u32(name);

    while (scope_id >= SCOPE_ID_NONE) {
        SymbolId id = scope_get_sym_scope_id(r, scope_id, name, hash);

        if (id != SYMBOL_ID_NONE) return id;

        scope_id = r -> table -> scopes[scope_id].parent;
    }

    return SYMBOL_ID_NONE;
}

ScopeId scope_enter(Resolver* r) {
    SymbolTable* table = r -> table;

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

    ScopeId new_scope = table->scope_count++;
    Scope* scope = &table->scopes[new_scope];

    scope -> parent = r -> current_scope_id;
    scope -> count = 0;

    arena_memset(scope -> ids, 0xff, sizeof(SymbolId) * scope -> capacity);

    r -> current_scope_id = new_scope;

    return new_scope;
}

ScopeId scope_exit(Resolver* r) {
    ScopeId current = r -> current_scope_id;
    assert(current != SCOPE_ID_NONE);

    Scope* scope = &r -> table -> scopes[current];
    r -> current_scope_id = scope -> parent;
    return current;
}

static void scope_resize(Scope* scope) {
    u32 old_cap = scope->capacity;
    u32 new_cap = old_cap * 2;

    StringId* new_str_ids = arena_alloc_array(&scope->arena, StringId, new_cap);
    SymbolId* new_ids = arena_alloc_array(&scope->arena, SymbolId, new_cap);

    arena_memset(new_ids, 0xff, new_cap * sizeof(SymbolId));

    u32 new_mask = new_cap - 1;

    for (u32 i = 0; i < old_cap; i++) {
        SymbolId id = scope -> ids[i];

        if (id == SYMBOL_ID_NONE) continue;

        StringId str_id = scope -> str_ids[i];

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
