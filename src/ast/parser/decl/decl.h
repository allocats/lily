#ifndef LILY_AST_PARSER_DECL_H
#define LILY_AST_PARSER_DECL_H

#include "ast/nodes/types.h"
#include "ast/parser/types.h"

// parsing
AstNodeId parse_import_decl(Parser* p);
AstNodeId parse_module_decl(Parser* p);
AstNodeId parse_enum_decl(Parser* p);
AstNodeId parse_union_decl(Parser* p);
AstNodeId parse_struct_decl(Parser* p);
AstNodeId parse_function_decl(Parser* p);
AstNodeId parse_macro_decl(Parser* p);

// can be a struct or function 
AstNodeId parse_external_decl(Parser* p);

// recovery
AstNodeId parser_error_decl(Parser* p, AstNode* node);

// for main parser loop
void parser_recover_decl(Parser* p);

#endif // !LILY_AST_PARSER_DECL_H
