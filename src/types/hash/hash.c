#include "hash/hash.h"
#include "types/entries/types.h"
#include "types/hash/hash.h"

#include <immintrin.h>

u32 types_hash_pointer(TypeId base) {
    u32 hash = 0xFFFFFFFF;

    hash = hash_crc32_u32_with_u32_base(hash, TYPE_POINTER);
    hash = hash_crc32_u32_with_u32_base(hash, base);

    return hash;
}

u32 types_hash_slice(TypeId base) {
    u32 hash = 0xFFFFFFFF;

    hash = hash_crc32_u32_with_u32_base(hash, TYPE_SLICE);
    hash = hash_crc32_u32_with_u32_base(hash, base);

    return hash;
}

u32 types_hash_function(TypeId ret, TypeId* args, u32 count) {
    u32 hash = 0xFFFFFFFF;

    hash = hash_crc32_u32_with_u32_base(hash, TYPE_FUNCTION);
    hash = hash_crc32_u32_with_u32_base(hash, ret);

    for (u32 i = 0; i < count; i++) {
        hash = hash_crc32_u32_with_u32_base(hash, args[i]);
    }

    return hash;
}
