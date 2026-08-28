#include "hash/hash.h"
#include "utils/debug.h"
#include "utils/types.h"

#include <immintrin.h>
#include <string.h>

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

    debug_printf("Hashed \"%.*s\" length=%u hash=0x%x", length, pointer, length, (u32) hash);

    return (u32) hash;
}

u32 hash_crc32_u32(u32 n) {
    u64 hash = 0xFFFFFFFF;

    hash = _mm_crc32_u32((u32) hash, n);

    return hash;
}

u32 hash_crc32_u32_with_u32_base(u32 base, u32 n) {
    return _mm_crc32_u32(base, n);
}
