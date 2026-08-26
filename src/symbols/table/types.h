#ifndef LILY_SYMBOLS_TABLE_TYPES_H
#define LILY_SYMBOLS_TABLE_TYPES_H

#include "meowrena/meowrena.h"
#include "symbols/scope/types.h"
#include "symbols/types.h"

// using two arenas for realloc optimisations (last used pointer extension)
typedef struct {
    // scopes
    Arena scope_array_arena;
    Arena scope_data_arena;

    Scope* scopes;
    u32 scope_count;
    u32 scope_capacity;

    // symbols
    Arena symbol_arena;

    Symbol* symbols;
    u32 symbol_count;
    u32 symbol_capacity;
} SymbolTable;

#endif // !LILY_SYMBOLS_TABLE_TYPES_H
