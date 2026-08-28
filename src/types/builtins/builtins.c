#include "types/builtins/builtins.h"
#include "string_interner/interner.h"
#include "types/builtins/types.h"
#include "types/table/table.h"

void types_register_builtins(void) {
    for (u32 i = 0; i < BUILTIN_NOMINAL_TYPES_COUNT; i += 1) {
        TypeBuiltin* type = &BUILTIN_NOMINAL_TYPES[i];
        type -> name_id = string_intern_str8(type -> name); 

        TypeId id = type_table_intern_nominal(type -> name_id, TYPE_BASE);

        type -> id = id;
    }
}
