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

AstNodeId parse_while_loop(Parser* p) {
    AstNodeId id = parser_create_node(p, AST_WHILE_LOOP, AST_FLAGS_NONE, 0);

    parser_advance(p); // advance past "while"

    AstNodeId condition_expr_id = parse_expression(p, 0);

    if (IS_NODE_ERROR(p, condition_expr_id)) {
        return parser_error(p, id, RECOVERY_STMT);
    }

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

        return parser_error(p, id, RECOVERY_STMT);
    }

    AstNodeId block_id = parse_block(p);

    if (IS_NODE_ERROR(p, block_id)) {
        return parser_error(p, id, RECOVERY_STMT);
    }

    node = parser_get_node(p, id);

    node -> as.while_loop.block = block_id;
    node -> tokens.end = p -> cursor - 1;

    return id;
}
