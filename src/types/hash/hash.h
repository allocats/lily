#ifndef LILY_TYPES_HASH_H
#define LILY_TYPES_HASH_H

#include "ids.h"

u32 types_hash_pointer(TypeId base);
u32 types_hash_slice(TypeId base);
u32 types_hash_function(TypeId ret, TypeId* args, u32 count);

#endif // !LILY_TYPES_HASH_H
