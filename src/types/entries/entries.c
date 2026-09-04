#include "types/entries/entries.h"
#include "driver/types.h"
#include "ids.h"
#include "types/entries/types.h"
#include "types/table/table.h"

extern DriverCtx driver;

inline bool is_type(TypeId id, TypeKind kind) {
    TypeEntry* type = TYPE_ID_LOOKUP_REF(id);

    if (type -> kind == kind) {
        return true;
    } else {
        return false;
    }
}

inline bool is_type_void(TypeId id) {
    return driver.type_table.builtins.type_void == id;
}

inline bool is_type_float(TypeId id) {
    if (id == driver.type_table.builtins.type_f32 || id == driver.type_table.builtins.type_f64) {
        return true;
    } else {
        return false;
    }
}

inline bool is_type_int(TypeId id) {
    if (id >= driver.type_table.builtins.type_u8 && id <= driver.type_table.builtins.type_isize) {
        return true;
    } else {
        return false;
    }
}

inline bool is_type_signed_int(TypeId id) {
    if (id >= driver.type_table.builtins.type_i8 && id <= driver.type_table.builtins.type_isize) {
        return true;
    } else {
        return false;
    }
}

inline bool is_type_unsigned_int(TypeId id) {
    if (id >= driver.type_table.builtins.type_u8 && id <= driver.type_table.builtins.type_usize) {
        return true;
    } else {
        return false;
    }
}

// Types must be passed in the correct order, it assumes R is being assigned to L
bool are_types_compatible(TypeId l, TypeId r) {
    if (l == TYPE_ID_NONE || r == TYPE_ID_NONE) {
        return false;
    }

    if (l == r) {
        return true;
    }

    if (is_type(l, TYPE_POINTER) && is_type(r, TYPE_POINTER)) {
        TypeId l_base = driver.type_table.entries[l].as.pointer_type.base;
        TypeId r_base = driver.type_table.entries[r].as.pointer_type.base;
        TypeId void_type = driver.type_table.builtins.type_void;

        return l_base == void_type || r_base == void_type;
    }

    bool both_signed = is_type_signed_int(l) && is_type_signed_int(r);
    bool both_unsigned = is_type_unsigned_int(l) && is_type_unsigned_int(r);
    bool both_floating = is_type_float(l) && is_type_float(r);

    if (both_signed || both_unsigned || both_floating) {
        u32 l_size = driver.type_table.entries[l].size;
        u32 r_size = driver.type_table.entries[r].size;

        return l_size >= r_size;
    }

    return false;
}

// Types must be passed in the correct order
bool can_types_explicitly_cast(TypeId to, TypeId from) {
    if (to == TYPE_ID_NONE || from == TYPE_ID_NONE) {
        return false;
    }

    if (to == from) {
        return true;
    }

    if (are_types_compatible(to, from)) {
        return true;
    }

    if (is_type(to, TYPE_POINTER) && is_type(from, TYPE_POINTER)) {
        return true;
    }

    bool to_int = is_type_signed_int(to) || is_type_unsigned_int(to);
    bool from_int = is_type_signed_int(from) || is_type_unsigned_int(from);

    if (to_int && from_int) {
        return true;
    }

    bool to_float = is_type_float(to);
    bool from_float = is_type_float(from);

    if ((to_int && from_float) || (to_float && from_int)) {
        return true;
    }

    if (to_float && from_float) {
        return true;
    }

    if (
        (is_type(to, TYPE_POINTER) && is_type_unsigned_int(from)) || 
        (is_type_unsigned_int(to) && is_type(from, TYPE_POINTER))
    ) {
        return true;
    }

    return false;
}
