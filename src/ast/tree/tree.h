#ifndef LILY_AST_TREE_H
#define LILY_AST_TREE_H

#include "ast/tree/types.h"

void ast_init(Ast* ast);
void ast_print(char *path, FileId file_id);

AstNodeId ast_alloc_node(Ast* ast);
AstNode*  ast_get_node(Ast* ast, AstNodeId id);

#endif // !LILY_AST_TREE_H
