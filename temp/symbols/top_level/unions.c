#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "hash/hash.h"
#include "diagnostics/diagnostics.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void sym_add_union(Resolver* r, AstNode* node, AstNodeId node_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);

    StringId name = node -> as.union_decl.name_id;
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

    sym_id = scope_add_sym(r, node_id, node -> as.union_decl.name_id, SYM_UNION);

    Symbol* symbol = &r -> table -> symbols[sym_id]; 

    u32 field_count = node -> as.union_decl.field_count;

    if (field_count == 0) {
        symbol -> as.unions.count = 0;
        return;
    }

    scope_enter(r);

    symbol -> as.unions.fields = arena_alloc(&r -> table -> arena, field_count * sizeof(SymbolId));
    symbol -> as.unions.count = field_count;

    Ast* ast = &module -> ast;

    for (u32 i = 0; i < field_count; i++) {
        AstNodeId field_node_id = node -> as.union_decl.fields[i];
        AstNode* field_node = &ast -> nodes[field_node_id];

        SymbolId field_id = scope_get_sym(
            r,
            field_node -> as.field_decl.name_id,
            hash_fnv1a_u32(field_node -> as.field_decl.name_id)
        );

        if (field_id != SYMBOL_ID_NONE) {
            diagnostic_add_symbol_already_defined(
                &driver_ctx.diagnostics,
                module,
                field_id,
                field_node_id
            );

            continue;
        }
        
        symbol -> as.unions.fields[i] = scope_add_sym(
            r,
            field_node_id,
            field_node -> as.field_decl.name_id,
            SYM_FIELD
        );
    }

    scope_exit(r);
}

