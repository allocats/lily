#ifndef LILY_AST_PARSER_EXPR_TYPES_H
#define LILY_AST_PARSER_EXPR_TYPES_H

#include "token/types.h"
#include "utils/types.h"
#include <assert.h>

typedef struct {
    u8 lbp;
    u8 rbp;
} OpInfo;

// helper: L(n) = left-assoc at level n, R(n) = right-assoc at level n
#define L(n) { (n), (n) - 1 }
#define R(n) { (n), (n) }

static OpInfo op_table[] = {
    // Assignment
    [TOK_EQ]            = R(10),
    [TOK_PLUS_EQ]       = R(10),
    [TOK_MINUS_EQ]      = R(10),
    [TOK_STAR_EQ]       = R(10),
    [TOK_SLASH_EQ]      = R(10),
    [TOK_PERCENT_EQ]    = R(10),
    [TOK_SHL_EQ]        = R(10),
    [TOK_SHR_EQ]        = R(10),
    [TOK_AMP_EQ]        = R(10),
    [TOK_PIPE_EQ]       = R(10),
    [TOK_CARET_EQ]      = R(10),

    // Logical OR 
    [TOK_PIPE_PIPE]     = L(20),

    // Logical AND
    [TOK_AMP_AMP]       = L(30),

    // Bitwise OR 
    [TOK_PIPE]          = L(40),

    // Bitwise XOR 
    [TOK_CARET]         = L(50),

    // Bitwise AND
    [TOK_AMP]           = L(60),

    // Equality
    [TOK_BANG_EQ]       = L(70),
    [TOK_EQ_EQ]         = L(70),

    // Relational 
    [TOK_LT]            = L(80),
    [TOK_LT_EQ]         = L(80),
    [TOK_GT]            = L(80),
    [TOK_GT_EQ]         = L(80),

    // Shift
    [TOK_SHL]           = L(90),
    [TOK_SHR]           = L(90),

    // Additive 
    [TOK_PLUS]          = L(100),
    [TOK_MINUS]         = L(100),

    // Multiplicative 
    [TOK_STAR]          = L(110),
    [TOK_SLASH]         = L(110),
    [TOK_PERCENT]       = L(110),

    // Postfix
    [TOK_L_PAREN]       = L(120),
    [TOK_L_BRACKET]     = L(120),
    [TOK_ARROW]         = L(120),
    [TOK_DOT]           = L(120),

    // [TOK_L_BRACE]       = L(120),
};

static constexpr u32 OP_TABLE_LEN = (sizeof(op_table) / sizeof(op_table[0]));
static_assert(OP_TABLE_LEN > 0);

#endif // !LILY_AST_PARSER_EXPR_TYPES_H
