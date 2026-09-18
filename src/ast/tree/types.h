#ifndef LILY_AST_TREE_TYPES_H
#define LILY_AST_TREE_TYPES_H

#include "ast/nodes/types.h"

typedef struct {
    // used for AstNodeIdLists
    Arena gpa;

    // used for nodes ONLY, to optimise 
    // the memory usage of the nodes array 
    Arena nodes_arena;

    // used for attributes ONLY
    Arena attributes_arena;

    AstNode* nodes;
    u32 node_count;
    u32 node_capacity;

    // each node can have multiple attributes
    AstNodeIdList* attributes;
    u32 attribute_count;
    u32 attribute_capacity;

    // ALL declarations, used for the global table allocation
    u32 declaration_count;

    // symbols to be exported, functions, structs, enums, unions and globals, used for scope allocation
    u32 top_level_declaration_count;
} Ast;

#endif // !LILY_AST_TREE_TYPES_H
