#ifndef LILY_RESOLVER_STACK_TYPES_H
#define LILY_RESOLVER_STACK_TYPES_H

#include "ids.h"

static constexpr u64 RESOLVER_STACK_MAX = 256;

typedef enum {
    QUERY_SYMBOL,
    QUERY_TYPE,
} ResolveQueryKind;

typedef struct {
    ResolveQueryKind kind;
    FileId file_id;

    union {
        SymbolId symbol;
        TypeId type;
    } as;
} ResolveQuery;

typedef struct {
    ResolveQuery items[RESOLVER_STACK_MAX];
    u32 top;
} ResolverStack;

#endif // !LILY_RESOLVER_STACK_TYPES_H
