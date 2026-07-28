#ifndef LILY_TOKENS_TYPES_H
#define LILY_TOKENS_TYPES_H

#include "meowrena/meowrena.h"
#include "utils/types.h"

#define TOKENS(X)       \
    X(TOK_UNKNOWN)      \
    X(TOK_ERROR)        \
    X(TOK_EOF)          \
                        \
    X(TOK_FLOAT_LIT)    \
    X(TOK_INTEGER_LIT)  \
    X(TOK_STRING_LIT)   \
    X(TOK_CHAR_LIT)     \
                        \
    X(TOK_IDENT)        \
                        \
    X(TOK_FALSE)        \
    X(TOK_TRUE)         \
    X(TOK_NULL)         \
                        \
    X(TOK_IMPORT)       \
    X(TOK_MODULE)       \
                        \
    X(TOK_EXTERNAL)     \
    X(TOK_FN)           \
                        \
    X(TOK_ENUM)         \
    X(TOK_UNION)        \
    X(TOK_STRUCT)       \
                        \
    X(TOK_IF)           \
    X(TOK_ELSE)         \
                        \
    X(TOK_FOR)          \
    X(TOK_WHILE)        \
                        \
    X(TOK_LET)          \
    X(TOK_CONST)        \
    X(TOK_DEFER)        \
    X(TOK_RETURN)       \
    X(TOK_BREAK)        \
    X(TOK_CONTINUE)     \
                        \
    X(TOK_LPAREN)       \
    X(TOK_RPAREN)       \
    X(TOK_LBRACE)       \
    X(TOK_RBRACE)       \
    X(TOK_LBRACKET)     \
    X(TOK_RBRACKET)     \
                        \
    X(TOK_DOT)          \
    X(TOK_DOT_DOT)      \
    X(TOK_DOT_DOT_DOT)  \
    X(TOK_COMMA)        \
    X(TOK_COLON)        \
    X(TOK_COLON_COLON)  \
    X(TOK_SEMI)         \
    X(TOK_ARROW)        \
                        \
    X(TOK_PLUS)         \
    X(TOK_MINUS)        \
    X(TOK_STAR)         \
    X(TOK_SLASH)        \
    X(TOK_PERCENT)      \
                        \
    X(TOK_PLUS_EQ)      \
    X(TOK_MINUS_EQ)     \
    X(TOK_STAR_EQ)      \
    X(TOK_SLASH_EQ)     \
    X(TOK_PERCENT_EQ)   \
                        \
    X(TOK_EQ)           \
    X(TOK_EQ_EQ)        \
    X(TOK_BANG)         \
    X(TOK_BANG_EQ)      \
                        \
    X(TOK_AMP)          \
    X(TOK_AMP_AMP)      \
    X(TOK_AMP_EQ)       \
                        \
    X(TOK_PIPE)         \
    X(TOK_PIPE_PIPE)    \
    X(TOK_PIPE_EQ)      \
                        \
    X(TOK_TILDE)        \
    X(TOK_TILDE_EQ)     \
                        \
    X(TOK_CARET)        \
    X(TOK_CARET_EQ)     \
                        \
    X(TOK_LT)           \
    X(TOK_LT_EQ)        \
    X(TOK_SHL)          \
    X(TOK_SHL_EQ)       \
                        \
    X(TOK_GT)           \
    X(TOK_GT_EQ)        \
    X(TOK_SHR)          \
    X(TOK_SHR_EQ)       \
                        \
    X(TOKEN_KIND_COUNT) \

typedef enum {
    TOKENS(GENERATE_ENUM)
} TokenKind;

static const char* TOKEN_KIND_STRS[] = {
    TOKENS(GENERATE_STRING)
};

#undef TOKENS

typedef struct {
    TokenKind kind;
    str8 lexeme;
} Token;

typedef struct {
    Arena arena;
    Token* items;
    u32 count;
    u32 capacity;
} TokenArray;

#endif // !LILY_TOKENS_TYPES_H
