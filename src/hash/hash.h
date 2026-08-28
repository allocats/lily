#ifndef LILY_HASH_H
#define LILY_HASH_H

#include "utils/types.h"

u32 hash_crc32_str(char* pointer, u32 length);
u32 hash_crc32_u32(u32 n);
u32 hash_crc32_u32_with_u32_base(u32 base, u32 n);

#endif // !LILY_HASH_H
