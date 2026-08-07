#ifndef LILY_AST_NODES_H
#define LILY_AST_NODES_H

#include "ast/nodes/types.h"

#include "ast/tree/types.h"

#define AST_NODE_ID_LOOKUP(ast, id)     (ast -> nodes[id])
#define AST_NODE_ID_LOOKUP_REF(ast, id) (&(ast) -> nodes[id])

AstNodeId ast_node_alloc(Arena* arena, Ast* ast);
AstNode*  ast_node_get(Ast* ast, AstNodeId id);

void ast_block_push_stmt(Arena* arena, AstNode* block, AstNodeId stmt);

bool ast_is_kind(Ast* ast, AstNodeId id, AstKind kind);

#endif // !LILY_AST_NODES_H
