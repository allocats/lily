#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"

AstNodeId parse_defer_statement(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_DEFER_STMT, AST_FLAGS_NONE, 0);

    parser_advance(p); // advance past "defer"

    // TODO: Think about this, could check in semantics that all 
    // statements are valid defers or create a block parser here
    AstNodeId deferred_statment = parse_statement(p);

    // if (!parser_check(p, TOK_SEMI)) {
    //     Token previous = parser_peek_previous(p);
    //
    //     diagnostic_add_token(
    //         p -> current_file -> id,
    //         DIAG_ERROR,
    //         &previous,
    //         DIAG_LOC_END_OF_TOK,
    //         "expected ';'",
    //         "add a ';' here after the deferred statement"
    //     );
    //
    //     return parser_error_stmt(p, id);
    // }

    AstNode* node = parser_get_node(p, id);

    node -> as.defer_stmt.stmt = deferred_statment;
    node -> tokens.end = p -> cursor;

    return id;
}
