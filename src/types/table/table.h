#ifndef LILY_TYPES_TABLE_H
#define LILY_TYPES_TABLE_H

#include "ids.h"
#include "types/entries/types.h"

void type_table_init(void);

TypeId type_table_intern_nominal(StringId name_id, TypeKind kind);

#endif // !LILY_TYPES_TABLE_H
