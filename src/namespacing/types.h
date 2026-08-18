#ifndef LILY_NAMESPACING_TYPES_H
#define LILY_NAMESPACING_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"

typedef struct {
    StringId* segments;
    u32 hash;
    u16 count;
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
