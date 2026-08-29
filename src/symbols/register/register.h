#ifndef LILY_SYMBOLS_REGISTER_H
#define LILY_SYMBOLS_REGISTER_H

#include "ids.h"

void symbols_register_builtin_types(void);
ScopeId register_top_level_symbols_for_file(FileId id);

#endif // !LILY_SYMBOLS_REGISTER_H
