#include "ast/parser/parser.h"
#include "ast/tree/tree.h"

void parser_recover_decl(Parser* p) {
    while (p -> cursor < p -> token_count) {
        TokenKind kind = parser_peek(p).kind;

        if (
            kind == TOK_KW_MODULE      ||
            kind == TOK_KW_IMPORT      ||
            kind == TOK_KW_EXTERNAL    ||
            kind == TOK_KW_CONST       ||
            kind == TOK_KW_FN          ||
            kind == TOK_KW_STRUCT      ||
            kind == TOK_KW_ENUM        ||
            kind == TOK_EOF
        ) {
            return;
        }

        parser_advance(p);
    }
}

AstNodeId parser_error_decl(Parser* p, AstNodeId id) {
    parser_recover_decl(p);

    AstNode* node = ast_get_node(&p -> current_file -> ast, id);

    node -> kind = AST_ERROR;
    node -> tokens.end = p -> cursor;

    return node -> id;
}
