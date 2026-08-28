#include "driver/types.h"
#include "ids.h"
#include "resolver_stack/types.h"
#include "string_interner/interner.h"
#include "string_interner/types.h"
#include "token/types.h"
#include "types/builtins/builtins.h"
#include "types/entries/entries.h"
#include "types/entries/types.h"
#include "types/hash/hash.h"
#include "types/table/table.h"
#include "types/table/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <sys/types.h>

extern DriverCtx driver;

static constexpr u32 init_capacity = 128;

static constexpr u64 buckets_arena_init_size = sizeof(TypeBucket) * init_capacity * 2;
static constexpr u64 entries_arena_init_size = sizeof(TypeEntry) * init_capacity * 2;

static constexpr u64 buckets_array_size = sizeof(TypeBucket) * init_capacity;
static constexpr u64 entries_array_size = sizeof(TypeEntry) * init_capacity;

static constexpr f64 load_factor = 0.75f;
static constexpr u32 init_threshold = (u32)(init_capacity * load_factor);

static void entries_resize(void);
static void nominal_buckets_resize();
static void structural_buckets_resize();

void type_table_init(void) {
    TypeTable* table = &driver.type_table;

    arena_init(&table -> nominal_arena, buckets_arena_init_size, ALIGN_DEFAULT);
    debug_printf("Init TypeTable's nominal arena with %lu bytes", buckets_arena_init_size);

    arena_init(&table -> structural_arena, buckets_arena_init_size, ALIGN_DEFAULT);
    debug_printf("Init TypeTable's structural arena with %lu bytes", buckets_arena_init_size);

    arena_init(&table -> entry_arena, entries_arena_init_size, ALIGN_DEFAULT);
    debug_printf("Init TypeTable's entries arena with %lu bytes", entries_arena_init_size);

    table -> nominal_buckets = arena_alloc(&table -> nominal_arena, buckets_array_size); 
    table -> nominal_count = 0;
    table -> nominal_capacity = init_capacity;
    table -> nominal_resize_threshold_as_u32 = init_threshold; 

    table -> structural_buckets = arena_alloc(&table -> structural_arena, buckets_array_size); 
    table -> structural_count = 0;
    table -> structural_capacity = init_capacity;
    table -> structural_resize_threshold_as_u32 = init_threshold;

    table -> entries = arena_alloc(&table -> entry_arena, entries_array_size); 
    table -> entry_count = 0;
    table -> entry_capacity = init_capacity;
    table -> entry_resize_threshold_as_u32 = init_threshold;

    arena_memset(table -> nominal_buckets, 0xff, buckets_array_size);
    arena_memset(table -> structural_buckets, 0xff, buckets_array_size);

    types_register_builtins();
}

TypeId type_table_intern_nominal(StringId name_id, TypeKind kind) {
    assert(type_family_lut[kind] == TYPE_FAMILY_NOMINAL);

    TypeTable* table = &driver.type_table;

    if (UNLIKELY(table -> nominal_count >= table -> nominal_resize_threshold_as_u32)) {
        nominal_buckets_resize();
    }

    StringEntry string_entry = STRING_ID_LOOKUP(name_id);

    u32 hash  = string_entry.hash;
    u32 mask  = table -> nominal_capacity - 1;
    u32 index = hash & mask; 

    while (table -> nominal_buckets[index].id != SYMBOL_ID_NONE) {
        TypeBucket bucket = table -> nominal_buckets[index];

        if (bucket.hash == hash) {
            return bucket.id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table -> entry_count >= table -> entry_resize_threshold_as_u32)) {
        entries_resize();
    }

    TypeId id = table -> entry_count++;

    table -> nominal_buckets[index].id = id;
    table -> nominal_buckets[index].hash = hash;

    TypeEntry* entry = &table -> entries[id];

    entry -> id = id;
    entry -> hash = hash;
    entry -> kind = kind;
    entry -> state = RESOLVE_UNRESOLVED;

    entry -> size = 0;
    entry -> alignment = 0;

    return id;
}

TypeId type_table_intern_pointer(TypeId base) {
    TypeTable* table = &driver.type_table;

    if (UNLIKELY(table -> structural_count >= table -> structural_resize_threshold_as_u32)) {
        structural_buckets_resize();
    }

    u32 hash  = types_hash_pointer(base);
    u32 mask  = table -> structural_capacity - 1;
    u32 index = hash & mask; 

    while (table -> structural_buckets[index].id != SYMBOL_ID_NONE) {
        TypeBucket bucket = table -> structural_buckets[index];

        if (bucket.hash == hash) {
            return bucket.id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table -> entry_count >= table -> entry_resize_threshold_as_u32)) {
        entries_resize();
    }

    TypeId id = table -> entry_count++;

    table -> structural_buckets[index].id = id;
    table -> structural_buckets[index].hash = hash;

    TypeEntry* entry = &table -> entries[id];

    entry -> id = id;
    entry -> hash = hash;
    entry -> kind = TYPE_POINTER;
    entry -> state = RESOLVE_RESOLVED;

    entry -> size = sizeof(void*);
    entry -> alignment = _Alignof(void*);

    entry -> as.pointer_type.base = base;

    return id;
}

TypeId type_table_intern_slice(TypeId base) {
    TypeTable* table = &driver.type_table;

    if (UNLIKELY(table -> structural_count >= table -> structural_resize_threshold_as_u32)) {
        structural_buckets_resize();
    }

    u32 hash  = types_hash_slice(base);
    u32 mask  = table -> structural_capacity - 1;
    u32 index = hash & mask; 

    while (table -> structural_buckets[index].id != SYMBOL_ID_NONE) {
        TypeBucket bucket = table -> structural_buckets[index];

        if (bucket.hash == hash) {
            return bucket.id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table -> entry_count >= table -> entry_resize_threshold_as_u32)) {
        entries_resize();
    }

    TypeId id = table -> entry_count++;

    table -> structural_buckets[index].id = id;
    table -> structural_buckets[index].hash = hash;

    TypeEntry* entry = &table -> entries[id];

    entry -> id = id;
    entry -> hash = hash;
    entry -> kind = TYPE_SLICE;
    entry -> state = RESOLVE_RESOLVED;

    entry -> size = sizeof(void*);
    entry -> alignment = _Alignof(void*);

    entry -> as.slice_type.element = base;

    return id;
}

TypeId type_table_intern_function(TypeId return_type, TypeId* arguments, u32 argument_count) {
    TypeTable* table = &driver.type_table;

    if (UNLIKELY(table -> structural_count >= table -> structural_resize_threshold_as_u32)) {
        structural_buckets_resize();
    }

    u32 hash  = types_hash_function(return_type, arguments, argument_count);
    u32 mask  = table -> structural_capacity - 1;
    u32 index = hash & mask; 

    while (table -> structural_buckets[index].id != SYMBOL_ID_NONE) {
        TypeBucket bucket = table -> structural_buckets[index];

        if (bucket.hash == hash) {
            return bucket.id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table -> entry_count >= table -> entry_resize_threshold_as_u32)) {
        entries_resize();
    }

    TypeId id = table -> entry_count++;

    table -> structural_buckets[index].id = id;
    table -> structural_buckets[index].hash = hash;

    TypeEntry* entry = &table -> entries[id];

    entry -> id = id;
    entry -> hash = hash;
    entry -> kind = TYPE_FUNCTION;
    entry -> state = RESOLVE_RESOLVED;

    entry -> size = sizeof(void*);
    entry -> alignment = _Alignof(void*);

    entry -> as.function_type.return_type = return_type;
    entry -> as.function_type.arguments = arguments;
    entry -> as.function_type.argument_count = argument_count;

    return id;
}

static inline void entries_resize(void) {
    TypeTable* table = &driver.type_table;

    u64 old_size = table -> entry_capacity * sizeof(TypeEntry);
    u64 new_size = old_size * 2;

    table -> entries = arena_realloc(&table -> entry_arena, table -> entries, old_size, new_size);
    table -> entry_capacity *= 2;

    table -> entry_resize_threshold_as_u32 = (u32)(table -> entry_capacity * load_factor);
}

static void nominal_buckets_resize(void) {
    TypeTable* table = &driver.type_table;

    u64 old_size = sizeof(TypeBucket) * table -> nominal_capacity;
    u64 new_size = old_size * 2;

    u64 new_capacity = table -> nominal_capacity * 2;

    TypeBucket* buckets = arena_realloc(&table -> nominal_arena, table -> nominal_buckets, old_size, new_size);
    arena_memset(buckets, U8_MAX, new_size);

    debug_printf("Interner -> buckets resize %lu -> %lu bytes", old_size , new_size);

    for (u32 i = 0; i < table -> entry_count; i++) {
        TypeEntry* entry = &table -> entries[i];

        if (type_family_lut[entry -> kind] != TYPE_FAMILY_NOMINAL) {
            continue;
        }

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (buckets[new_index].id != FILE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        buckets[new_index].id = i;
        buckets[new_index].hash = entry -> hash;
    }

    table -> nominal_buckets = buckets;
    table -> nominal_capacity = new_capacity;
    table -> nominal_resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}

static void structural_buckets_resize() {
    TypeTable* table = &driver.type_table;

    u64 old_size = sizeof(TypeBucket) * table -> structural_capacity;
    u64 new_size = old_size * 2;

    u64 new_capacity = table -> structural_capacity * 2;

    TypeBucket* buckets = arena_realloc(&table -> structural_arena, table -> structural_buckets, old_size, new_size);
    arena_memset(buckets, U8_MAX, new_size);

    debug_printf("Interner -> buckets resize %lu -> %lu bytes", old_size , new_size);

    for (u32 i = 0; i < table -> entry_count; i++) {
        TypeEntry* entry = &table -> entries[i];

        if (type_family_lut[entry -> kind] != TYPE_FAMILY_STRUCUTRAL) {
            continue;
        }

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (buckets[new_index].id != FILE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        buckets[new_index].id = i;
        buckets[new_index].hash = entry -> hash;
    }

    table -> structural_buckets = buckets;
    table -> structural_capacity = new_capacity;
    table -> structural_resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}
