#ifndef LILY_PARSER_H
#define LILY_PARSER_H

#include "ast/nodes/types.h"
#include "parser/types.h"

void parse_file(FileId id);

AstNodeId parser_create_node(Parser* p, AstNodeKind kind, u16 flags);

Token parser_peek(Parser* p);
Token parser_advance(Parser* p);
bool  parser_check(Parser* p, TokenKind kind);

#endif // !LILY_PARSER_H
