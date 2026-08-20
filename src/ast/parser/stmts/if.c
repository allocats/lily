#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "token/types.h"

AstNodeId parse_if_statement(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_IF_STMT, AST_FLAGS_NONE, 0);
    AstNode* node = parser_get_node(p, id);

    u32 starting_index = p -> cursor;

    parser_advance(p); // advance past "if"

    while (p -> cursor < p -> token_count) {
        AstNodeId condition_expr_id = parse_expression(p, 0); 

        if (!parser_check(p, TOK_L_BRACE)) {
            Token previous = parser_peek_previous(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &previous,
                DIAG_LOC_END_OF_TOK,
                "expected '{'",
                "add a '{' here"
            );

            return parser_error_stmt(p, id);
        }

        AstNodeId block_id = parse_block(p);

        AstNodeId branch_id  = parser_create_node(p, AST_BRANCH, AST_FLAGS_NONE, p -> cursor - starting_index);
        AstNode* branch_node = parser_get_node(p, branch_id);

        branch_node -> as.branch.condition = condition_expr_id;
        branch_node -> as.branch.block = block_id;

        node = parser_get_node(p, id);

        ast_id_list_append(&node -> as.if_stmt.branches, &p -> current_file -> ast, branch_id);

        if (!parser_check(p, TOK_KW_ELSE)) {
            break;
        }

        parser_advance(p); // advance past "else"

        if (parser_check(p, TOK_L_BRACE)) {
            AstNodeId else_id = parse_block(p);

            node = parser_get_node(p, id);
            node -> as.if_stmt.else_block = else_id;
            break;
        }

        if (!parser_check(p, TOK_KW_IF)) {
            Token previous = parser_peek_previous(p);

            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &previous,
                DIAG_LOC_END_OF_TOK,
                "expected 'if'",
                "add a 'if' here"
            );

            return parser_error_stmt(p, id);
        }

        parser_advance(p);
    }

    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor - 1;

    return id;
}
