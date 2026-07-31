#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "hash/hash.h"
#include "diagnostics/diagnostics.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_add_enum(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.enum_decl.name_id;
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

    sym_id = scope_add_sym(r, node_id, node -> as.enum_decl.name_id, SYM_ENUM);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 variant_count = node -> as.enum_decl.variant_count;

    if (variant_count == 0) {
        symbol -> as.enums.count = 0;
        return;
    }

    scope_enter(r);

    symbol -> as.enums.variants = arena_alloc(&r -> table -> arena, variant_count * sizeof(SymbolId));
    symbol -> as.enums.count = variant_count;

    Ast* ast = &module -> ast;

    for (u32 i = 0; i < variant_count; i++) {
        AstNodeId variant_node_id = node -> as.enum_decl.variants[i];
        AstNode* variant_node = &ast -> nodes[variant_node_id];

        SymbolId variant_id = scope_get_sym(
            r,
            variant_node -> as.variant_decl.name_id,
            hash_fnv1a_u32(variant_node -> as.variant_decl.name_id)
        );

        if (variant_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                variant_id,
                variant_node_id
            );

            continue;
        }
        
        symbol -> as.enums.variants[i] = scope_add_sym(
            r,
            variant_node_id,
            variant_node -> as.variant_decl.name_id,
            SYM_VARIANT
        );
    }

    scope_exit(r);
}
