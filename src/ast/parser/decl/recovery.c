#include "ast/parser/parser.h"
#include "token/types.h"

void parser_recover_decl(Parser* p) {
    while (p -> cursor < p -> token_count) {
        TokenKind kind = parser_peek(p).kind;

        if (
            kind == TOK_HASHTAG     ||
            // kind == TOK_IDENT       ||
            kind == TOK_EOF
        ) {
            return;
        }

        parser_advance(p);
    }
}

AstNodeId parser_recover_end_of_fn(Parser* p, AstNodeId id) {
    i32 depth = 0;

    while (p -> cursor < p -> token_count) {
        TokenKind kind = parser_peek(p).kind;

        if (kind == TOK_L_BRACE) {
            depth++;
        } else if (kind == TOK_R_BRACE) {
            depth--;

            if (depth == 0) {
                parser_advance(p);
                break;
            }
        }

        parser_advance(p);
    }

    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_ERROR;
    node -> tokens.end = p -> cursor;

    return id;
}

AstNodeId parser_error_decl(Parser* p, AstNodeId id) {
    parser_recover_decl(p);

    AstNode* node = parser_get_node(p, id);
    node -> kind = AST_ERROR;
    node -> tokens.end = p -> cursor;
    return node -> id;
}
