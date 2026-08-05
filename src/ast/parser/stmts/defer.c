#include "ast/nodes/nodes.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"

AstNodeId parse_defer_stmt(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_DEFER);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    node -> as.defer_stmt.stmt = parse_expression(p, 0);

    if (!parser_check(p, TOK_SEMI)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );

        return parser_error_stmt(p, node);
    }

    parser_advance(p);

    node -> token_span.end = p -> cursor;

    return id;
}
