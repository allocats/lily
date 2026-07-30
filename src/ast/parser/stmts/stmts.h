#ifndef LILY_AST_PARSER_STMTS_H
#define LILY_AST_PARSER_STMTS_H

#include "ast/nodes/types.h"
#include "ast/parser/parser.h"

// recovery
void     parser_recover_stmt(Parser* p);
AstNodeId parser_error_stmt(Parser* p, AstNode* node);

AstNodeId parse_block(Parser* p);
AstNodeId parse_var_decl(Parser* p);
AstNodeId parse_const_decl(Parser* p);

AstNodeId parse_while_loop(Parser* p);
AstNodeId parse_for_loop(Parser* p);
AstNodeId parse_if_stmt(Parser* p);

AstNodeId parse_defer_stmt(Parser* p);
AstNodeId parse_return_stmt(Parser* p);

#endif // !LILY_AST_PARSER_STMTS_H
