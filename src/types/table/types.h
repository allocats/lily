#ifndef LILY_TYPES_TABLE_TYPES_H
#define LILY_TYPES_TABLE_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "types/entries/types.h"

typedef struct {
    u32 hash;
    TypeId id;
} TypeBucket;

typedef struct {
    Arena nominal_arena;
    TypeBucket* nominal_buckets;
    u32 nominal_count;
    u32 nominal_capacity;

    Arena structural_arena;
    TypeBucket* structural_buckets;
    u32 structural_count;
    u32 structural_capacity;

    Arena entry_arena;
    TypeEntry* entries;
    u32 entry_count;
    u32 entry_capacity;

    u32 nominal_resize_threshold_as_u32;
    u32 structural_resize_threshold_as_u32;
    u32 entry_resize_threshold_as_u32;
} TypeTable;

#endif // !LILY_TYPES_TABLE_TYPES_H
