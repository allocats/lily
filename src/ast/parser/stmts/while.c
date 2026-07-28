#include "ast/nodes/nodes.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"

AstNodeId parse_while_loop(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_WHILE);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    node -> as.while_loop.condition = parse_expression(p, 0);

    if (!parser_check(p, TOK_LBRACE)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected '{'",
            "add a '{' here"
        );

        return parser_error_stmt(p, node);
    }

    parser_advance(p);

    node -> as.while_loop.block = parse_block(p);

    return id;
}
