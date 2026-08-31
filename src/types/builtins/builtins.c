#include "types/builtins/builtins.h"
#include "driver/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "types/entries/types.h"
#include "types/table/table.h"

extern DriverCtx driver;

TypeBuiltin BUILTIN_NOMINAL_TYPES[] = {
#define X(id, str, sz, al)              \
    {                                   \
        .name = {                       \
            .ptr = str,                 \
            .len = sizeof(str) - 1,     \
        },                              \
        .size = sz,                     \
        .align = al,                    \
    },

    BUILTIN_TYPES(X)
#undef X
};


void types_register_builtins(void) {
    for (u32 i = 0; i < BUILTIN_NOMINAL_TYPES_COUNT; i++) {
        TypeBuiltin* type = &BUILTIN_NOMINAL_TYPES[i];
        type -> name_id = string_intern_str8(type -> name); 

        TypeId id = type_table_intern_nominal(SYMBOL_ID_NONE, type -> name_id, TYPE_BASE);

        type -> id = id;

        TypeEntry* entry = TYPE_ID_LOOKUP_REF(id);

        entry -> size = type -> size;
        entry -> alignment = type -> align;

        TypeId* builtin_ids = (TypeId*) &driver.type_table.builtins;

        static_assert(
            sizeof(TypeBuiltinIds) == BUILTIN_NOMINAL_TYPES_COUNT * sizeof(TypeId),
            "TypeBuiltinIds is not tightly packed"
        );

        builtin_ids[i] = id;
    }
}
