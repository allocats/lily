#ifndef LILY_TYPES_ENTRIES_TYPES_H
#define LILY_TYPES_ENTRIES_TYPES_H

#include "ids.h"

#include <assert.h>
#include <stddef.h>

typedef enum {
    TYPE_FAMILY_STRUCUTRAL,
    TYPE_FAMILY_NOMINAL,
    // TYPE_FAMILY_BASE,
    TYPE_FAMILY_ERROR
} TypeFamily;

typedef enum {
    TYPE_ARRAY,
    TYPE_BASE,
    TYPE_ENUM,
    TYPE_FUNCTION,
    TYPE_MODULE,
    TYPE_POINTER,
    TYPE_SLICE,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ERROR,
} __attribute__((packed)) TypeKind;

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
        } module_type;
    } as;
} TypeEntry;

#endif // !LILY_TYPES_ENTRIES_TYPES_H
