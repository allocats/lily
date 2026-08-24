#include "hash/hash.h"
#include "utils/debug.h"
#include "utils/types.h"

#include <immintrin.h>
#include <string.h>

#define FNV1A32_BASIS 0x811c9dc5
#define FNV1A32_PRIME 0x01000193

u32 hash_crc32_str(char* pointer, u32 length) {
    u64 hash = 0xFFFFFFFF;

    char* ptr = pointer;
    u32 len = length;

    while (len >= 8) {
        u64 chunk;
        memcpy(&chunk, ptr, 8);
        hash = _mm_crc32_u64(hash, chunk);

        ptr += 8;
        len -= 8;
    } 

    while (len >= 4) {
        u64 chunk;
        memcpy(&chunk, ptr, 4);
        hash = _mm_crc32_u32((u32)hash, chunk);

        ptr += 4;
        len -= 4;
    } 

    while (len--) {
        hash = _mm_crc32_u8((u32) hash, *ptr++);
    }

    debug_printf("Hashed \"%.*s\" length=%u hash=0x%x", length, pointer, length, hash);

    return (u32) hash;
}

u32 hash_fnv1a_str8(str8 str) {
    u32 hash = FNV1A32_BASIS;

    for (u32 i = 0; i < str.len; i++) {
        hash ^= str.ptr[i];
        hash *= FNV1A32_PRIME;
    }

    return hash;
}

u32 hash_fnv1a_cstr(const char* str, u32 len) {
    u32 hash = FNV1A32_BASIS;

    for (u32 i = 0; i < len; i++) {
        hash ^= str[i];
        hash *= FNV1A32_PRIME;
    }

    return hash;
}

u32 hash_fnv1a_u32(u32 i) {
    u32 hash = FNV1A32_BASIS;

    hash ^= i;
    hash *= FNV1A32_PRIME;

    return hash;
}
