#include "types/entries/entries.h"
#include "driver/types.h"

extern DriverCtx driver;

inline bool is_type_void(TypeId id) {
    return driver.type_table.builtins.type_void == id;
}
