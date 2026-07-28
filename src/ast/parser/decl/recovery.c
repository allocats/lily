#include "ast/parser/parser.h"

void parser_recover_decl(Parser* p) {
    while (p -> cursor < p -> token_count) {
        TokenKind kind = parser_peek(p) -> kind;

        if (
            kind == TOK_MODULE      ||
            kind == TOK_IMPORT      ||
            kind == TOK_EXTERNAL    ||
            kind == TOK_CONST       ||
            kind == TOK_FN          ||
            kind == TOK_STRUCT      ||
            kind == TOK_ENUM        ||
            kind == TOK_EOF
        ) {
            return;
        }

        parser_advance(p);
    }
}

AstNodeId parser_error_decl(Parser* p, AstNode* node) {
    parser_recover_decl(p);
    node -> kind = AST_ERROR;
    return node -> id;
}
