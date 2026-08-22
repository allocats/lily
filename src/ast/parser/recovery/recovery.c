#include "ast/parser/parser.h"
#include "ast/parser/types.h"
#include "ast/parser/recovery/types.h"
#include "ids.h"
#include "token/types.h"

static constexpr u64 stmt_sync_tokens = (u64)  0
                                      | ((u64) 1 << TOK_EOF)
                                      | ((u64) 1 << TOK_KW_IF)
                                      | ((u64) 1 << TOK_KW_SWITCH)
                                      | ((u64) 1 << TOK_KW_FOR)
                                      | ((u64) 1 << TOK_KW_WHILE)
                                      | ((u64) 1 << TOK_KW_DEFER)
                                      | ((u64) 1 << TOK_KW_RETURN)
                                      | ((u64) 1 << TOK_R_BRACE)
                                      | ((u64) 1 << TOK_SEMI)
                                      | ((u64) 1 << TOK_HASHTAG);

static constexpr u64 expr_sync_tokens = (u64)  0
                                      | ((u64) 1 << TOK_EOF)
                                      | ((u64) 1 << TOK_R_PAREN)
                                      | ((u64) 1 << TOK_R_BRACE)
                                      | ((u64) 1 << TOK_R_BRACKET)
                                      | ((u64) 1 << TOK_COMMA)
                                      | ((u64) 1 << TOK_SEMI);

static bool is_sync_token(RecoveryKind recovery_kind, TokenKind token_kind) {
    switch (recovery_kind) {
        case RECOVERY_DECL:
            return token_kind == TOK_HASHTAG || token_kind == TOK_EOF;

        case RECOVERY_STMT:
            return stmt_sync_tokens & (1 << token_kind); 

        case RECOVERY_EXPR:
        case RECOVERY_TYPE:
            return expr_sync_tokens & (1 << token_kind);

        case RECOVERY_BLOCK:
            return token_kind == TOK_R_BRACE || token_kind == TOK_EOF;

        case RECOVERY_NONE:
            return true;
    }

    return false;
}

// stops at sync token
void parser_recover(Parser* p, RecoveryKind kind) {
    while (p -> cursor < p -> token_count) {
        TokenKind token_kind = parser_peek(p).kind;

        if (is_sync_token(kind, token_kind)) {
            return;
        }

        parser_advance(p);
    }
}

// advances past sync token
void parser_recover_and_advance(Parser* p, RecoveryKind kind) {
    parser_recover(p, kind);

    if (!parser_check(p, TOK_EOF)) {
        parser_advance(p);
    }
}

AstNodeId parser_error(Parser* p, AstNodeId id, RecoveryKind kind) {
    parser_recover(p, kind);

    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_ERROR;
    node -> tokens.end = p -> cursor;
    
    return id;
}

AstNodeId parser_error_and_advance(Parser* p, AstNodeId id, RecoveryKind kind) {
    parser_recover_and_advance(p, kind);

    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_ERROR;
    node -> tokens.end = p -> cursor - 1;
    
    return id;
}
