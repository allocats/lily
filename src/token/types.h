#ifndef LILY_TOKEN_TYPES_H
#define LILY_TOKEN_TYPES_H

#include "meowrena/meowrena.h"
#include "utils/types.h"

#define TOKENS(X)       \
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
    X(TOK_KW_FALSE)     \
    X(TOK_KW_TRUE)      \
    X(TOK_KW_NULL)      \
                        \
    X(TOK_KW_EXTERNAL)  \
    X(TOK_KW_MACRO)     \
    X(TOK_KW_FN)        \
                        \
    X(TOK_KW_ENUM)      \
    X(TOK_KW_UNION)     \
    X(TOK_KW_STRUCT)    \
                        \
    X(TOK_KW_IF)        \
    X(TOK_KW_ELSE)      \
                        \
    X(TOK_KW_SWITCH)    \
    X(TOK_KW_CASE)      \
    X(TOK_KW_DEFAULT)   \
                        \
    X(TOK_KW_FOR)       \
    X(TOK_KW_WHILE)     \
                        \
    X(TOK_KW_CONST)     \
    X(TOK_KW_DEFER)     \
    X(TOK_KW_RETURN)    \
    X(TOK_KW_BREAK)     \
    X(TOK_KW_CONTINUE)  \
                        \
    X(TOK_L_PAREN)      \
    X(TOK_R_PAREN)      \
    X(TOK_L_BRACE)      \
    X(TOK_R_BRACE)      \
    X(TOK_L_BRACKET)    \
    X(TOK_R_BRACKET)    \
                        \
    X(TOK_DOT)          \
    X(TOK_DOT_DOT)      \
    X(TOK_ELLIPSIS)     \
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
    X(TOK_AT)           \
    X(TOK_HASHTAG)      \
    X(TOK_DOLLAR)       \
    X(TOK_QUESTION)     \
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
    X(TOKEN_KIND_COUNT)

typedef enum {
    TOKENS(GENERATE_ENUM)
} __attribute__((packed)) TokenKind;

static const char* TOKEN_KIND_STRS[] = {
    TOKENS(GENERATE_STRING)
};

#undef TOKENS

typedef struct {
    u32 start;
    u16 length;

    // packed to 1 byte, therefore max 255 (256th is reserved) token kinds
    TokenKind kind; 
} Token;

typedef struct {
    Arena arena;

    Token* items;
    u32 count;
    u32 capacity;
} TokenArray;

#endif // !LILY_TOKEN_TYPES_H
