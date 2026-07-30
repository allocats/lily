#include "namespacing.h"

#include "hash/hash.h"
#include "namespacing/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "driver/types.h"

#include <string.h>

#define INTERNER_LOAD_FACTOR 0.75

extern LilyCtx driver_ctx;

static void namespace_interner_buckets_resize(NamespaceInterner* interner);
static void namespace_interner_entries_resize(NamespaceInterner* interner);

void namespace_interner_init(void) {
    NamespaceInterner* interner = &driver_ctx.namespace_interner;

    arena_init(&interner -> arena, ARENA_KB(2), ALIGN_2);
    debug_printf("Allocated namespace interner's arena with 2KB\n");

    interner -> entries = arena_alloc_array(&interner -> arena, NamespaceEntry,NAMESPACE_INTERNER_INIT_CAPACITY);
    interner -> entry_capacity = NAMESPACE_INTERNER_INIT_CAPACITY;

    interner -> buckets = arena_alloc_array(&interner -> arena, NamespaceId, NAMESPACE_INTERNER_INIT_CAPACITY);
    interner -> bucket_capacity = NAMESPACE_INTERNER_INIT_CAPACITY;

    interner -> count = 0;

    arena_memset(interner -> buckets, 0xff, sizeof(NamespaceId) * NAMESPACE_INTERNER_INIT_CAPACITY);
}

NamespaceId namespace_intern(StringId ns[NAMESPACE_MAX_DEPTH], u32 count) {
    NamespaceInterner* interner = &driver_ctx.namespace_interner;

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
            debug_printf("Namespace Interner: namespace_intern() returned %d\n", id);
            return id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner -> count >= interner -> bucket_capacity * INTERNER_LOAD_FACTOR)) {
        namespace_interner_buckets_resize(interner);
        index = (index + 1) & interner -> bucket_capacity;
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

    // debug_hex_dump("Array -> segments", ns, sizeof(StringId) * count);
    // debug_hex_dump("Entry -> segments", interner -> entries[id].segments, sizeof(StringId) * count);

    debug_printf("Namespace Interner: Added hash = 0x%x id: %d\n", hash, id);

    debug_assert(id == namespace_lookup(ns, count) && "Id in namespace_intern() does not match lookup");

    return id;
}

NamespaceId namespace_lookup(StringId ns[NAMESPACE_MAX_DEPTH], u32 count) {
    NamespaceInterner* interner = &driver_ctx.namespace_interner;

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
            debug_printf("Namespace Interner: namespace_lookup() returned %d\n", id);
            return id;
        }

        index = (index + 1) & mask;
    }

    return NAMESPACE_ID_NONE;
}

static void namespace_interner_buckets_resize(NamespaceInterner* interner) {
    u32 new_capacity = interner -> bucket_capacity * 2;
    u64 size = new_capacity * sizeof(NamespaceId);

    NamespaceId* new_buckets = arena_alloc(&driver_ctx.namespace_interner.arena, size);
    arena_memset(new_buckets, 0xff, size);

    debug_printf("Namespace interner new buckets resize from %ld to %ld\n", size / 2, size);

    for (u32 i = 1; i < interner -> count; i++) {
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
