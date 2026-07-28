#ifndef LILY_STRING_INTERNER_TYPES_H
#define LILY_STRING_INTERNER_TYPES_H

#include "meowrena/meowrena.h"
#include "utils/types.h"

#define STRING_INTERNER_INIT_CAPACITY 256
#define STRING_ID_NONE U32_MAX

typedef u32 StringId;

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
