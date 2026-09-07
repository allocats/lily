#ifndef LILY_SYMBOLS_RESOLVE_H
#define LILY_SYMBOLS_RESOLVE_H

#include "ids.h"
#include "files/types.h"

void resolve_symbols(void);
bool resolve_symbol(SymbolId id);
SymbolId resolve_name_expr(File* file, AstNodeId node_id);
StringId resolve_name_id(File* file, AstNodeId node_id);

#endif // !LILY_SYMBOLS_RESOLVE_H
