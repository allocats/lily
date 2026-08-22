#ifndef LILY_STRING_INTERNER_TYPES_H
#define LILY_STRING_INTERNER_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

typedef struct {
    str8 str;
    u32 hash;
} StringEntry;

static_assert(sizeof(StringEntry) == 16);

typedef struct {
    StringId id;
    u32 hash;
} StringBucket;

typedef struct {
    Arena arena;

    StringEntry* entries;
    StringBucket* buckets;

    u32 count;

    u32 bucket_capacity;
    u32 entry_capacity;

    u32 resize_threshold_as_u32;
} StringInterner;

#endif // !LILY_STRING_INTERNER_TYPES_H
