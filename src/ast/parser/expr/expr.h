#ifndef LILY_AST_PARSER_EXPR_H
#define LILY_AST_PARSER_EXPR_H

#include "ast/parser/types.h"

typedef struct {
    u8 lbp;   // left binding power — 0 means "not an infix/postfix operator"
    u8 rbp;   // binding power used when parsing the RHS
} OpInfo;

// helper: L(n) = left-assoc at level n, R(n) = right-assoc at level n
#define L(n) {(n), (n)+1}
#define R(n) {(n), (n)}

static OpInfo op_table[] = {
    // assignment — lowest, right-assoc
    [TOK_EQ]          = R(10),
    [TOK_PLUS_EQ]     = R(10),
    [TOK_MINUS_EQ]    = R(10),
    [TOK_STAR_EQ]     = R(10),
    [TOK_SLASH_EQ]    = R(10),
    [TOK_PERCENT_EQ]  = R(10),
    [TOK_AMP_EQ]      = R(10),
    [TOK_PIPE_EQ]     = R(10),
    [TOK_TILDE_EQ]    = R(10),
    [TOK_CARET_EQ]    = R(10),
    [TOK_SHL_EQ]      = R(10),
    [TOK_SHR_EQ]      = R(10),

    // logical
    [TOK_PIPE_PIPE]   = L(20),
    [TOK_AMP_AMP]     = L(30),

    // bitwise
    [TOK_PIPE]        = L(40),
    [TOK_CARET]       = L(50),
    [TOK_AMP]         = L(60),

    // equality
    [TOK_EQ_EQ]       = L(70),
    [TOK_BANG_EQ]     = L(70),

    // relational
    [TOK_LT]          = L(80),
    [TOK_LT_EQ]       = L(80),
    [TOK_GT]          = L(80),
    [TOK_GT_EQ]       = L(80),

    // shift
    [TOK_SHL]         = L(90),
    [TOK_SHR]         = L(90),

    // additive
    [TOK_PLUS]        = L(100),
    [TOK_MINUS]       = L(100),

    // multiplicative
    [TOK_STAR]        = L(110),
    [TOK_SLASH]       = L(110),
    [TOK_PERCENT]     = L(110),

    // postfix: calls, indexing, member access — tightest
    [TOK_LPAREN]      = L(140),
    [TOK_LBRACKET]    = L(140),
    [TOK_ARROW]       = L(140),
    [TOK_DOT]         = L(140),

    [TOK_COLON_COLON] = L(150),
};

#define OP_TABLE_LEN (sizeof(op_table) / sizeof(op_table[0]))

AstNodeId parse_expression(Parser* p, i32 min_bp);

#endif // !LILY_AST_PARSER_EXPR_H
