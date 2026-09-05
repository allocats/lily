#ifndef LILY_TYPES_ENTRIES_TYPES_H
#define LILY_TYPES_ENTRIES_TYPES_H

#include "ids.h"
#include "utils/types.h"

#include <assert.h>
#include <stddef.h>

typedef enum {
    TYPE_FAMILY_STRUCUTRAL,
    TYPE_FAMILY_NOMINAL,
    // TYPE_FAMILY_BASE,
    TYPE_FAMILY_ERROR
} TypeFamily;

#define TYPES(X)        \
    X(TYPE_ARRAY)       \
    X(TYPE_BASE)        \
    X(TYPE_ENUM)        \
    X(TYPE_FUNCTION)    \
    X(TYPE_MODULE)      \
    X(TYPE_POINTER)     \
    X(TYPE_SLICE)       \
    X(TYPE_STRUCT)      \
    X(TYPE_UNION)       \
    X(TYPE_ERROR)

typedef enum {
    TYPES(GENERATE_ENUM)
} __attribute__((packed)) TypeKind;

static const char* TYPE_KIND_STRINGS[] = {
    TYPES(GENERATE_STRING)
};

#undef TYPES

typedef struct {
    TypeId id;
    TypeKind kind;

    u16 alignment;
    u32 size;

    u32 hash;

    SymbolId symbol_id;

    union {
        struct {
            StringId name;
        } base_type;

        struct {
            TypeId base;
        } pointer_type;

        struct {
            TypeId element;
            u32 size;
        } array_type;

        struct {
            TypeId element;
        } slice_type;

        struct {
            TypeId* fields;
            u32 field_count;

            SymbolId symbol_id;
        } struct_type;

        struct {
            TypeId* fields;
            u32 field_count;

            SymbolId symbol_id;
        } union_type;

        struct {
            TypeId underlying_type;

            SymbolId symbol_id;
        } enum_type;

        struct {
            TypeId* arguments;
            u32 argument_count;

            TypeId return_type;
        } function_type;

        struct { // TODO: think about this
            ScopeId scope_id;

            SymbolId symbol_id;
        } module_type;
    } as;
} TypeEntry;

#endif // !LILY_TYPES_ENTRIES_TYPES_H
