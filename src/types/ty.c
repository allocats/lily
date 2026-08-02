#include "types/ty.h"
#include "driver/types.h"
#include "files/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "symbols/symbols.h"
#include "types/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#define LOAD_FACTOR 0.75

extern LilyCtx driver_ctx;

typedef struct {
    str8 name;
    u32 size;
    u32 align;
} TypeBuiltin;

static const TypeBuiltin BUILTIN_NOMINAL_TYPES[] = {
    { .name = { .pointer = "u8" , .length = sizeof("u8" ) - 1 }, .size = 1, .align = _Alignof(u8)  },
    { .name = { .pointer = "u16", .length = sizeof("u16") - 1 }, .size = 2, .align = _Alignof(u16) },
    { .name = { .pointer = "u32", .length = sizeof("u32") - 1 }, .size = 4, .align = _Alignof(u32) },
    { .name = { .pointer = "u64", .length = sizeof("u64") - 1 }, .size = 8, .align = _Alignof(u64) },

    { .name = { .pointer = "i8" , .length = sizeof("i8" ) - 1 }, .size = 1, .align = _Alignof(i8)  },
    { .name = { .pointer = "i16", .length = sizeof("i16") - 1 }, .size = 2, .align = _Alignof(i16) },
    { .name = { .pointer = "i32", .length = sizeof("i32") - 1 }, .size = 4, .align = _Alignof(i32) },
    { .name = { .pointer = "i64", .length = sizeof("i64") - 1 }, .size = 8, .align = _Alignof(i64) },

    { .name = { .pointer = "f32", .length = sizeof("f32") - 1 }, .size = 4, .align = _Alignof(f32) },
    { .name = { .pointer = "f64", .length = sizeof("f64") - 1 }, .size = 8, .align = _Alignof(f64) },

    { .name = { .pointer = "bool", .length = sizeof("bool") - 1 }, .size = 1, .align = _Alignof(bool) },

    { .name = { .pointer = "usize", .length = sizeof("usize") - 1 }, .size = sizeof(void*), .align = _Alignof(void*)},
    { .name = { .pointer = "isize", .length = sizeof("isize") - 1 }, .size = sizeof(void*), .align = _Alignof(void*)},
};

static const u32 BUILTIN_NOMINAL_TYPES_COUNT = sizeof(BUILTIN_NOMINAL_TYPES) / sizeof(TypeBuiltin);

static TypeId builtin_add_primitive(TypeBuiltin type);

static void type_table_structural_buckets_resize(TypeTable* table);
static void type_table_nominal_buckets_resize(TypeTable* table);
static void type_table_entries_resize(TypeTable* table);

void type_table_init(void) {
    TypeTable* table = &driver_ctx.type_table;

    arena_init(&table -> arena, ARENA_KB(2), ALIGN_8);
    debug_printf("Types: Init arena with 2KB\n");

    table -> structural_buckets = arena_alloc_array(&table -> arena, TypeId, 32);
    table -> structural_bucket_capacity = 32; 

    table -> nominal_buckets = arena_alloc_array(&table -> arena, TypeId, 32);
    table -> nominal_bucket_capacity = 32; 

    table -> entries = arena_alloc_array(&table -> arena, TypeEntry, 64);
    table -> entry_capacity = 64;

    table -> count = 0;

    arena_memset(table -> structural_buckets, 0xff, 32 * sizeof(TypeId));
    arena_memset(table -> nominal_buckets, 0xff, 32 * sizeof(TypeId));

    for (u32 i = 0; i < BUILTIN_NOMINAL_TYPES_COUNT; i++) {
        TypeId id = builtin_add_primitive(BUILTIN_NOMINAL_TYPES[i]);

        builtins_register_type(id);
    }
}

static TypeId builtin_add_primitive(TypeBuiltin type) {
    debug_printf("Type: Registering builtin primitive %.*s\n", type.name.length, type.name.pointer);

    TypeTable* table = &driver_ctx.type_table;

    if (UNLIKELY(table -> count >= table -> nominal_bucket_capacity * LOAD_FACTOR)) {
        type_table_nominal_buckets_resize(table);
    }

    StringId name_id = string_intern_str8(type.name);

    u32 hash  = hash_fnv1a_u32(name_id);
    u32 mask  = table -> nominal_bucket_capacity - 1;
    u32 index = hash & mask;

    while (table -> nominal_buckets[index] != TYPE_ID_NONE) {
        TypeId type_id = table -> nominal_buckets[index];
        TypeEntry* type_entry = &table -> entries[type_id];

        if (
            type_entry -> hash == hash &&
            type_entry -> name == name_id
        ) {
            debug_printf(
                "Types: Builtin add primitive %.*s already exists at id=%d\n",
                type.name.length,
                type.name.pointer,
                type_id
            );

            return type_id;
        }

        index = (index + 1) & mask;
    }

    if (table -> count >= table -> entry_capacity) {
        type_table_entries_resize(table);
    }

    TypeId id = table -> count++;

    table -> nominal_buckets[index] = id;

    TypeEntry* entry = &table -> entries[id];

    entry -> name  = name_id;
    entry -> hash  = hash;
    entry -> size  = type.size;
    entry -> align = type.align;

    debug_printf("Types: Added builtin type %.*s at id=%d\n", type.name.length, type.name.pointer, id);

    return id;
}

static void type_table_structural_buckets_resize(TypeTable* table) {
    u32 new_capacity = table -> structural_bucket_capacity * 2;
    u64 size = new_capacity * sizeof(TypeId);

    TypeId* new_structural_buckets = arena_alloc(&table -> arena, size);
    arena_memset(new_structural_buckets, U8_MAX, size);

    debug_printf("Types: Table new structural_buckets resize from %ld -> %ld\n", size / 2, size);

    for (u32 i = 0; i < table -> count; i++) {
        TypeEntry* entry = &table -> entries[i];

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (new_structural_buckets[new_index] != TYPE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_structural_buckets[new_index] = i;
    }

    table -> structural_buckets = new_structural_buckets;
    table -> structural_bucket_capacity = new_capacity;
}

static void type_table_nominal_buckets_resize(TypeTable* table) {
    u32 new_capacity = table -> nominal_bucket_capacity * 2;
    u64 size = new_capacity * sizeof(TypeId);

    TypeId* new_nominal_buckets = arena_alloc(&table -> arena, size);
    arena_memset(new_nominal_buckets, U8_MAX, size);

    debug_printf("Types: Table new nominal_buckets resize from %ld -> %ld\n", size / 2, size);

    for (u32 i = 0; i < table -> count; i++) {
        TypeEntry* entry = &table -> entries[i];

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (new_nominal_buckets[new_index] != TYPE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_nominal_buckets[new_index] = i;
    }

    table -> nominal_buckets = new_nominal_buckets;
    table -> nominal_bucket_capacity = new_capacity;
}

static void type_table_entries_resize(TypeTable* table) {
    u64 size = sizeof(TypeEntry) * table -> entry_capacity; 

    table -> entries = arena_realloc(&table -> arena, table -> entries, size, size * 2);
    table -> entry_capacity *= 2;

    debug_printf("Types: Table entries realloc from %ld -> %ld bytes\n", size, size * 2);
}
