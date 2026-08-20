#ifndef LILY_AST_PARSER_EXPR_H
#define LILY_AST_PARSER_EXPR_H

#include "ids.h"
#include "ast/parser/types.h"

AstNodeId parse_expression(Parser* p, u32 prec);

#endif // !LILY_AST_PARSER_EXPR_H
