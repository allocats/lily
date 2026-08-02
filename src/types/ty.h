#ifndef LILY_TYPES_H
#define LILY_TYPES_H

#include "ids.h"

#include "modules/types.h"

#define TYPE_ID_LOOKUP_REF(id) (&driver_ctx.type_table.entries[id])

void type_table_init(void);

TypeId resolve_type(ModuleId module_id, AstNodeId type_expr_id);

TypeId type_table_add_struct(Module* module, AstNode* node);
TypeId type_table_add_union(Module* module, AstNode* node);
TypeId type_table_add_enum(Module* module, AstNode* node);

#endif // !LILY_TYPES_H
