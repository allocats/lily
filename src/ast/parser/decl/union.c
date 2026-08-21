#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/parser.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"

AstNodeId parse_union_decl(Parser* p, StringId name) {
    AstNodeId id = parser_create_node(p, AST_UNION_DECL, AST_FLAGS_IS_TOP_DECL, -2);
    AstNode* node = parser_get_node(p, id);

    node -> as.union_decl.name = name;

    if (!parser_check(p, TOK_L_BRACE)) {
        Token token = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &token,
            DIAG_LOC_END_OF_TOK,
            "expected '{' for union body",
            "add a '{' here" 
        );

        return parser_recover_end_of_fn(p, id);
    }

    parser_advance(p); // advance past '{'

    while (!parser_check(p, TOK_R_BRACE)) {
        AstNodeId type_expr_id = AST_NODE_ID_NONE;
        Token field_name_token = parser_peek(p);

        u32 starting_index = p -> cursor;

        if (field_name_token.kind != TOK_IDENT) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &field_name_token,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier for field name",
                "add a valid identifier here"
            );

            return parser_error_decl(p, id);
        }

        parser_advance(p); // advance past identifier

        if (!parser_check(p, TOK_COLON)) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &field_name_token,
                DIAG_LOC_END_OF_TOK,
                "expected ':'",
                "add a ':' here"
            );

            return parser_error_decl(p, id);
        }

        parser_advance(p); // advance past ':'

        type_expr_id = parse_type_expr(p);

        if (type_expr_id == AST_NODE_ID_NONE) {
            return parser_error_decl(p, id);
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

            return parser_error_decl(p, id);
        }

        AstNodeId field_node_id = parser_create_node(p, AST_FIELD, AST_FLAGS_NONE, 0);
        AstNode* field_node = parser_get_node(p, field_node_id);

        field_node -> as.field.name = string_intern_token(p -> current_file -> id, field_name_token);
        field_node -> as.field.type_expr = type_expr_id;

        field_node -> tokens.start = starting_index;
        field_node -> tokens.end = p -> cursor;

        node = parser_get_node(p, id);

        ast_id_list_append(&node -> as.union_decl.fields, &p -> current_file -> ast, field_node_id);

        parser_advance(p); // advance past ';'
    }

    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past '}'

    return id;
}
