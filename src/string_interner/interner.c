#include "string_interner/interner.h"

#include "driver/types.h"
#include "hash/hash.h"
#include "string_interner/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <string.h>

extern LilyCtx driver_ctx;

#define INTERNER_LOAD_FACTOR 0.75

static void string_interner_buckets_resize(StringInterner* interner);
static void string_interner_entries_resize(StringInterner* interner);

void string_intnerner_init(void) {
    StringInterner* interner = &driver_ctx.string_interner;

    arena_init(&interner -> arena, ARENA_KB(2), ALIGN_8);
    debug_printf("String Interner: Allocated string interner's arena with 2KB\n");

    interner -> entries = arena_alloc_array(&interner -> arena, StringEntry, STRING_INTERNER_INIT_CAPACITY);
    interner -> entry_capacity = STRING_INTERNER_INIT_CAPACITY;

    interner -> buckets = arena_alloc_array(&interner -> arena, StringId, STRING_INTERNER_INIT_CAPACITY);
    interner -> bucket_capacity = STRING_INTERNER_INIT_CAPACITY;

    interner -> count = 0;

    arena_memset(interner -> buckets, U8_MAX, sizeof(StringId) * STRING_INTERNER_INIT_CAPACITY);
}

StringId string_intern_str8(str8 str) {
    StringInterner* interner = &driver_ctx.string_interner;

    u32 hash  = hash_fnv1a_str8(str);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index] != STRING_ID_NONE) {
        StringId id = interner -> buckets[index];
        StringEntry* entry = &interner -> entries[id];

        if (
            entry -> hash == hash &&
            entry -> str.length == str.length &&
            memcmp(entry -> str.pointer, str.pointer, str.length) == 0
        ) {
            debug_printf("String Interner: string_intern_str8() returned %d\n", id);
            return id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner -> count >= interner -> bucket_capacity * INTERNER_LOAD_FACTOR)) {
        string_interner_buckets_resize(interner);
        index = (index + 1) & (interner -> bucket_capacity - 1);
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        string_interner_entries_resize(interner);
    }

    StringId id = interner -> count++;

    interner -> buckets[index] = id;
    interner -> entries[id] = (StringEntry) {
        .str = {
            .pointer = str.pointer,
            .length = str.length
        },
        .hash = hash
    };

    debug_printf("String Interner: Added '%.*s' hash = 0x%x id: %d\n", str.length, str.pointer, hash, id);

    debug_assert(id == string_lookup_str8(str) && "Id in string_intern() does not match lookup");

    return id;
}

StringId string_lookup_str8(str8 str) {
    StringInterner* interner = &driver_ctx.string_interner;

    u32 hash  = hash_fnv1a_str8(str);
    u32 index = hash & (interner -> bucket_capacity - 1);

    while (interner -> buckets[index] != STRING_ID_NONE) {
        StringId id = interner -> buckets[index];
        StringEntry* entry = &interner -> entries[id];

        if (
            entry -> hash == hash &&
            entry -> str.length == str.length &&
            memcmp(entry -> str.pointer, str.pointer, str.length) == 0
        ) {
            debug_printf("String Interner: string_interner_lookup_str8() returned %d\n", id);
            return id;
        }

        index = (index + 1) & (interner -> bucket_capacity - 1);
    }

    return STRING_ID_NONE;
}

static void string_interner_buckets_resize(StringInterner* interner) {
    u32 new_capacity = interner -> bucket_capacity * 2;
    u64 size = new_capacity * sizeof(StringId);

    StringId* new_buckets = arena_alloc(&interner -> arena, size);
    arena_memset(new_buckets, U8_MAX, size);

    debug_printf("string interner new buckets resize from %ld to %ld\n", size / 2, size);

    for (u32 i = 1; i < interner -> count; i++) {
        StringEntry* entry = &interner -> entries[i];

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (new_buckets[new_index] != STRING_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index] = i;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;
}

static void string_interner_entries_resize(StringInterner* interner) {
    u64 size = sizeof(StringEntry) * interner -> entry_capacity; 

    interner -> entries = arena_realloc(&interner -> arena, interner -> entries, size, size * 2);
    interner -> entry_capacity *= 2;
}
