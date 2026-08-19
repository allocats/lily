#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/parser.h"

void parse_function_decl(Parser* p, StringId name) {
    AstNodeId id = parser_create_node(p, AST_FUNCTION_DECL, AST_FLAGS_IS_TOP_DECL, -2);
    AstNode* node = parser_get_node(p, id);
}
