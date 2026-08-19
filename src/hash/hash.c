#include "hash/hash.h"

#define FNV1A32_BASIS 0x811c9dc5
#define FNV1A32_PRIME 0x01000193

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
