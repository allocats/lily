#ifndef LILY_TYPES_TABLE_H
#define LILY_TYPES_TABLE_H

#include "ids.h"
#include "types/entries/types.h"

#define TYPE_ID_LOOKUP(i)      ( driver.type_table.entries[i])
#define TYPE_ID_LOOKUP_REF(i)  (&driver.type_table.entries[i])

void type_table_init(void);

TypeId type_table_intern_nominal(SymbolId symbol_id, StringId name_id, TypeKind kind);
TypeId type_table_intern_pointer(TypeId base);
TypeId type_table_intern_function(TypeId return_type, TypeId* arguments, u32 argument_count);

#endif // !LILY_TYPES_TABLE_H
