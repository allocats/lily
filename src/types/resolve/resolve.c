#include "ast/nodes/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "symbols/resolve/resolve.h"
#include "symbols/symbols/symbols.h"
#include "symbols/table/table.h"
#include "types/entries/entries.h"
#include "types/entries/types.h"
#include "types/resolve/resolve.h"
#include "types/table/table.h"
#include "utils/macros.h"

#include <assert.h>

extern DriverCtx driver;

static TypeId resolve_nominal_type_entry(TypeId id);
static TypeId resolve_base_type_expr(File* file, AstNodeId node_id);

void resolve_top_level_types(void) {
    TypeTable* table = &driver.type_table;

    for (u32 i = 0; i < table -> entry_count; i++) {
        TypeEntry* entry = &table -> entries[i];

        if (type_family_lut[entry -> kind] != TYPE_FAMILY_NOMINAL) {
            continue;
        }

        resolve_nominal_type_entry(i);
    }
}

static TypeId resolve_nominal_type_entry(TypeId id) {
    assert(id != TYPE_ID_NONE);

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(id);

    resolve_symbol(entry -> symbol_id);

    return id;
}

TypeId resolve_type_expr(FileId file_id, AstNodeId expr_id) {
    if (expr_id == AST_NODE_ID_NONE) {
        return driver.type_table.builtins.type_void;
    }

    File* file = file_lookup_id(file_id);

    AstNode* node = &file -> ast.nodes[expr_id];

    TypeId id = TYPE_ID_NONE;

    switch (node -> kind) {
        case AST_TYPE_BASE:
            id = resolve_base_type_expr(file, node -> as.type_base.expr);
            break;

        case AST_TYPE_ARRAY:
            TypeId element = resolve_type_expr(file_id, node -> as.type_array.element);

            if (node -> as.type_array.size_expr == AST_NODE_ID_NONE) {
                id = type_table_intern_slice(element);
            } else {
                // TODO: compile time interperter & intern array;
            }
            break;

        case AST_TYPE_POINTER:
            TypeId base = resolve_type_expr(file_id, node -> as.type_pointer.base_type);

            if (base == TYPE_ID_NONE) {
                break;
            }

            id = type_table_intern_pointer(base);
            break;

        case AST_TYPE_FUNCTION:
            u32 count = node -> as.type_function.parameters.count;

            TypeId* arguments = arena_alloc(&driver.type_table.gpa, count * sizeof(TypeId));

            for (u32 i = 0; i < count; i++) {
                TypeId arg_id = resolve_type_expr(file_id, node -> as.type_function.parameters.ids[i]);
                
                if (arg_id == TYPE_ID_NONE) {
                    break;
                }

                arguments[i] = id; 
            }

            TypeId return_type_id = resolve_type_expr(file_id, node -> as.type_function.return_type);

            if (return_type_id == TYPE_ID_NONE) {
                break;
            }

            id = type_table_intern_function(return_type_id, arguments, count);
            break;

        case AST_TYPE_VARIADIC:
            id = driver.type_table.builtins.type_va_list;
            break;

        default:
            UNREACHABLE("resolve_type_expr()");
    }

    return id;
}

static TypeId resolve_base_type_expr(File* file, AstNodeId node_id) {
    AstNode* node = &file -> ast.nodes[node_id];

    if (node -> kind == AST_IDENTIFIER) {
        TypeId id = type_table_lookup_nominal(node -> as.identifier.name);

        if (id == TYPE_ID_NONE) { 
            diagnostic_add_symbol_does_not_exist(file -> id, node -> id, node -> as.identifier.name);
        }

        return id;
    }

    SymbolId symbol_id = SYMBOL_ID_NONE;

    if (node -> kind == AST_FUNCTION_CALL) {
        symbol_id = symbol_table_lookup(file -> scope_id, node -> as.function_call.identifier, file -> id);
    } else {
        symbol_id = resolve_name_expr(file, node_id);
    }

    if (symbol_id == SYMBOL_ID_NONE) {
        diagnostic_add_symbol_does_not_exist(file -> id, node -> id, node -> as.identifier.name);
        return TYPE_ID_NONE;
    }

    return get_type_from_symbol(symbol_id);
}
