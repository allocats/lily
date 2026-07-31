#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "hash/hash.h"
#include "diagnostics/diagnostics.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_add_macro(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.macro_decl.name_id;
    SymbolId sym_id = scope_get_sym(r, name, hash_fnv1a_u32(name));

    if (sym_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            sym_id,
            node_id
        );

        return;
    }

    sym_id = scope_add_sym(r, node_id, node -> as.macro_decl.name_id, SYM_MACRO);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 param_count = node -> as.macro_decl.param_count;

    if (param_count == 0) {
        symbol -> as.macro.count = 0;
        return;
    }

    symbol -> as.macro.params = arena_alloc(&r -> table -> arena, param_count * sizeof(SymbolId));
    symbol -> as.macro.count = param_count;
}
