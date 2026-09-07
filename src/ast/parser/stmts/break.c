#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/types.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include <stdio.h>

AstNodeId parse_break_statement(Parser* p) {
    AstNodeId id = parser_create_node(p, AST_BREAK_STMT, AST_FLAGS_NONE, 0);

    parser_advance(p); // advance past "break"

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

    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past ';'

    return id;
}
