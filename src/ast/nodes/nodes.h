#ifndef LILY_AST_NODES_H
#define LILY_AST_NODES_H

#include "ast/nodes/types.h"
#include "ast/tree/types.h"

void ast_id_list_init(Arena* arena, AstNodeIdList* list, u32 count);
void ast_id_list_append(AstNodeIdList* list, Ast* ast, AstNodeId id);

bool is_node_constant(Ast* ast, AstNodeId id);

#endif // !LILY_AST_NODES_H
