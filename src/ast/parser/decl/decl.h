#ifndef LILY_AST_PARSER_DECL_H
#define LILY_AST_PARSER_DECL_H

#include "ast/parser/types.h"

void parse_function_decl(Parser* p, StringId name);

// recovery
AstNodeId parser_error_decl(Parser* p, AstNodeId id);

// for main parser loop
void parser_recover_decl(Parser* p);

#endif // !LILY_AST_PARSER_DECL_H
