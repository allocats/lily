#include "ast/nodes/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "resolver_stack/types.h"
#include "symbols/scope/scope.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "types/builtins/types.h"
#include "symbols/register/register.h"

extern DriverCtx driver;

static inline SymbolId register_builtin_type(TypeId id, TypeBuiltin* type) {
    SymbolId symbol_id = scope_intern(COMPILER_SCOPE_ID, type -> name_id, SYMBOL_TYPE);
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    symbol -> state = RESOLVE_RESOLVED;
    symbol -> as.type_symbol.type_id = id;

    return symbol_id;
}

inline void symbols_register_builtin_types(void) {
    for (u32 i = 0; i < BUILTIN_NOMINAL_TYPES_COUNT; i++) {
        TypeBuiltin* type = &BUILTIN_NOMINAL_TYPES[i];
        register_builtin_type(type -> id, type);
    }
}

void register_top_level_symbols_for_file(FileId id) {
    File* file = file_lookup_id(id); 
    Ast* ast = &file -> ast;

    for (u32 i = 0; i < ast -> count; i++) {
        AstNode* node = &ast -> nodes[i];

        if (!(node -> flags & AST_FLAGS_IS_TOP_DECL)) {
            continue; 
        }

        // register_symbol_from_node();
    }
}
