#ifndef LILY_AST_PARSER_RECOVERY_H
#define LILY_AST_PARSER_RECOVERY_H

#include "ast/parser/recovery/types.h"
#include "ast/parser/types.h"
#include "ids.h"

void parser_recover(Parser* p, RecoveryKind kind); // stops at sync token
void parser_recover_and_advance(Parser* p, RecoveryKind kind); // advances past sync token

AstNodeId parser_error(Parser* p, AstNodeId id, RecoveryKind kind);
AstNodeId parser_error_and_advance(Parser* p, AstNodeId id, RecoveryKind kind);

#endif // !LILY_AST_PARSER_RECOVERY_H
