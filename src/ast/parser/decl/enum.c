#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/types.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"
#include "utils/types.h"

AstNodeId parse_enum_decl(Parser *p, StringId name) {
    AstNodeId id = parser_create_node(p, AST_ENUM_DECL, AST_FLAGS_IS_TOP_DECL | AST_FLAGS_IS_CONSTANT, -3);
    AstNode* node = parser_get_node(p, id);

    node -> as.enum_decl.name = name;

    if (parser_check(p, TOK_L_BRACKET)) {
        parser_advance(p); // advance past '['

        AstNodeId type_expr_id = parse_type_expr(p);

        if (IS_NODE_ERROR(p, type_expr_id)) {
            return parser_error(p, id, RECOVERY_DECL);
        }

        if (!parser_check(p, TOK_R_BRACKET)) {
            Token previous = parser_peek_previous(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &previous,
                DIAG_LOC_END_OF_TOK,
                "expected ']'",
                "add a ']' here"
            );

            return parser_error(p, id, RECOVERY_DECL);
        }
        
        parser_advance(p); // advance past ']'

        node = parser_get_node(p, id);
        node -> as.enum_decl.type_expr = type_expr_id;
    }

    if (!parser_check(p, TOK_L_BRACE)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected '}'",
            "add a '}' here"
        );

        return parser_error(p, id, RECOVERY_DECL);
    }

    parser_advance(p); // advance past '{'

    while (!parser_check(p, TOK_R_BRACE)) {
        AstNodeId value_expr_id = AST_NODE_ID_NONE;
        Token variant_name_token = parser_peek(p);

        u32 starting_index = p -> cursor;

        if (variant_name_token.kind != TOK_IDENT) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &variant_name_token,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier for variant name",
                "add a valid identifier here"
            );

            return parser_error(p, id, RECOVERY_DECL);
        }

        parser_advance(p); // advance past identifier

        if (parser_check(p, TOK_EQ)) {
            parser_advance(p); // advance past '='

            value_expr_id = parse_expression(p, 0);

            if (IS_NODE_ERROR(p, value_expr_id)) {
                return parser_error(p, id, RECOVERY_DECL);
            }
        }

        if (!parser_check(p, TOK_SEMI)) {
            Token previous = parser_peek_previous(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &previous,
                DIAG_LOC_END_OF_TOK,
                "expected ';'",
                "add a ';' here"
            );

            return parser_error(p, id, RECOVERY_DECL);
        }

        AstNodeId variant_node_id = parser_create_node(p, AST_VARIANT, AST_FLAGS_NONE, 0);
        AstNode* variant_node = parser_get_node(p, variant_node_id);

        variant_node -> as.variant.name = string_intern_token(p -> current_file -> id, variant_name_token);
        variant_node -> as.variant.value_expr = value_expr_id;

        variant_node -> tokens.start = starting_index;
        variant_node -> tokens.end = p -> cursor;

        node = parser_get_node(p, id);

        ast_id_list_append(&node -> as.enum_decl.variants, &p -> current_file -> ast, variant_node_id);

        parser_advance(p); // advance past ';'
    }

    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past '}'
    
    p -> current_file -> ast.declaration_count += node -> as.enum_decl.variants.count;

    return id;
}
