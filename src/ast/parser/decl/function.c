#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types/ty.h"
#include "ast/parser/parser.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"

AstNodeId parse_function_decl(Parser* p, StringId name) {
    AstNodeId id = parser_create_node(p, AST_FUNCTION_DECL, AST_FLAGS_IS_TOP_DECL, -2);
    AstNode* node = parser_get_node(p, id);

    node -> as.function_decl.name = name;

    if (!parser_check(p, TOK_L_PAREN)) {
        Token token = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &token,
            DIAG_LOC_START_OF_TOK,
            "expected '(' after 'fn'",
            "add a '(' here"
        );

        return parser_recover_end_of_fn(p, id);
    }

    parser_advance(p);

    while (p -> cursor < p -> token_count) {
        if (parser_check(p, TOK_R_PAREN)) {
            parser_advance(p);
            break;
        }

        Token param_name_token = parser_advance(p);

        if (param_name_token.kind != TOK_IDENT) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &param_name_token,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier here"
            );

            return parser_recover_end_of_fn(p, id);
        }

        AstNodeId param_node_id = parser_create_node(p, AST_PARAMETER, AST_FLAGS_NONE,  -1);
        AstNode* param_node = parser_get_node(p, param_node_id);

        param_node -> as.parameter_decl.name = string_intern_token(
            p -> current_file -> id,
            param_name_token
        ); 

        if (!parser_check(p, TOK_COLON)) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &param_name_token,
                DIAG_LOC_END_OF_TOK,
                "expected ':'",
                "add a ':' here"
            );

            return parser_recover_end_of_fn(p, id);
        }

        parser_advance(p); // advance past ':'

        AstNodeId param_type_expr_id = parse_param_type_expr(p);

        if (param_type_expr_id == AST_NODE_ID_NONE) {
            return parser_recover_end_of_fn(p, id);
        }

        AstNode* param_type_node = parser_get_node(p, param_type_expr_id);

        param_node = parser_get_node(p, param_node_id);
        param_node -> as.parameter_decl.type_expr = param_type_expr_id;

        node = parser_get_node(p, id);

        if (param_type_node -> kind == AST_TYPE_VARIADIC) {
            node -> flags |= AST_FLAGS_IS_VARIADIC;

            if (parser_check(p, TOK_COMMA)) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "variadic parameter must be the last parameter",
                    "remove the parameters after '...'"
                );

                return parser_recover_end_of_fn(p, id);
            }

            if (!parser_check(p, TOK_R_PAREN)) {
                Token token = parser_peek_previous(p);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here"
                );

                return parser_recover_end_of_fn(p, id);
            }

            parser_advance(p);
            break;
        }

        if (is_node_constant(&p -> current_file -> ast, param_type_expr_id)) {
            param_node -> flags |=  AST_FLAGS_IS_CONSTANT;
        }

        ast_id_list_append(
            &node -> as.function_decl.parameters,
            &p -> current_file -> ast,
            param_node_id
        );

        if (parser_check(p, TOK_R_PAREN)) {
            parser_advance(p);
            break;
        }

        if (parser_check(p, TOK_COMMA)) {
            parser_advance(p);
            continue;
        }

        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ',' or ')'",
            "add a ',' or ')' here"
        );

        return parser_recover_end_of_fn(p, id);
    }

    if (parser_check(p, TOK_ARROW)) {
        parser_advance(p);

        AstNodeId return_type_expr = parse_type_expr(p);

        node = parser_get_node(p, id);
        node -> as.function_decl.return_type_expr = return_type_expr;
    }

    if (!parser_check(p, TOK_L_BRACE)) {
        Token token = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &token,
            DIAG_LOC_END_OF_TOK,
            "expected '{' for function body",
            "add a '{' here" 
        );

        return parser_recover_end_of_fn(p, id);
    }

    AstNodeId block_id = parse_block(p);

    node = parser_get_node(p, id);

    node -> as.function_decl.block = block_id;
    node -> tokens.end = p -> cursor;

    return id;
}
