#ifndef LILY_TYPES_TYPES_H
#define LILY_TYPES_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "types/builtins.h"

typedef struct {
    StringId name;
    u32 hash;

    u32 size;
    u32 align;
} TypeEntry;

typedef struct {
    #define X(id, name, size, align) TypeId type_##id;
        BUILTIN_TYPES(X)
    #undef X
} TypeBuiltinIds;

typedef struct {
    Arena arena;

    // primitives, enums, unions, and structs -> uses stringid
    TypeId* nominal_buckets;

    // arrays and pointers -> going to have to create some sort of shape hashing
    TypeId* structural_buckets;

    TypeEntry* entries;

    u32 count;

    u32 nominal_bucket_capacity;
    u32 structural_bucket_capacity;
    u32 entry_capacity;

    TypeBuiltinIds builtins;
} TypeTable;

#endif // !LILY_TYPES_TYPES_H
