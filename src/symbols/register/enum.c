#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"
#include "types/ty.h"

#include <stdio.h>

void sym_register_enum(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId enum_name = node -> as.enum_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, enum_name, hash_fnv1a_u32(enum_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, enum_name, SYM_ENUM);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 variant_count = node -> as.enum_decl.variant_count;
    if (variant_count == 0) {
        symbol -> as.enums.count = 0;
        return;
    }

    symbol -> as.enums.variants = arena_alloc(&r -> table -> arena, variant_count * sizeof(SymbolId));
    symbol -> as.enums.count = variant_count;

    TypeId type_id = type_table_add_enum(module, node);

    if (type_id == TYPE_ID_NONE) {
        printf("pluh");
    }
}
