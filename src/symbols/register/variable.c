#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_register_variable(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId variable_name = node -> as.var_decl.name_id;
    SymbolId symbol_id = table_get_sym(r, variable_name);

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, variable_name, SYM_VARIABLE);
}
