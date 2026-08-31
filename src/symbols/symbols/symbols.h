#ifndef LILY_SYMBOLS_SYMBOLS_H
#define LILY_SYMBOLS_SYMBOLS_H

#include "ids.h"

SymbolId make_symbol_from_ast_node(FileId file_id, AstNodeId node_id);

TypeId get_type_from_symbol(SymbolId id);

#endif // !LILY_SYMBOLS_SYMBOLS_H
