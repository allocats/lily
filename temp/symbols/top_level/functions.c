#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "hash/hash.h"
#include "diagnostics/diagnostics.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_add_function(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.func_decl.name_id;
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

    sym_id = scope_add_sym(r, node_id, node -> as.func_decl.name_id, SYM_FUNCTION);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 param_count = node -> as.func_decl.param_count;

    if (param_count == 0) {
        symbol -> as.function.count = 0;
        return;
    }

    symbol -> as.function.params = arena_alloc(&r -> table -> arena, param_count * sizeof(SymbolId));
    symbol -> as.function.count = param_count;
}

void resolve_function(Resolver* r, Symbol* sym) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);
    Ast* ast = &module -> ast;

    AstNodeId node_id = sym -> declaration;
    AstNode* node = &ast -> nodes[node_id];

    scope_enter(r);

    for (u32 i = 0; i < node -> as.func_decl.param_count; i++) {
        AstNodeId param_id = node -> as.func_decl.params[i];
        AstNode* param = &ast -> nodes[param_id];

        scope_add_sym(r, param_id, param -> as.param_decl.name_id, SYM_PARAMETER);
    }

    resolve_block(r, node -> as.func_decl.block);

    scope_exit(r);
}
