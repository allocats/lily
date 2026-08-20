#include "ast/nodes/types.h"
#include "ast/parser/parser.h"

AstNodeId parse_enum_decl(Parser *p, StringId name) {
    AstNodeId id = parser_create_node(p, AST_ENUM_DECL, AST_FLAGS_IS_TOP_DECL, -2);
    AstNode* node = parser_get_node(p, id);

    node -> as.function_decl.name = name;
    
    return id;
}
