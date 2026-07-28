#ifndef LILY_AST_PARSER_TYPES_H
#define LILY_AST_PARSER_TYPES_H

#include "../../modules/types.h"
#include "../../utils/types.h"

typedef struct {
    FileId id;
    Module* module;
    TokenArray* tokens;
    u32 cursor;
    u32 token_count;
} Parser;

#endif // !LILY_AST_PARSER_TYPES_H
