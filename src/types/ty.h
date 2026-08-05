#ifndef LILY_TYPES_H
#define LILY_TYPES_H

#include "ids.h"

#include "modules/types.h"
#include "types/types.h"

#define TYPE_ID_LOOKUP_REF(id) (&driver_ctx.type_table.entries[id])

void type_table_init(void);

TypeId builtin_add_primitive(TypeBuiltin type);
TypeId builtin_lookup_primitive(StringId name_id);

void resolve_types(void);
TypeId resolve_type(ModuleId module_id, AstNodeId type_expr_id);
TypeEntry* resolve_type_entry(ModuleId module_id, TypeId id);

TypeId type_table_lookup_nominal(ModuleId module_id, AstNode* ident);
TypeId type_table_register_nominal(u32 hash, StringId name, TypeKind kind, AstNodeId node_id);

TypeId type_table_lookup_pointer(TypeId base);
TypeId type_table_register_pointer(TypeId base);

TypeId type_table_lookup_array(TypeId base);
TypeId type_table_register_array(TypeId element, AstNodeId length);

TypeId type_table_register_struct(Module* module, AstNode* node, AstNodeId id, SymbolId sym_id);
TypeId type_table_register_union(Module* module, AstNode* node, AstNodeId id, SymbolId sym_id);
TypeId type_table_register_enum(Module* module, AstNode* node, AstNodeId id, SymbolId sym_id);

u32 types_hash_id(u32 hash, TypeId id);
u32 types_hash_nominal(NamespaceId ns, StringId name);
u32 types_hash_pointer(TypeId base);

void print_type_table(TypeTable* table);

#endif // !LILY_TYPES_H
