#ifndef LILY_AST_PARSER_TYPES_TY_H
#define LILY_AST_PARSER_TYPES_TY_H

#include "ast/parser/parser.h"

typedef u32 TypeId;

TypeId parse_type(Parser* p);

#endif // !LILY_AST_PARSER_TYPES_TY_H
