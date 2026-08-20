#include "ast/parser/parser.h"
#include "ids.h"
#include "token/types.h"

void parser_recover_stmt(Parser* p) {
    while (p -> cursor < p -> token_count) {
        TokenKind kind = parser_peek(p).kind;

        if (
            kind == TOK_R_BRACE ||
            kind == TOK_SEMI    ||
            kind == TOK_EOF
        ) {
            return;
        }

        parser_advance(p);
    }
}

AstNodeId parser_error_stmt(Parser* p, AstNodeId id) {
    parser_recover_stmt(p);

    AstNode* node = parser_get_node(p, id);
    node -> kind = AST_ERROR;
    node -> tokens.end = p -> cursor;
    return node -> id;
}
