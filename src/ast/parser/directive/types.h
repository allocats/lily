#ifndef LILY_AST_PARSER_DIRECTIVES_TYPES_H
#define LILY_AST_PARSER_DIRECTIVES_TYPES_H

#include "ast/nodes/types.h"
#include "ids.h"

typedef enum {
    DIRECTIVE_STATEMENT,
    DIRECTIVE_ATTRIBUTE_FLAG,
    DIRECTIVE_ATTRIBUTE_VALUE,
} DirectiveKind;

typedef struct {
    DirectiveKind kind;

    union {
        AstNodeKind kind;
        AstNodeId   value;
        u32         flag;
    } as;
} DirectiveInfo;

#endif // !LILY_AST_PARSER_DIRECTIVES_TYPES_H
