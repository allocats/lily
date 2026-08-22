#include "ast/nodes/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "token/types.h"

AstNodeId parse_return_statement(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_RETURN_STMT, AST_FLAGS_NONE, 0);

    parser_advance(p); // advance past "return"

    AstNodeId return_expr = parse_expression(p, 0);

    if (IS_NODE_ERROR(p, return_expr)) {
        return parser_error(p, id, RECOVERY_STMT);
    }

    if (!parser_check(p, TOK_SEMI)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here after the return statement's expresion"
        );

        return parser_error(p, id, RECOVERY_STMT);
    }

    AstNode* node = parser_get_node(p, id);

    node -> as.return_stmt.expr = return_expr;
    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past the terminating ';'

    return id;
}
