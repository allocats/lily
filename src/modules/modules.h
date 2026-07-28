#ifndef LILY_MODULES_H
#define LILY_MODULES_H

#include "modules/types.h"
#include "namespacing/types.h"

void module_registry_init(void);

#define MODULE_ID_LOOKUP(index)     (driver_ctx.module_registry.entries[index])
#define MODULE_ID_LOOKUP_REF(index) (&driver_ctx.module_registry.entries[index])

ModuleId module_intern(NamespaceId id);
ModuleId module_lookup(NamespaceId id);

#endif // !LILY_MODULES_H
