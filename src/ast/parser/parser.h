#ifndef LILY_AST_PARSER_H
#define LILY_AST_PARSER_H

#include "ast/nodes/types.h"
#include "ast/parser/types.h"
#include "driver/types.h"

extern LilyCtx driver_ctx;

void parser_parse_file(FileId id);

AstNodeId parser_create_node(Parser* p, AstKind kind, u32 flags);

Token* parser_peek(Parser* p);
Token* parser_peek_previous(Parser* p);
Token* parser_advance(Parser* p);
bool   parser_check(Parser* p, TokenKind kind);

#endif // !LILY_AST_PARSER_H
