#ifndef LILY_NAMESPACING_TYPES_H
#define LILY_NAMESPACING_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"

#define NAMESPACE_INTERNER_INIT_CAPACITY 32
#define NAMESPACE_MAX_DEPTH 8

typedef struct {
    StringId segments[NAMESPACE_MAX_DEPTH];
    u32 count;
    u32 hash;

    bool defined;
} NamespaceEntry;

typedef struct {
    NamespaceEntry* entries;
    NamespaceId* buckets;

    u32 count;

    u32 bucket_capacity;
    u32 entry_capacity;

    Arena arena;
} NamespaceInterner;

#endif // !LILY_NAMESPACING_TYPES_H
