#include "driver/types.h"
#include "ids.h"
#include "symbols/scope/scope.h"
#include "string_interner/interner.h"
#include "symbols/scope/types.h"
#include "symbols/symbols/symbols.h"
#include "symbols/table/table.h"
#include "utils/debug.h"
#include "utils/macros.h"

extern DriverCtx driver;

static constexpr f64 load_factor = 0.75f;
static constexpr u64 buckets_size_as_bytes = sizeof(ScopeBucket) * scope_init_capacity;
static constexpr u64 entries_size_as_bytes = sizeof(SymbolId) * scope_init_capacity;

static void scope_resize(ScopeId id);

void scope_init(ScopeId id) {
    Scope* scope = &driver.symbol_table.scopes[id];
    Arena* arena = &driver.symbol_table.scope_data_arena;

    scope -> buckets = arena_alloc(arena, buckets_size_as_bytes);
    scope -> entries = arena_alloc(arena, entries_size_as_bytes);
    scope -> parent = SCOPE_ID_NONE;
    scope -> count = 0;
    scope -> capacity = scope_init_capacity;
    scope -> resize_threshold_as_u32 = scope_init_capacity * load_factor;

    arena_memset(scope -> entries, 0xff, entries_size_as_bytes);

    debug_printf("Scope (%p) allocated %lu bytes for buckets", scope, buckets_size_as_bytes);
    debug_printf("Scope (%p) allocated %lu bytes for entries", scope, entries_size_as_bytes);
}

SymbolId scope_intern(ScopeId scope_id, FileId file_id, StringId name_id, AstNodeId node_id) {
    Scope* scope = SCOPE_ID_LOOKUP_REF(scope_id);

    if (UNLIKELY(scope -> count >= scope -> capacity)) {
        scope_resize(scope_id);
    }

    StringEntry string_entry = STRING_ID_LOOKUP(name_id);

    u32 hash  = string_entry.hash;
    u32 mask  = scope -> capacity - 1;
    u32 index = hash & mask;

    while (scope -> entries[index] != SYMBOL_ID_NONE) {
        if (scope -> buckets[index].hash == hash) {
            if (scope -> buckets[index].string_id == name_id) {
                return scope -> entries[index];
            }
        }

        index = (index + 1) & mask;
    }

    SymbolId id = make_symbol_from_ast_node(file_id, node_id);

    scope -> buckets[index].hash = string_entry.hash;
    scope -> buckets[index].string_id = name_id;

    scope -> entries[index] = id;

    return id;
}

SymbolId scope_lookup(ScopeId scope_id, StringId name_id) {
    Scope* scope = SCOPE_ID_LOOKUP_REF(scope_id);

    StringEntry string_entry = STRING_ID_LOOKUP(name_id);

    u32 hash  = string_entry.hash;
    u32 mask  = scope -> capacity - 1;
    u32 index = hash & mask;

    while (scope -> entries[index] != SYMBOL_ID_NONE) {
        if (scope -> buckets[index].hash == hash) {
            if (scope -> buckets[index].string_id == name_id) {
                return scope -> entries[index];
            }
        }

        index = (index + 1) & mask;
    }

    return SYMBOL_ID_NONE;
}

static void scope_resize(ScopeId id) {
    Scope* scope = SCOPE_ID_LOOKUP_REF(id);

    u32 old_capacity = scope -> capacity;
    u32 new_capacity = old_capacity * 2;

    ScopeBucket* new_buckets = arena_alloc(&driver.symbol_table.scope_data_arena, new_capacity * sizeof(ScopeBucket));
    SymbolId* new_entries = arena_alloc(&driver.symbol_table.scope_data_arena, new_capacity * sizeof(SymbolId));

    arena_memset(new_entries, 0xff, new_capacity * sizeof(SymbolId));

    u32 mask = new_capacity - 1;

    for (u32 i = 0; i < old_capacity; i++) {
        SymbolId id = scope -> entries[i];

        if (id == SYMBOL_ID_NONE) continue;

        ScopeBucket bucket = scope -> buckets[i];

        u32 index = bucket.hash & mask;

        while (new_entries[index] != SYMBOL_ID_NONE) {
            index = (index + 1) & mask;
        }

        new_entries[index] = id;
        new_buckets[index] = bucket;
    }

    scope -> entries  = new_entries;
    scope -> buckets  = new_buckets;
    scope -> capacity = new_capacity;

    debug_printf("Scope (%p) resized.", scope);
    debug_printf(
        "reallocated buckets from %lu to %lu bytes",
        old_capacity * sizeof(ScopeBucket),
        new_capacity * sizeof(ScopeBucket)
    );
    debug_printf(
        "reallocated entries from %lu to %lu bytes",
        old_capacity * sizeof(SymbolId),
        new_capacity * sizeof(SymbolId)
    );
}
