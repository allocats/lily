#ifndef LILY_SYMBOLS_REGISTER_TYPES_H
#define LILY_SYMBOLS_REGISTER_TYPES_H

#include "files/types.h"
#include "ids.h"

typedef struct {
    File* file;
    ScopeId scope_id;

    // Used in resolving for functions
    SymbolId current_symbol;
    bool in_loop_ctx;
} Registrar;

#endif // !LILY_SYMBOLS_REGISTER_TYPES_H
