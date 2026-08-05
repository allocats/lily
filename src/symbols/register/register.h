#ifndef LILY_SYMBOLS_REGISTER_H
#define LILY_SYMBOLS_REGISTER_H

#include "ast/nodes/types.h"
#include "resolver/types.h"

void register_symbol(Resolver* r, AstNode* node, AstNodeId node_id);

void sym_register_constant(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_register_enum(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_register_function(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_register_macro(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_register_struct(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_register_union(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_register_variable(Resolver* r, AstNode* node, AstNodeId node_id);

#endif // !LILY_SYMBOLS_REGISTER_H
