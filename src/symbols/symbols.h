#ifndef LILY_SYMBOLS_H
#define LILY_SYMBOLS_H

#include "driver/types.h"
#include "ids.h"
#include "symbols/types.h"

extern LilyCtx driver_ctx;

void symbol_table_builtins_init(void);
SymbolId builtins_get_sym(Resolver* r, StringId name, u32 hash);
SymbolId builtins_register_type(TypeId id);

void scope_init(Scope* scope);

void symbols_register_top_level_declarations(ModuleId id);
void symbols_resolve(ModuleId id);

bool symbols_resolve_by_id(ModuleId module_id, SymbolId id);

SymbolId scope_add_sym(Resolver* r, AstNodeId node_id, StringId name, SymbolKind kind);
SymbolId scope_get_sym(Resolver* r, StringId name, u32 hash);
SymbolId scope_get_sym_scope_id(Resolver* r, ScopeId scope_id, StringId name, u32 hash);

SymbolId table_get_sym(Resolver* r, StringId name);

ScopeId scope_enter(Resolver* r);
ScopeId scope_exit(Resolver* r);

#endif // !LILY_SYMBOLS_H
