#include "driver/types.h"
#include "files/types.h"
#include "hash/hash.h"
#include "modules/modules.h"
#include "modules/types.h"
#include "symbols/symbols.h"
#include "utils/debug.h"
#include "utils/macros.h"

extern LilyCtx driver_ctx;

#define INTERNER_INIT_CAPACITY 32
#define INTERNER_LOAD_FACTOR   0.75

static void module_buckets_resize(ModuleRegistry* registry);
static void module_entries_resize(ModuleRegistry* registry);

void module_registry_init(void) {
    ModuleRegistry* registry = &driver_ctx.module_registry;

    arena_init(&registry -> arena, ARENA_KB(2), ALIGN_8);
    debug_printf("Module Registry: Allocated arena with 2KB\n");

    registry -> buckets = arena_alloc_array(&registry -> arena, ModuleId, INTERNER_INIT_CAPACITY);
    registry -> bucket_capacity = INTERNER_INIT_CAPACITY;

    registry -> entries = arena_alloc_array(&registry -> arena, Module, INTERNER_INIT_CAPACITY);
    registry -> entry_capacity = INTERNER_INIT_CAPACITY;

    registry -> count = 0;

    arena_memset(registry -> buckets, 0xff, INTERNER_INIT_CAPACITY * sizeof(ModuleId));
}

ModuleId module_intern(NamespaceId id) {
    ModuleRegistry* registry = &driver_ctx.module_registry;

    u32 hash  = hash_fnv1a_u32(id);
    u32 mask  = registry -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (registry -> buckets[index] != MODULE_ID_NONE) {
        ModuleId entry_id = registry -> buckets[index];
        Module* entry = &registry -> entries[entry_id];

        if (
            entry -> namespace_id == id
        ) {
            debug_printf("Module Registry: module_intern() returned %d, already registered\n", id);
            return entry_id;
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(registry -> count >= registry -> entry_capacity)) {
        module_entries_resize(registry);
    }

    if (UNLIKELY(registry -> count >= registry -> bucket_capacity * INTERNER_LOAD_FACTOR)) {
        module_buckets_resize(registry);
        index = (index + 1) & (registry -> bucket_capacity - 1);
    }

    ModuleId module_id = registry -> count++;
    Module*  module = &registry -> entries[module_id];

    registry -> buckets[index] = module_id;

    module -> namespace_id = id;
    module -> hash = hash;

    arena_init(&module -> ast.arena, ARENA_KB(4), ALIGN_8);
    debug_printf("Module Registry: Init module %d's AST arena with 4KB\n", module_id);

    module -> ast.nodes = arena_alloc_array(&module -> ast.arena, AstNode, 64); 
    module -> ast.count = 0;
    module -> ast.capacity = 64;

    arena_init(&module -> symbol_table.arena, ARENA_KB(2), ALIGN_8);
    debug_printf("Module Registry: Init module %d's symbol table arena with 2KB\n", module_id);

    module -> symbol_table.symbols = arena_alloc_array(&module -> symbol_table.arena, Symbol, 32);
    module -> symbol_table.symbol_count = 0;
    module -> symbol_table.symbol_capacity = 32;

    module -> symbol_table.scopes = arena_alloc_array(&module -> symbol_table.arena, Scope, 4);
    module -> symbol_table.scope_count = 0;
    module -> symbol_table.scope_capacity = 4;

    for (u32 i = 0; i < module -> symbol_table.scope_capacity; i++) {
        scope_init(&module -> symbol_table.scopes[i]);
    }

    
    return module_id;
}

ModuleId module_lookup(NamespaceId id) {
    ModuleRegistry* registry = &driver_ctx.module_registry;

    u32 hash  = hash_fnv1a_u32(id);
    u32 mask  = registry -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (registry -> buckets[index] != MODULE_ID_NONE) {
        ModuleId entry_id = registry -> buckets[index];
        Module* entry = &registry -> entries[entry_id];

        if (
            entry -> namespace_id == id
        ) {
            debug_printf("Module Registry: module_intern() returned %d\n", id);
            return entry_id;
        }

        index = (index + 1) & mask;
    }

    return MODULE_ID_NONE;
}

static void module_buckets_resize(ModuleRegistry* registry) {
    u32 new_capacity = registry -> bucket_capacity * 2;
    u64 size = new_capacity * sizeof(ModuleId);

    ModuleId* new_buckets = arena_alloc(&registry -> arena, size);
    arena_memset(new_buckets, U8_MAX, size);

    debug_printf("string registry new buckets resize from %ld to %ld\n", size / 2, size);

    for (u32 i = 1; i < registry -> count; i++) {
        Module* entry = &registry -> entries[i];

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (new_buckets[new_index] != STRING_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index] = i;
    }

    registry -> buckets = new_buckets;
    registry -> bucket_capacity = new_capacity;
}

static void module_entries_resize(ModuleRegistry* registry) {
    u64 size = sizeof(Module) * registry -> entry_capacity; 

    registry -> entries = arena_realloc(&registry -> arena, registry -> entries, size, size * 2);
    registry -> entry_capacity *= 2;
}
