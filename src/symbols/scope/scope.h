#ifndef LILY_SYMBOLS_SCOPE_H
#define LILY_SYMBOLS_SCOPE_H

#include "ids.h"

void scope_init(ScopeId id);

SymbolId scope_intern(ScopeId scope_id, FileId file_id, StringId name_id, AstNodeId node_id);
SymbolId scope_lookup(ScopeId scope_id, StringId name_id);

#endif // !LILY_SYMBOLS_SCOPE_H
