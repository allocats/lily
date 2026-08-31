#include "ast/nodes/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "symbols/symbols/symbols.h"
#include "resolver_stack/types.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "utils/macros.h"

extern DriverCtx driver;

SymbolId make_symbol_from_ast_node(FileId file_id, AstNodeId node_id) {
    Arena* arena = &driver.symbol_table.symbol_data_arena;

    File* file = file_lookup_id(file_id);
    AstNode* node = &file -> ast.nodes[node_id];

    SymbolId id = symbol_table_alloc_symbol();
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    symbol -> id = id;
    symbol -> kind = SYMBOL_ERROR;

    symbol -> file_id = file_id;
    symbol -> ast_node_id = node_id;

    symbol -> flags = node -> flags;
    symbol -> state = RESOLVE_UNRESOLVED;

    switch (node -> kind) {
        case AST_IMPORT_DIRECTIVE:
            symbol -> kind = SYMBOL_IMPORT;
            symbol -> name_id = node -> as.import_directive.binding;

            // Caller must set scope_id
            break;

        case AST_FUNCTION_DECL:
            symbol -> kind = SYMBOL_FUNCTION;
            symbol -> name_id = node -> as.function_decl.name;

            u32 parameter_count = node -> as.function_decl.parameters.count;

            if (parameter_count == 0) {
                symbol -> as.function_symbol.parameters = null;
            } else {
                symbol -> as.function_symbol.parameters = arena_alloc(arena, sizeof(SymbolId) * parameter_count);
            }

            symbol -> as.function_symbol.parameter_count = parameter_count;
            symbol -> as.function_symbol.return_type_id = TYPE_ID_NONE;
            break;

        case AST_PARAMETER:
            symbol -> kind = SYMBOL_PARAMETER;
            symbol -> name_id = node -> as.parameter_decl.name;
            symbol -> as.parameter_symbol.type_id = TYPE_ID_NONE;
            break;

        case AST_ENUM_DECL:
            symbol -> kind = SYMBOL_ENUM;
            symbol -> name_id = node -> as.enum_decl.name;

            u32 variant_count = node -> as.enum_decl.variants.count;

            symbol -> as.enum_symbol.variants = arena_alloc(arena, sizeof(SymbolId) * variant_count);
            symbol -> as.enum_symbol.variant_count = variant_count;
            symbol -> as.enum_symbol.resolved_type_id = TYPE_ID_NONE;
            break;

        case AST_VARIABLE_DECL:
            symbol -> kind = SYMBOL_VARIABLE;
            symbol -> name_id = node -> as.variable_decl.name;
            symbol -> as.variable_symbol.type_id = TYPE_ID_NONE;
            break;

        case AST_FIELD:
            symbol -> kind = SYMBOL_FIELD;
            symbol -> name_id = node -> as.field.name;
            symbol -> as.field_symbol.type_id = TYPE_ID_NONE;
            break;

        case AST_STRUCT_DECL:
            symbol -> kind = SYMBOL_STRUCT;
            symbol -> name_id = node -> as.struct_decl.name;

            u32 struct_field_count = node -> as.struct_decl.fields.count;

            if (struct_field_count == 0) {
                symbol -> as.struct_symbol.fields = 0;
            } else {
                symbol -> as.struct_symbol.fields = arena_alloc(arena, sizeof(SymbolId) * struct_field_count);
            }

            symbol -> as.struct_symbol.field_count = struct_field_count;
            symbol -> as.struct_symbol.resolved_type_id = TYPE_ID_NONE;
            break;

        case AST_UNION_DECL:
            symbol -> kind = SYMBOL_UNION;
            symbol -> name_id = node -> as.union_decl.name;

            u32 union_field_count = node -> as.union_decl.fields.count;

            if (union_field_count == 0) {
                symbol -> as.union_symbol.fields = 0;
            } else {
                symbol -> as.union_symbol.fields = arena_alloc(arena, sizeof(SymbolId) * union_field_count);
            }

            symbol -> as.union_symbol.field_count = union_field_count;
            symbol -> as.union_symbol.resolved_type_id = TYPE_ID_NONE;
            break;

        default:
            UNREACHABLE("invalid case in make_symbol_from_ast_node()");
    }

    return id;
}

TypeId get_type_from_symbol(SymbolId id) {
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(id);

    switch (symbol -> kind) {
        case SYMBOL_TYPE:
            return symbol -> as.type_symbol.type_id;

        case SYMBOL_STRUCT:
            return symbol -> as.struct_symbol.resolved_type_id;

        case SYMBOL_UNION:
            return symbol -> as.union_symbol.resolved_type_id;

        case SYMBOL_ENUM:
            return symbol -> as.enum_symbol.resolved_type_id;

        case SYMBOL_IMPORT:
            return symbol -> as.import_symbol.type_id;

        default:
            return TYPE_ID_NONE;
    }
}
