#include "driver/types.h"
#include "ids.h"
#include "symbols/scope/scope.h"
#include "symbols/scope/types.h"
#include "token/types.h"

extern DriverCtx driver;

void scope_init(ScopeId id) {
    Scope* scope = &driver.symbol_table.scopes[id];
    Arena* arena = &driver.symbol_table.scope_data_arena;

    scope -> buckets = arena_alloc(arena, sizeof(ScopeBucket) * scope_init_capacity);
    scope -> entries = arena_alloc(arena, sizeof(SymbolId) * scope_init_capacity);
    scope -> parent = SCOPE_ID_NONE;
    scope -> count = 0;
    scope -> capacity = scope_init_capacity;
}
