#include "string_interner/interner.h"

#include "driver/types.h"
#include "hash/hash.h"
#include "string_interner/types.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <string.h>

extern DriverCtx driver;

static constexpr f64 interner_load_factor = 0.75f;

static_assert(interner_load_factor > 0.0f);
static_assert(interner_load_factor < 1.0f);

static constexpr u64 interner_arena_init_size_kb = 4;

static_assert(interner_arena_init_size_kb > 0);

static constexpr u64 interner_init_capacity = 128;

static_assert(interner_init_capacity > 0);
static_assert(ARENA_KB(interner_arena_init_size_kb) > (sizeof(StringId) * interner_init_capacity + sizeof(StringEntry) * interner_init_capacity));

static void string_interner_buckets_resize(StringInterner* interner);
static void string_interner_entries_resize(StringInterner* interner);

void string_interner_init(void) {
    StringInterner* interner = &driver.string_interner;

    arena_init(&interner -> arena, ARENA_KB(interner_arena_init_size_kb), ALIGN_8);
    debug_printf("String Interner: Allocated string interner's arena with %luKB", interner_arena_init_size_kb);

    interner -> entries = arena_alloc_array(&interner -> arena, StringEntry, interner_init_capacity);
    interner -> entry_capacity = interner_init_capacity;

    interner -> buckets = arena_alloc_array(&interner -> arena, StringId, interner_init_capacity);
    interner -> bucket_capacity = interner_init_capacity;

    interner -> count = 0;

    arena_memset(interner -> buckets, U8_MAX, sizeof(StringId) * interner_init_capacity);

    debug_printf("allocted interner -> entries with %lu bytes", sizeof(StringEntry) * interner_init_capacity);
    debug_printf("allocted interner -> buckets with %lu bytes", sizeof(StringId)    * interner_init_capacity);
}

StringId string_intern_str8(str8 str) {
    assert(str.ptr != null);
    assert(str.len > 0);

    StringInterner* interner = &driver.string_interner;

    if (UNLIKELY(interner -> count >= interner -> bucket_capacity * interner_load_factor)) {
        string_interner_buckets_resize(interner);
    }

    u32 hash  = hash_fnv1a_str8(str);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index] != STRING_ID_NONE) {
        StringId id = interner -> buckets[index];
        StringEntry* entry = &interner -> entries[id];

        if (
            entry -> hash == hash &&
            entry -> str.len == str.len &&
            memcmp(entry -> str.ptr, str.ptr, str.len) == 0
        ) {
            debug_printf("string_intern_str8() found and returned %d", id);
            return id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        string_interner_entries_resize(interner);
    }

    StringId id = interner -> count++;

    interner -> buckets[index] = id;
    interner -> entries[id] = (StringEntry) {
        .str = {
            .ptr = str.ptr,
            .len = str.len
        },
        .hash = hash
    };

    debug_printf("Added string '%.*s' hash = 0x%x id: %d", str.len, str.ptr, hash, id);

    // TODO: Profile this assert, potentially turn it into debug_assert()
    assert(id == string_lookup_str8(str) && "id in string_intern() does not match lookup");

    return id;
}

StringId string_lookup_str8(str8 str) {
    assert(str.ptr != null);
    assert(str.len > 0);

    StringInterner* interner = &driver.string_interner;

    u32 hash  = hash_fnv1a_str8(str);
    u32 index = hash & (interner -> bucket_capacity - 1);

    while (interner -> buckets[index] != STRING_ID_NONE) {
        StringId id = interner -> buckets[index];
        StringEntry* entry = &interner -> entries[id];

        if (
            entry -> hash == hash &&
            entry -> str.len == str.len &&
            memcmp(entry -> str.ptr, str.ptr, str.len) == 0
        ) {
            debug_printf("string_interner_lookup_str8() returned %d", id);
            return id;
        }

        index = (index + 1) & (interner -> bucket_capacity - 1);
    }

    debug_printf("string_interner_lookup_str8() could not find '%.*s'", str.len, str.ptr);
    return STRING_ID_NONE;
}

static void string_interner_buckets_resize(StringInterner* interner) {
    u32 new_capacity = interner -> bucket_capacity * 2;
    u64 new_size = new_capacity * sizeof(StringId);

    StringId* new_buckets = arena_alloc(&interner -> arena, new_size);
    arena_memset(new_buckets, U8_MAX, new_size);

    debug_printf("interner -> buckets new allocation from %lu to %lu", new_size / 2, new_size);

    for (u32 i = 0; i < interner -> count; i++) {
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
    u64 old_size = sizeof(StringEntry) * interner -> entry_capacity; 
    u64 new_size = old_size * 2;

    interner -> entries = arena_realloc(&interner -> arena, interner -> entries, old_size, new_size);
    interner -> entry_capacity *= 2;

    debug_printf("interner -> entries realloc from %lu to %lu bytes", old_size, new_size);
}
