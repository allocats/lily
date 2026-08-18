#ifndef LILY_AST_TREE_TYPES_H
#define LILY_AST_TREE_TYPES_H

#include "ast/nodes/types.h"

typedef struct {
    // used for AstNodeIdLists
    Arena gpa;

    // used for nodes ONLY, to optimise 
    // the memory usage of the nodes array 
    Arena nodes_arena;

    AstNode* nodes;
    u64 count;
    u64 capacity;
} Ast;

#endif // !LILY_AST_TREE_TYPES_H
