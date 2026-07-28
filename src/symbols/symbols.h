#ifndef LILY_SYMBOLS_H
#define LILY_SYMBOLS_H

#include "modules/types.h"
#include "symbols/types.h"

void scope_init(Scope* scope);

void symbols_register(ModuleId id);

#endif // !LILY_SYMBOLS_H
