#ifndef LILY_PARSER_H
#define LILY_PARSER_H

#include "ast/nodes/types.h"
#include "ast/parser/types.h"

#define IS_NODE_ERROR(parser, id) (parser.current_file -> ast.nodes[id].kind == AST_ERROR)

void parse_file(FileId id);

AstNodeId parser_create_node(Parser* p, AstNodeKind kind, u16 flags, u32 start_offset);
AstNode*  parser_get_node(Parser* p, AstNodeId id);

Token parser_peek(Parser* p);
Token parser_advance(Parser* p);
bool  parser_check(Parser* p, TokenKind kind);

#endif // !LILY_PARSER_H
