#ifndef LILY_SYMBOLS_H
#define LILY_SYMBOLS_H

#include "driver/types.h"
#include "modules/types.h"
#include "symbols/types.h"

extern LilyCtx driver_ctx;

void scope_init(Scope* scope);

void symbols_register_top_level_declarations(ModuleId id);
void symbols_resolve(ModuleId id);

SymbolId scope_add_sym(Resolver* r, AstNodeId node_id, StringId name, SymbolKind kind);
SymbolId scope_get_sym(Resolver* r, StringId name, u32 hash);
SymbolId scope_get_sym_scope_id(Resolver* r, ScopeId scope_id, StringId name, u32 hash);

SymbolId table_get_sym(Resolver* r, StringId name);

ScopeId scope_enter(Resolver* r);
ScopeId scope_exit(Resolver* r);

void sym_add_function(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_add_macro(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_add_struct(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_add_union(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_add_enum(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_add_const(Resolver* r, AstNode* node, AstNodeId node_id);
void sym_add_var(Resolver* r, AstNode* node, AstNodeId node_id);

void resolve_symbol(Resolver* r, Symbol* sym);

void resolve_function(Resolver* r, Symbol* sym);

void resolve_block(Resolver* r, AstNodeId block_id);

void resolve_stmt(Resolver* r, AstNodeId block_id);

void resolve_expr(Resolver* r, AstNodeId expr_id);

void resolve_type_expr(Resolver* r, AstNodeId type_expr_id);

#endif // !LILY_SYMBOLS_H
