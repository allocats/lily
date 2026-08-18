#include "namespacing.h"

#include "hash/hash.h"
#include "ids.h"
#include "namespacing/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "driver/types.h"

#include <assert.h>
#include <string.h>

static constexpr f64 interner_load_factor = 0.75f;
static constexpr u64 interner_arena_init_size_kb = 2;
static constexpr u64 interner_init_capacity = 32;

static_assert(interner_load_factor > 0.0f);
static_assert(interner_load_factor < 1.0f);

static_assert(interner_arena_init_size_kb > 0);

static_assert(interner_init_capacity > 0);
static_assert(ARENA_KB(interner_arena_init_size_kb) > (sizeof(NamespaceId) * interner_init_capacity + sizeof(NamespaceEntry) * interner_init_capacity));

extern DriverCtx driver;

static void namespace_interner_buckets_resize(NamespaceInterner* interner);
static void namespace_interner_entries_resize(NamespaceInterner* interner);

void namespace_interner_init(void) {
    NamespaceInterner* interner = &driver.namespace_interner;

    arena_init(&interner -> arena, ARENA_KB(interner_arena_init_size_kb), ALIGN_2);
    debug_printf("Allocated namespace interner's arena with %luKB", interner_arena_init_size_kb);

    interner -> entries = arena_alloc_array(&interner -> arena, NamespaceEntry, interner_init_capacity);
    interner -> entry_capacity = interner_init_capacity;

    interner -> buckets = arena_alloc_array(&interner -> arena, NamespaceId, interner_init_capacity);
    interner -> bucket_capacity = interner_init_capacity;

    interner -> count = 0;

    arena_memset(interner -> buckets, 0xff, sizeof(NamespaceId) * interner_init_capacity);
}

NamespaceId namespace_intern(StringId* ns, u32 count) {
    assert(ns != null);
    assert(count > 0);

    NamespaceInterner* interner = &driver.namespace_interner;

    u32 hash  = hash_fnv1a_namespace(ns, count);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    if (UNLIKELY(interner -> count >= interner -> bucket_capacity * interner_load_factor)) {
        namespace_interner_buckets_resize(interner);
    }

    while (interner -> buckets[index] != NAMESPACE_ID_NONE) {
        NamespaceId id = interner -> buckets[index];
        NamespaceEntry* entry = &interner -> entries[id];

        if (
            entry -> hash  == hash  &&
            entry -> count == count &&
            memcmp(entry -> segments, ns, count * sizeof(StringId)) == 0
        ) {
            debug_printf("Namespace Interner: intern() found and returned returned %d", id);
            return id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        namespace_interner_entries_resize(interner);
    }

    NamespaceId id = interner -> count++;

    interner -> buckets[index] = id;
    interner -> entries[id] = (NamespaceEntry) {
        .count = count,
        .hash  = hash
    };

    arena_memcpy(interner -> entries[id].segments, ns, count * sizeof(StringId));

    debug_printf("Namespace Interner: Added hash = 0x%x id: %d", hash, id);

    assert(id == namespace_lookup(ns, count) && "Id in namespace_intern() does not match lookup");

    return id;
}

NamespaceId namespace_lookup(StringId* ns, u32 count) {
    assert(ns != null);
    assert(count > 0);

    NamespaceInterner* interner = &driver.namespace_interner;

    u32 hash  = hash_fnv1a_namespace(ns, count);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index] != NAMESPACE_ID_NONE) {
        NamespaceId id = interner -> buckets[index];
        NamespaceEntry* entry = &interner -> entries[id];

        if (
            entry -> hash  == hash  &&
            entry -> count == count &&
            memcmp(entry -> segments, ns, count * sizeof(StringId)) == 0
        ) {
            debug_printf("Namespace Interner: namespace_lookup() returned %d", id);
            return id;
        }

        index = (index + 1) & mask;
    }

    debug_printf("Namespace Interner: namespace_lookup() couldn't find namespace");

    return NAMESPACE_ID_NONE;
}

static void namespace_interner_buckets_resize(NamespaceInterner* interner) {
    u32 new_capacity = interner -> bucket_capacity * 2;
    u64 size = new_capacity * sizeof(NamespaceId);

    NamespaceId* new_buckets = arena_alloc(&driver.namespace_interner.arena, size);
    arena_memset(new_buckets, 0xff, size);

    debug_printf("Namespace interner -> buckets resize from %lu to %lu bytes", size / 2, size);

    for (u32 i = 0; i < interner -> count; i++) {
        NamespaceEntry* entry = &interner -> entries[i];

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (new_buckets[new_index] != NAMESPACE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index] = i;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;
}

static void namespace_interner_entries_resize(NamespaceInterner* interner) {
    u64 size = sizeof(NamespaceEntry) * interner -> entry_capacity; 

    interner -> entries = arena_realloc(&interner -> arena, interner -> entries, size, size * 2);
    interner -> entry_capacity *= 2;
}
