#ifndef LILY_HASH_H
#define LILY_HASH_H

#include "ids.h"
#include "utils/types.h"

u32 hash_fnv1a_str8(str8 str);
u32 hash_fnv1a_u32(u32 i);
u32 hash_fnv1a_namespace(StringId* ns, u32 count);

#endif // !LILY_HASH_H
