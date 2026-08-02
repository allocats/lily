#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"
#include "types/ty.h"

#include <stdio.h>

void sym_register_struct(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId struct_name = node -> as.struct_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, struct_name, hash_fnv1a_u32(struct_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, struct_name, SYM_STRUCT);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 field_count = node -> as.struct_decl.field_count;
    if (field_count == 0) {
        symbol -> as.structs.count = 0;
        return;
    }

    symbol -> as.structs.fields = arena_alloc(&r -> table -> arena, field_count * sizeof(SymbolId));
    symbol -> as.structs.count = field_count;

    TypeId type_id = type_table_add_struct(module, node);

    if (type_id == TYPE_ID_NONE) {
        printf("struct is already defined!");
    }
}
