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

#endif // !LILY_AST_PARSER_EXPR_TYPES_H
