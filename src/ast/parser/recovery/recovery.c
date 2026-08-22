#include "ast/parser/parser.h"
#include "ast/parser/types.h"
#include "ast/parser/recovery/types.h"
#include "ids.h"
#include "token/types.h"

static const bool STMT_SYNC_TOKENS[] = {
    [TOK_EOF]       = true,

    [TOK_EOF + 1 ... TOK_KW_IF - 1] = false,

    [TOK_KW_IF]     = true,
    [TOK_KW_SWITCH] = true,
    [TOK_KW_FOR]    = true,
    [TOK_KW_WHILE]  = true,
    [TOK_KW_DEFER]  = true,
    [TOK_KW_RETURN] = true,

    [TOK_KW_RETURN + 1 ... TOK_R_BRACE - 1] = false,

    [TOK_R_BRACE]   = true,
    [TOK_SEMI]      = true,

    [TOK_SEMI + 1 ... TOK_HASHTAG - 1] = false,
    
    [TOK_HASHTAG]   = true,
};

static const bool EXPR_SYNC_TOKENS[] = {
    [TOK_EOF]       = true,

    [TOK_EOF + 1 ... TOK_R_PAREN - 1] = false,

    [TOK_R_PAREN]   = true,
    [TOK_R_PAREN + 1 ... TOK_R_BRACE - 1] = false,

    [TOK_R_BRACE]   = true,
    [TOK_R_BRACE + 1 ... TOK_R_BRACKET - 1] = false,
    
    [TOK_R_BRACKET] = true,
    [TOK_R_BRACKET + 1 ... TOK_COMMA - 1] = false,

    [TOK_COMMA]     = true,
    [TOK_COMMA + 1 ... TOK_SEMI - 1] = false,

    [TOK_SEMI]      = true,
};

static bool is_sync_token(RecoveryKind recovery_kind, TokenKind token_kind) {
    switch (recovery_kind) {
        case RECOVERY_DECL:
            return token_kind == TOK_HASHTAG || token_kind == TOK_EOF;

        case RECOVERY_STMT:
            return STMT_SYNC_TOKENS[token_kind];

        case RECOVERY_EXPR:
        case RECOVERY_TYPE:
            return EXPR_SYNC_TOKENS[token_kind];

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
