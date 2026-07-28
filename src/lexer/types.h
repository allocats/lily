#ifndef LILY_LEXER_TYPES_H
#define LILY_LEXER_TYPES_H

#include "utils/types.h"
#include "token/types.h"

#define DELIMITER_STACK_MAX_DEPTH 1024

typedef struct {
    i32 top;
    Token* items[DELIMITER_STACK_MAX_DEPTH];
} DelimiterStack;

static const char CHAR_MAP[] = {
    ['0' ... '9'] = 1,

    ['a' ... 'z'] = 2,
    ['A' ... 'Z'] = 2,
    ['_'] = 2,
    
    ['@'] = 4,
    ['#'] = 4,
    ['&'] = 4,
    ['|'] = 4,
    ['~'] = 4,
    ['^'] = 4,
    ['-'] = 4,
    ['+'] = 4,
    ['/'] = 4,
    ['*'] = 4,
    ['%'] = 4,
    ['='] = 4,
    ['!'] = 4,
    ['<'] = 4,
    ['>'] = 4,
    ['.'] = 4,

    [','] = 8,
    ['['] = 8,
    [']'] = 8,
    ['('] = 8,
    [')'] = 8,
    ['{'] = 8,
    ['}'] = 8,
    [';'] = 8,
    [':'] = 8,
    ['\0'] = 8,

    [' '] = 16, 
    ['\t'] = 16, 
    ['\n'] = 16,
    ['\f'] = 16,
    ['\r'] = 16,

    ['\''] = 32,
    ['\"'] = 64,
};

#endif // !LILY_LEXER_TYPES_H
