#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "modules/modules.h"
#include "modules/types.h"
#include "string_interner/interner.h"
#include "string_interner/types.h"
#include "symbols/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <stdio.h>

#define LOAD_FACTOR 0.75

extern LilyCtx driver_ctx;

static SymbolId scope_add_sym(Resolver* r, StringId name, SymbolKind kind);
static SymbolId scope_get_sym(Resolver* r, StringId name, u32 hash);

static SymbolId table_get_sym(Resolver* r, StringId name);

static ScopeId scope_enter(SymbolTable* table);
static ScopeId scope_exit(SymbolTable* table);

static void scope_resize(Scope* scope);

static void table_symbols_resize(SymbolTable* table);

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

        SymbolId sym_id = SYMBOL_ID_NONE;
        StringId name = STRING_ID_NONE;

        switch (node -> kind) {
            case AST_FUNCTION:
                name = node -> as.func_decl.name_id;
                sym_id = scope_get_sym(&r, name, hash_fnv1a_u32(name));

                if (sym_id != SCOPE_ID_NONE) {
                    str8 str = STRING_ID_LOOKUP(name).str;
                    printf("Already found function %.*s\n", str.length, str.pointer);
                }


                sym_id = scope_add_sym(&r, node -> as.func_decl.name_id, SYM_FUNCTION);
                break;

            case AST_STRUCT:
                name = node -> as.struct_decl.name_id;
                sym_id = scope_get_sym(&r, name, hash_fnv1a_u32(name));

                if (sym_id != SCOPE_ID_NONE) {
                    str8 str = STRING_ID_LOOKUP(name).str;
                    printf("Already found struct %.*s\n", str.length, str.pointer);
                }

                sym_id = scope_add_sym(&r, node -> as.struct_decl.name_id, SYM_TYPE);
                break;

            case AST_ENUM:
                name = node -> as.enum_decl.name_id;
                sym_id = scope_get_sym(&r, name, hash_fnv1a_u32(name));

                if (sym_id != SCOPE_ID_NONE) {
                    str8 str = STRING_ID_LOOKUP(name).str;
                    printf("Already found enum %.*s\n", str.length, str.pointer);
                }

                sym_id = scope_add_sym(&r, node -> as.enum_decl.name_id, SYM_FUNCTION);
                break;

            case AST_CONST:
                StringId name = node -> as.const_decl.name_id;
                sym_id = scope_get_sym(&r, name, hash_fnv1a_u32(name));

                if (sym_id != SCOPE_ID_NONE) {
                    str8 str = STRING_ID_LOOKUP(name).str;
                    printf("Already found const %.*s\n", str.length, str.pointer);
                }

                sym_id = scope_add_sym(&r, node -> as.const_decl.name_id, SYM_FUNCTION);
                break;

            default:
                // printf("Not yet implemented\n");
                break;
        }
    }
}

static SymbolId scope_add_sym(Resolver* r, StringId name, SymbolKind kind) {
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

    sym -> name = name;
    sym -> scope = scope_id;
    sym -> kind = kind;
    sym -> id = sym_id;

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

static SymbolId table_get_sym(Resolver* r, StringId name) {
    i32 scope_id = r -> current_scope_id;

    u32 hash = hash_fnv1a_u32(name);

    while (scope_id >= 0) {
        SymbolId id = scope_get_sym(r, name, hash);

        if (id != SCOPE_ID_NONE) return id;

        scope_id--;
    }

    return SCOPE_ID_NONE;
}

static ScopeId scope_enter(SymbolTable* table) {
    table -> scope_count++;

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

static ScopeId scope_exit(SymbolTable* table) {
    assert(table -> scope_count > 0);

    ScopeId id = table -> scope_count--;
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
