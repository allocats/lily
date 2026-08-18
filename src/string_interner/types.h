#ifndef LILY_STRING_INTERNER_TYPES_H
#define LILY_STRING_INTERNER_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"

typedef struct {
    str8 str;
    u32  hash;
} StringEntry;

typedef struct {
    Arena arena;

    StringEntry* entries;
    StringId* buckets;

    u32 count;

    u32 bucket_capacity;
    u32 entry_capacity;
} StringInterner;

#endif // !LILY_STRING_INTERNER_TYPES_H
