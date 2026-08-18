#ifndef LILY_PARSER_TYPES_H
#define LILY_PARSER_TYPES_H

#include "files/types.h"
#include "token/types.h"

typedef struct {
    File* current_file;

    // points to: current_file -> tokens
    TokenArray* tokens_array;

    u32 cursor;
    u32 token_count;
} Parser;

#endif // !LILY_PARSER_TYPES_H
