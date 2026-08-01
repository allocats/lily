#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_register_constant(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId constant_name = node -> as.const_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, constant_name, hash_fnv1a_u32(constant_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, constant_name, SYM_CONSTANT);
}
