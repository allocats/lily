#include "driver/types.h"
#include "ids.h"
#include "symbols/register/register.h"
#include "symbols/scope/scope.h"
#include "symbols/scope/types.h"
#include "symbols/table/table.h"
#include "symbols/table/types.h"
#include "symbols/symbols/types.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>

extern DriverCtx driver;

void symbol_table_init(u32 count) {
    SymbolTable* table = &driver.symbol_table;

    const u32 symbols_init_capacity = NEXT_POWER_OF_TWO(count + BUILTIN_NOMINAL_TYPES_COUNT);
    const u64 symbols_init_arena_size_in_bytes = sizeof(Symbol) * symbols_init_capacity;

    debug_printf("Allocating symbol table for %u symbols", symbols_init_capacity);

    arena_init(&table -> symbol_array_arena, symbols_init_arena_size_in_bytes, ALIGN_DEFAULT);
    debug_printf("Init Symbol Table's symbols arena with %lu bytes", symbols_init_arena_size_in_bytes);

    table -> symbols = arena_alloc(&table -> symbol_array_arena, symbols_init_arena_size_in_bytes);
    table -> symbol_count = 0;
    table -> symbol_capacity = symbols_init_capacity;

    arena_init(&table -> symbol_data_arena, ARENA_KB(1), ALIGN_DEFAULT);
    debug_printf("Init Symbol Table's symbols data arena with 1KB");

    debug_printf("Allocated Symbol Table's symbols array with %lu bytes", symbols_init_arena_size_in_bytes);

    const u32 scopes_init_capacity = NEXT_POWER_OF_TWO(driver.file_interner.count * 4);
    const u64 scopes_init_arena_size_in_bytes = sizeof(Scope) * scopes_init_capacity;

    arena_init(&table -> scope_array_arena, scopes_init_arena_size_in_bytes, ALIGN_DEFAULT);
    debug_printf("Init Symbol Table's scope array arena with %lu bytes", scopes_init_arena_size_in_bytes);

    const u64 data_init_arena_size_in_bytes = scopes_init_capacity * scope_init_capacity * (sizeof(ScopeBucket) + sizeof(SymbolId));

    arena_init(&table -> scope_data_arena, data_init_arena_size_in_bytes, ALIGN_DEFAULT);
    debug_printf("Init Symbol Table's scope data arena with %lu bytes", data_init_arena_size_in_bytes);

    table -> scopes = arena_alloc(&table -> scope_array_arena, scopes_init_arena_size_in_bytes);
    table -> scope_count = 1; // scope 0 is reserved for the compiler scope (builtins)
    table -> scope_capacity = scopes_init_capacity;

    debug_printf("Allocated Symbol Table's scopes array with %lu bytes", scopes_init_arena_size_in_bytes);

    for (u32 i = 0; i < scopes_init_capacity; i++) {
        scope_init(i);
    }

    symbols_register_builtin_types();
}

inline ScopeId symbol_table_alloc_scope(void) {
    SymbolTable* table = &driver.symbol_table;
    
    if (UNLIKELY(table -> scope_count >= table -> scope_capacity)) {
        u64 old_size = sizeof(Scope) * table -> scope_capacity;
        u64 new_size = old_size * 2;

        table -> scopes = arena_realloc(&table -> scope_array_arena, table -> scopes, old_size, new_size);
        table -> scope_capacity *= 2;

        for (u32 i = table -> scope_count; i < table -> scope_capacity; i++) {
            scope_init(i);
        }
    }

    return table -> scope_count++;
}

inline SymbolId symbol_table_alloc_symbol(void) { 
    SymbolTable* table = &driver.symbol_table;
    
    if (UNLIKELY(table -> symbol_count >= table -> symbol_capacity)) {
        u64 old_size = sizeof(Symbol) * table -> symbol_capacity;
        u64 new_size = old_size * 2;

        table -> symbols = arena_realloc(&table -> symbol_array_arena, table -> symbols, old_size, new_size);
        table -> symbol_capacity *= 2;

        debug_printf("SymbolTable -> symbols realloc from %lu to %lu bytes", old_size, new_size);
    }

    return table -> symbol_count++;
}

SymbolId symbol_table_lookup_top_level(ScopeId scope_id, StringId name_id) {
    SymbolId id = scope_lookup(scope_id, name_id);

    if (id != SYMBOL_ID_NONE) {
        return id;
    }

    Scope* scope = SCOPE_ID_LOOKUP_REF(scope_id);
    scope_id = scope -> parent;

    while (scope_id != SCOPE_ID_NONE) {
        scope = SCOPE_ID_LOOKUP_REF(scope_id);
        id = scope_lookup(scope_id, name_id);

        if (id != SYMBOL_ID_NONE) {
            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

            if (symbol -> kind != SYMBOL_IMPORT) {
                return id;
            }
        }

        scope_id = scope -> parent;
    }

    return SYMBOL_ID_NONE;
}

SymbolId symbol_table_lookup(ScopeId scope_id, StringId name_id, FileId file_id) {
    while (scope_id != SCOPE_ID_NONE) {
        Scope* scope = SCOPE_ID_LOOKUP_REF(scope_id);
        SymbolId id = scope_lookup(scope_id, name_id);

        if (id != SYMBOL_ID_NONE) {
            Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

            // import bindings are only visible to the file that declared them
            if (symbol -> kind != SYMBOL_IMPORT || symbol -> file_id == file_id) {
                return id;
            }
        }

        scope_id = scope -> parent;
    }

    return SYMBOL_ID_NONE;
}
