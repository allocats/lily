#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "token/types.h"

AstNodeId parse_while_loop(Parser* p) {
    AstNodeId id = parser_create_node(p, AST_WHILE_LOOP, AST_FLAGS_NONE, 0);

    parser_advance(p); // advance past "while"

    AstNodeId condition_expr_id = parse_expression(p, 0);

    AstNode* node = parser_get_node(p, id);

    node -> as.while_loop.cond = condition_expr_id;

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

    node = parser_get_node(p, id);

    node -> as.while_loop.block = block_id;
    node -> tokens.end = p -> cursor - 1;

    return id;
}
