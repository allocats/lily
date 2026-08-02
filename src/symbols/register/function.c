#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "ids.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_register_function(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId function_name = node -> as.func_decl.name_id;
    u32 hash = hash_fnv1a_u32(function_name);

    SymbolId symbol_id = builtins_get_sym(r, function_name, hash);

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_is_builtin(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_get_sym(r, function_name, hash);

    if (symbol_id != SYMBOL_ID_NONE) {
        diagnostic_add_symbol_already_defined(
            &driver_ctx.diagnostics,
            module,
            symbol_id,
            node_id
        );

        return;
    }

    symbol_id = scope_add_sym(r, node_id, function_name, SYM_FUNCTION);

    Symbol* symbol = &r -> table -> symbols[symbol_id]; 

    u32 param_count = node -> as.func_decl.param_count;
    if (param_count == 0) {
        symbol -> as.function.count = 0;
        return;
    }

    symbol -> as.function.params = arena_alloc(&r -> table -> arena, param_count * sizeof(SymbolId));
    symbol -> as.function.count = param_count;
}
