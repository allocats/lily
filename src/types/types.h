#ifndef LILY_TYPES_TYPES_H
#define LILY_TYPES_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "resolver/enums.h"
#include "symbols/types.h"
#include "types/builtins.h"
#include "utils/types.h"

#define TYPE_KINDS(X)   \
    X(TYPE_BASE)        \
    X(TYPE_POINTER)     \
    X(TYPE_ARRAY)       \
    X(TYPE_STRUCT)      \
    X(TYPE_UNION)       \
    X(TYPE_ENUM)        \

typedef enum {
    TYPE_KINDS(GENERATE_ENUM)
} TypeKind;

static const char* TYPE_KIND_STRINGS[] = {
    TYPE_KINDS(GENERATE_STRING)
};

#undef TYPE_KINDS

typedef struct {
    StringId name;
    u32 hash;

    u32 size;
    u32 align;

    TypeKind kind;

    ResolveState resolve_state;

    AstNodeId type_expr;

    SymbolRef declaration;

    union {
        struct {
            TypeId base;
        } pointer;

        struct {
            TypeId element;
            u64 length;
        } array;
    } as;
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
