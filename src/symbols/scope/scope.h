#ifndef LILY_SYMBOLS_SCOPE_H
#define LILY_SYMBOLS_SCOPE_H

#include "ids.h"
#include "symbols/register/types.h"
#include "symbols/symbols/types.h"

void scope_init(ScopeId id);

SymbolId scope_intern(ScopeId scope_id, StringId name_id, SymbolKind kind);
SymbolId scope_lookup(ScopeId scope_id, StringId name_id);

SymbolId scope_add_symbol(ScopeId scope_id, SymbolId symbol_id);
ScopeId  scope_merge(ScopeId dest_id, ScopeId src_id);

SymbolId scope_intern_from_node(ScopeId scope_id, FileId file_id, StringId name_id, AstNodeId node_id);

ScopeId scope_enter(Registrar* r);
ScopeId scope_exit(Registrar* r);

#endif // !LILY_SYMBOLS_SCOPE_H
