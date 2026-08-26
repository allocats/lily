#ifndef LILY_SYMBOLS_SCOPE_TYPES_H
#define LILY_SYMBOLS_SCOPE_TYPES_H

#include "ids.h"
#include "utils/types.h"

static constexpr u32 scope_init_capacity = 8;

typedef struct {
    u32 hash; // store hash to not have to touch another array just to cmp hashes
    StringId string_id;
} ScopeBucket;

typedef struct {
    ScopeId parent; // SCOPE_ID_NONE for no parent

    ScopeBucket* buckets;
    SymbolId* entries;
    u32 count;
    u32 capacity; // must be power of two
} Scope;

#endif // !LILY_SYMBOLS_SCOPE_TYPES_H
