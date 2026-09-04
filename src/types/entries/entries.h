#ifndef LILY_TYPES_ENTRIES_H
#define LILY_TYPES_ENTRIES_H

#include "types/entries/types.h"

static constexpr TypeFamily type_family_lut[] = {
    [TYPE_ARRAY]    = TYPE_FAMILY_STRUCUTRAL,
    [TYPE_BASE]     = TYPE_FAMILY_NOMINAL,
    [TYPE_ENUM]     = TYPE_FAMILY_NOMINAL,
    [TYPE_FUNCTION] = TYPE_FAMILY_STRUCUTRAL,
    [TYPE_MODULE]   = TYPE_FAMILY_NOMINAL,
    [TYPE_POINTER]  = TYPE_FAMILY_STRUCUTRAL,
    [TYPE_SLICE]    = TYPE_FAMILY_STRUCUTRAL,
    [TYPE_STRUCT]   = TYPE_FAMILY_NOMINAL,
    [TYPE_UNION]    = TYPE_FAMILY_NOMINAL,
    [TYPE_ERROR]    = TYPE_FAMILY_ERROR,
};

bool is_type(TypeId id, TypeKind kind);

bool is_type_void(TypeId id);
bool is_type_float(TypeId id);
bool is_type_int(TypeId id);
bool is_type_signed_int(TypeId id);
bool is_type_unsigned_int(TypeId id);

// Types must be passed in the correct order, it assumes B is being assigned to A
bool are_types_compatible(TypeId a, TypeId b);
bool can_type_cast_to(TypeId to, TypeId from);

#endif // !LILY_TYPES_ENTRIES_H
