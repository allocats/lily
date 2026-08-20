#ifndef LILY_AST_PARSER_TYPES_TY_H
#define LILY_AST_PARSER_TYPES_TY_H

#include "ast/parser/types.h"

// Caller must check for constants on the returned 
// type expression node and mark their identifier as constant
AstNodeId parse_param_type_expr(Parser* p);
AstNodeId parse_type_expr(Parser* p);

#endif // !LILY_AST_PARSER_TYPES_TY_H
