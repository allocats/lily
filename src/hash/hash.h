#ifndef LILY_HASH_H
#define LILY_HASH_H

#include "utils/types.h"

u32 hash_fnv1a_str8(str8 str);
u32 hash_fnv1a_u32(u32 i);

#endif // !LILY_HASH_H
