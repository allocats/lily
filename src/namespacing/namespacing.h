#ifndef LILY_NAMESPACING_H
#define LILY_NAMESPACING_H

#include "namespacing/types.h"

#define NAMESPACE_ID_LOOKUP(index)     (driver_ctx.namespace_interner.entries[index])
#define NAMESPACE_ID_LOOKUP_REF(index) (&driver_ctx.namespace_interner.entries[index])

void namespace_interner_init(void);

NamespaceId namespace_intern(StringId* ns, u32 count);
NamespaceId namespace_lookup(StringId* ns, u32 count);

#endif // !LILY_NAMESPACING_H
