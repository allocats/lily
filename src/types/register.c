#include "ast/nodes/types.h"
#include "driver/types.h"
#include "ids.h"
#include "modules/types.h"
#include "types/ty.h"
#include "types/types.h"
#include "utils/debug.h"

#ifdef DEBUG_MODE
#include "string_interner/interner.h"
#endif /* ifdef DEBUG_MODE */

extern LilyCtx driver_ctx;

TypeId type_table_register_struct(Module* module, AstNode* node, AstNodeId id, SymbolId sym_id) {
    StringId name = node -> as.struct_decl.name_id;
    u32 hash = types_hash_nominal(module -> namespace_id, name);

    debug_printf(
        "Types: Registering struct %.*s: 0x%x)\n",
        STRING_ID_LOOKUP(name).str.length,
        STRING_ID_LOOKUP(name).str.pointer,
        hash
    );

    TypeId type_id = type_table_register_nominal(hash, name, TYPE_STRUCT, id);

    if (type_id == TYPE_ID_NONE) return type_id;

    TypeEntry* entry = &driver_ctx.type_table.entries[type_id];

    entry -> declaration.module_id = module -> id;
    entry -> declaration.symbol_id = sym_id;

    return type_id;
}

TypeId type_table_register_union(Module* module, AstNode* node, AstNodeId id, SymbolId sym_id) {
    StringId name = node -> as.union_decl.name_id;
    u32 hash = types_hash_nominal(module -> namespace_id, name);

    debug_printf(
        "Types: Registering union %.*s: 0x%x)\n",
        STRING_ID_LOOKUP(name).str.length,
        STRING_ID_LOOKUP(name).str.pointer,
        hash
    );

    TypeId type_id = type_table_register_nominal(hash, name, TYPE_STRUCT, id);

    if (type_id == TYPE_ID_NONE) return type_id;

    TypeEntry* entry = &driver_ctx.type_table.entries[type_id];

    entry -> declaration.module_id = module -> id;
    entry -> declaration.symbol_id = sym_id;

    return type_id;
}

TypeId type_table_register_enum(Module* module, AstNode* node, AstNodeId id, SymbolId sym_id) {
    StringId name = node -> as.enum_decl.name_id;
    u32 hash = types_hash_nominal(module -> namespace_id, name);

    debug_printf(
        "Types: Registering enum %.*s: 0x%x)\n",
        STRING_ID_LOOKUP(name).str.length,
        STRING_ID_LOOKUP(name).str.pointer,
        hash
    );

    TypeId type_id = type_table_register_nominal(hash, name, TYPE_STRUCT, id);

    if (type_id == TYPE_ID_NONE) return type_id;

    TypeEntry* entry = &driver_ctx.type_table.entries[type_id];

    entry -> declaration.module_id = module -> id;
    entry -> declaration.symbol_id = sym_id;

    return type_id;
}
