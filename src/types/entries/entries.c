#include "types/entries/entries.h"
#include "driver/types.h"
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
