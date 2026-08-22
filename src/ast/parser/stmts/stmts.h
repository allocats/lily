#ifndef LILY_AST_PARSER_STMTS_H
#define LILY_AST_PARSER_STMTS_H

#include "ast/parser/types.h"
#include "ids.h"

// dispatcher
AstNodeId parse_statement(Parser* p);
AstNodeId parse_block(Parser* p);

AstNodeId parse_variable_decl(Parser* p);
AstNodeId parse_defer_statement(Parser* p);
AstNodeId parse_return_statement(Parser* p);

AstNodeId parse_if_statement(Parser* p);
AstNodeId parse_switch_statement(Parser* p);

AstNodeId parse_for_loop(Parser* p);
AstNodeId parse_while_loop(Parser* p);

#endif // !LILY_AST_PARSER_STMTS_H
