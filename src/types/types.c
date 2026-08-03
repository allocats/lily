#include "ast/nodes/types.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "query/types.h"
#include "string_interner/interner.h"
#include "symbols/symbols.h"
#include "symbols/types.h"
#include "types/ty.h"
#include "types/builtins.h"
#include "types/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#define LOAD_FACTOR 0.75

#define FNV1A32_BASIS 0x811c9dc5
#define FNV1A32_PRIME 0x01000193

extern LilyCtx driver_ctx;

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

    TypeId* builtin_ids = (TypeId *) &table -> builtins;
    static_assert(
        sizeof(TypeBuiltinIds) == BUILTIN_NOMINAL_TYPES_COUNT * sizeof(TypeId),
        "TypeBuiltinIds is not tightly packed"
    );

    for (u32 i = 0; i < BUILTIN_NOMINAL_TYPES_COUNT; i++) {
        TypeId id = builtin_add_primitive(BUILTIN_NOMINAL_TYPES[i]);

        builtins_register_type(id);

        builtin_ids[i] = id;
    }
}

TypeId builtin_add_primitive(TypeBuiltin type) {
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

    entry -> kind  = TYPE_BASE;
    entry -> name  = name_id;
    entry -> hash  = hash;
    entry -> size  = type.size;
    entry -> align = type.align;

    entry -> resolve_state = RESOLVE_RESOLVED;
    entry -> owning_symbol = SYMBOL_ID_NONE;

    debug_printf("Types: Added builtin type %.*s at id=%d\n", type.name.length, type.name.pointer, id);

    return id;
}

TypeId builtin_lookup_primitive(StringId name_id) {
    TypeTable* table = &driver_ctx.type_table;

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
                "Types: Builtin lookup primitive returned id=%d\n",
                type_id
            );

            return type_id;
        }

        index = (index + 1) & mask;
    }

    return TYPE_ID_NONE;
}

TypeId type_table_register_nominal(u32 hash, StringId name, TypeKind kind, AstNodeId node_id, SymbolId sym_id) {
    TypeTable* table = &driver_ctx.type_table;

    if (UNLIKELY(table -> count >= table -> nominal_bucket_capacity * LOAD_FACTOR)) {
        type_table_nominal_buckets_resize(table);
    }

    u32 mask  = table -> nominal_bucket_capacity - 1;
    u32 index = hash & mask;

    while (table -> nominal_buckets[index] != TYPE_ID_NONE) {
        TypeId id = table -> nominal_buckets[index];
        TypeEntry* entry = &table -> entries[id];

        if (
            entry -> hash == hash && 
            entry -> name == name
        ) {
            debug_printf("Types: Nominal add returned %d\n", id);
            return TYPE_ID_NONE;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table -> count >= table -> entry_capacity)) {
        type_table_entries_resize(table);
    }

    TypeId id = table -> count++;

    table -> nominal_buckets[index] = id;

    TypeEntry* entry = &table -> entries[id];

    entry -> name  = name;
    entry -> hash  = hash;
    entry -> size  = 0;
    entry -> align = 0;
    entry -> node_id = node_id;
    entry -> owning_symbol = sym_id;
    entry -> resolve_state = RESOLVE_UNRESOLVED;

    return id;
}

TypeId type_table_lookup_nominal(AstNode* ident) {
    TypeTable* table = &driver_ctx.type_table;

    u32 hash  = types_hash_nominal(ident -> as.ident.namespace_id, ident -> as.ident.name_id);
    u32 mask  = table -> nominal_bucket_capacity - 1;
    u32 index = hash & mask;

    while (table -> nominal_buckets[index] != TYPE_ID_NONE) {
        TypeId id = table -> nominal_buckets[index];
        TypeEntry* entry = &table -> entries[id];

        if (
            entry -> hash == hash && 
            entry -> name == ident -> as.ident.name_id
        ) {
            debug_printf("Types: Nominal lookup returned %d\n", id);

            return id;
        }

        index = (index + 1) & mask;
    }

    return TYPE_ID_NONE;
}

TypeId type_table_register_pointer(TypeId base) {
    TypeTable* table = &driver_ctx.type_table;

    if (UNLIKELY(table -> count >= table -> structural_bucket_capacity * LOAD_FACTOR)) {
        type_table_structural_buckets_resize(table);
    }

    u32 hash  = types_hash_pointer(base);
    u32 mask  = table -> structural_bucket_capacity - 1;
    u32 index = hash & mask;

    while (table -> structural_buckets[index] != TYPE_ID_NONE) {
        TypeId id = table -> structural_buckets[index];
        TypeEntry* entry = &table -> entries[id];

        if (
            entry -> hash == hash &&
            entry -> kind == TYPE_POINTER &&
            entry -> as.pointer.base == base
        ) {
            debug_printf("Types: Found pointer: %d\n", id);
            return id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table -> count >= table -> entry_capacity)) {
        type_table_entries_resize(table);
    }

    TypeId id = table -> count++;

    table -> structural_buckets[index] = id;

    TypeEntry* entry = &table -> entries[id];

    entry -> kind  = TYPE_POINTER;
    entry -> hash  = hash;
    entry -> size  = sizeof(void*);
    entry -> size  = _Alignof(void*);

    entry -> owning_symbol = SYMBOL_ID_NONE;

    entry -> as.pointer.base = base;

    debug_printf("Types: Added pointer: %d\n", id);

    return id;
}

TypeId type_table_lookup_pointer(TypeId base) {
    TypeTable* table = &driver_ctx.type_table;

    u32 hash  = types_hash_pointer(base);
    u32 mask  = table -> structural_bucket_capacity - 1;
    u32 index = hash & mask;

    while (table -> structural_buckets[index] != TYPE_ID_NONE) {
        TypeId id = table -> structural_buckets[index];
        TypeEntry* entry = &table -> entries[id];

        if (
            entry -> hash == hash &&
            entry -> kind == TYPE_POINTER &&
            entry -> as.pointer.base == base
        ) {
            return id;
        }

        index = (index + 1) & mask;
    }

    return TYPE_ID_NONE;
}


TypeId type_table_register_array(TypeId element, AstNodeId length) {

}

TypeId type_table_lookup_array(TypeId base) {

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

u32 types_hash_id(u32 hash, u32 id) {
    hash ^= id;
    hash *= FNV1A32_PRIME;
    return hash;
}

u32 types_hash_nominal(NamespaceId ns, StringId name) {
    u32 hash = FNV1A32_BASIS;

    hash = types_hash_id(hash, ns);
    hash = types_hash_id(hash, name);

    return hash;
}

u32 types_hash_pointer(TypeId base) {
    u32 hash = FNV1A32_BASIS;

    hash = types_hash_id(hash, TYPE_POINTER);
    hash = types_hash_id(hash, base);

    return hash;
}
