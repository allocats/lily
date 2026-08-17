#ifndef LILY_LEXER_TYPES_H
#define LILY_LEXER_TYPES_H

#include "utils/types.h"
#include "token/types.h"

static constexpr u32 DELIMITER_STACK_MAX_DEPTH = 2048;

typedef struct {
    u32 top;
    u32 items[DELIMITER_STACK_MAX_DEPTH];
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
