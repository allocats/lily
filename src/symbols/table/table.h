#ifndef LILY_SYMBOLS_TABLE_H
#define LILY_SYMBOLS_TABLE_H

#include "ids.h"
#include "utils/types.h"

#define SCOPE_ID_LOOKUP(i)      ( driver.symbol_table.scopes[i])
#define SCOPE_ID_LOOKUP_REF(i)  (&driver.symbol_table.scopes[i])

#define SYMBOL_ID_LOOKUP(i)      ( driver.symbol_table.symbols[i])
#define SYMBOL_ID_LOOKUP_REF(i)  (&driver.symbol_table.symbols[i])

void symbol_table_init(u32 count);

ScopeId  symbol_table_alloc_scope(void);
SymbolId symbol_table_alloc_symbol(void);

SymbolId symbol_table_lookup_top_level(ScopeId id, StringId name);
SymbolId symbol_table_lookup(ScopeId scope_id, StringId name_id, FileId file_id);

#endif // !LILY_SYMBOLS_TABLE_H
