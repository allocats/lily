#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "ids.h"
#include "symbols/register/register.h"
#include "modules/modules.h"
#include "symbols/symbols.h"
#include "symbols/types.h"
#include "types/ty.h"
#include "utils/debug.h"

#include <stdio.h>

extern LilyCtx driver_ctx;

void sym_resolve_function(Resolver* r, Symbol* symbol) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    TypeId return_type_id = resolve_type(r -> current_module_id, node -> as.func_decl.return_type_expr);
    if (return_type_id == TYPE_ID_NONE) {
        // diagnostic_add_invalid_return_type();
    }

    symbol -> as.function.return_type = return_type_id;

    scope_enter(r);

    for (u32 i = 0; i < node -> as.func_decl.param_count; i++) {
        AstNodeId param_id = node -> as.func_decl.params[i];
        AstNode* param = &module -> ast.nodes[param_id];

        TypeId param_type = resolve_type(r -> current_module_id, param -> as.param_decl.type_expr);

        if (return_type_id == TYPE_ID_NONE) {
            // diagnostic_add_invalid_type();
        }

        SymbolId param_symbol_id = table_get_sym(r, param -> as.param_decl.name_id);

        if (param_symbol_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                param_symbol_id,
                param_id
            );

            continue;
        }

        param_symbol_id = scope_add_sym(r, param_id, param -> as.param_decl.name_id, SYM_PARAMETER); 

        Symbol* param_symbol = &module -> symbol_table.symbols[param_symbol_id];

        param_symbol -> as.parameter.type = param_type;

        symbol -> as.function.params[i] = param_symbol_id;

        debug_printf("Symbols: Added parameter (%d) to function(%d)\n", param_symbol_id, symbol -> id);
    }

    scope_exit(r);
}
