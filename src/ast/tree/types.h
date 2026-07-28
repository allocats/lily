#ifndef LILY_AST_TREE_TYPES_H
#define LILY_AST_TREE_TYPES_H

#include "../nodes/types.h"

typedef struct {
    Arena arena;

    AstNode* nodes;
    u32 count;
    u32 capacity;
} Ast;

#endif // !LILY_AST_TREE_TYPES_H
