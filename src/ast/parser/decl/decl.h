#ifndef LILY_AST_PARSER_DECL_H
#define LILY_AST_PARSER_DECL_H

#include "ast/parser/types.h"

AstNodeId parse_top_level_decl(Parser* p);

AstNodeId parse_external_decl(Parser* p, StringId name);
AstNodeId parse_function_decl(Parser* p, StringId name);
AstNodeId parse_enum_decl(Parser* p, StringId name);
AstNodeId parse_struct_decl(Parser* p, StringId name);
AstNodeId parse_union_decl(Parser* p, StringId name);

// void parser_recover_decl(Parser* p);

#endif // !LILY_AST_PARSER_DECL_H
