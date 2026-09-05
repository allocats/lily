#ifndef LILY_SYMBOLS_REGISTER_H
#define LILY_SYMBOLS_REGISTER_H

#include "ast/nodes/types.h"
#include "ids.h"
#include "symbols/register/types.h"

SymbolId register_variable(Registrar* r, AstNode* node);

void symbols_register_builtin_types(void);
ScopeId register_top_level_symbols_for_file(FileId id);

#endif // !LILY_SYMBOLS_REGISTER_H
