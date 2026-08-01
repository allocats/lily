#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_register_union(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId union_name = node -> as.union_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, union_name, hash_fnv1a_u32(union_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, union_name, SYM_UNION);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 field_count = node -> as.union_decl.field_count;
    if (field_count == 0) {
        symbol -> as.unions.count = 0;
        return;
    }

    symbol -> as.unions.fields = arena_alloc(&r -> table -> arena, field_count * sizeof(SymbolId));
    symbol -> as.unions.count = field_count;
}
