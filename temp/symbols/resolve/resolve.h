#ifndef LILY_SYMBOLS_RESOLVE_H
#define LILY_SYMBOLS_RESOLVE_H

#include "ast/nodes/types.h"
#include "symbols/types.h"

void sym_resolve_constant(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_resolve_enum(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_resolve_function(Resolver* r, Symbol* symbol, AstNodeId node_id);
void sym_resolve_macro(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_resolve_struct(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_resolve_union(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_resolve_variable(Resolver* r, AstNode* node, AstNodeId node_id);

#endif // !LILY_SYMBOLS_RESOLVE_H
