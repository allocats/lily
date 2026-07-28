#include "ast/parser/parser.h"

void parser_recover_stmt(Parser* p) {
    while (p -> cursor < p -> token_count) {
        TokenKind kind = parser_peek(p) -> kind;

        if (
            kind == TOK_RBRACE  ||
            kind == TOK_SEMI
        ) {
            parser_advance(p);
            return;
        }

        if (
            kind == TOK_EOF
        ) {
            return;
        }

        parser_advance(p);
    }
}

AstNodeId parser_error_stmt(Parser* p, AstNode* node) {
    parser_recover_stmt(p);
    node -> kind = AST_ERROR;
    return node -> id;
}
