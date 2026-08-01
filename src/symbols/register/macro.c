#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_register_macro(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId macro_name = node -> as.macro_decl.name_id;
    SymbolId symbol_id = scope_get_sym(r, macro_name, hash_fnv1a_u32(macro_name));

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, macro_name, SYM_MACRO);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 param_count = node -> as.macro_decl.param_count;
    if (param_count == 0) {
        symbol -> as.macro.count = 0;
        return;
    }

    symbol -> as.macro.params = arena_alloc(&r -> table -> arena, param_count * sizeof(SymbolId));
    symbol -> as.macro.count = param_count;
}
