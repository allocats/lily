#include "ast/nodes/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "ids.h"
#include "modules/modules.h"
#include "query/query.h"
#include "query/types.h"
#include "symbols/symbols.h"
#include "types/builtins.h"
#include "types/ty.h"
#include "types/types.h"

#include <stdio.h>

extern LilyCtx driver_ctx;

static TypeId resolve_type_base(Module* module, AstNode* expr);
static TypeId resolve_type_body(ModuleId module_id, TypeId id);

void resolve_types(void) {
    TypeTable* table = &driver_ctx.type_table;

    for (u32 i = BUILTIN_NOMINAL_TYPES_COUNT; i < table -> count; i++) {
        resolve_type_entry(1, i);
    }
}

TypeId resolve_type(ModuleId module_id, AstNodeId type_expr_id) {
    if (type_expr_id == AST_NODE_ID_NONE) {
        return driver_ctx.type_table.builtins.type_void;
    }

    Module* module = MODULE_ID_LOOKUP_REF(module_id);
    AstNode* type_expr = &module -> ast.nodes[type_expr_id];

    switch (type_expr -> kind) {
        // nominal: enums, structs, and unions
        case AST_TYPE_BASE:
            return resolve_type_base(module, type_expr);

        // structural 
        case AST_TYPE_POINTER:
            TypeId base = resolve_type(
                module_id,
                type_expr -> as.type_pointer_expr.base_type
            );

            if (base == TYPE_ID_NONE) {
                return TYPE_ID_NONE;
            }

            return type_table_register_pointer(base);
            break;

        // structural 
        case AST_TYPE_ARRAY:
            TypeId element = resolve_type(
                module_id,
                type_expr -> as.type_array_expr.element
            );

            if (element == TYPE_ID_NONE) {
                return TYPE_ID_NONE;
            }

            i64 length = 0;

            // if (!evaluate_const_expr_i64(module_id, type_expr -> as.type_array_expr.size_expr, &length)) {
            // }

            return type_table_register_array(
                element,
                type_expr -> as.type_array_expr.size_expr
            );
            break;

        default:
            return TYPE_ID_NONE;
    }

    return TYPE_ID_NONE;
}

static TypeId resolve_type_base(Module* module, AstNode* expr) {
    Ast* ast = &module -> ast;
    AstNode* ident = &ast -> nodes[expr -> as.type_base_expr.ident];

    if (ident -> as.ident.namespace_id == NAMESPACE_ID_NONE) {
        TypeId builtin = builtin_lookup_primitive(ident -> as.ident.name_id);

        if (builtin != TYPE_ID_NONE) {
            return builtin;
        }

        // TODO: ponder this
        ident -> as.ident.namespace_id = module -> namespace_id;
    }

    TypeId id = type_table_lookup_nominal(ident);

    if (id == TYPE_ID_NONE) {
        // TODO: Errors
        printf("Unknown type\n");
    }

    return id;
}

TypeEntry* resolve_type_entry(ModuleId module_id, TypeId id) {
    TypeTable* table = &driver_ctx.type_table;
    TypeEntry* entry = &table -> entries[id];

    if (entry -> resolve_state == RESOLVE_RESOLVED) return entry;
    if (entry -> resolve_state == RESOLVE_ERROR)    return null;

    Query query = {
        .kind = QUERY_TYPE,
        .module_id = module_id,
        .as.type_id = id
    };

    if (entry -> resolve_state == RESOLVE_RESOLVING) {
        i32 cycle_start = query_stack_find(&driver_ctx.query_stack, query);

        diagnostic_add_query_type_cycle(
            &driver_ctx.diagnostics,
            cycle_start
        );

        entry -> resolve_state = RESOLVE_ERROR;
        return null;
    }

    entry -> resolve_state = RESOLVE_RESOLVING;

    if (!query_stack_push(&driver_ctx.query_stack, query)) {
        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_ERROR,
            "reached recursion limit"
        );

        entry -> resolve_state = RESOLVE_ERROR;
        return null;
    }

    bool resolved_body = resolve_type_body(module_id, id);

    query_stack_pop(&driver_ctx.query_stack);
    
    entry -> resolve_state = resolved_body ? RESOLVE_RESOLVED : RESOLVE_ERROR;

    return resolved_body ? entry : null;
}

static TypeId resolve_type_body(ModuleId module_id, TypeId id) {
    TypeTable* table = &driver_ctx.type_table;
    TypeEntry* entry = &table -> entries[id]; 

    if (entry -> owning_symbol == SYMBOL_ID_NONE) {
        return true;
    }

    return symbols_resolve_by_id(module_id, entry -> owning_symbol);
}
