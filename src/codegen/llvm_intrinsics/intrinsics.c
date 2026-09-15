#include "codegen/llvm_intrinsics/intrinsics.h"
#include "codegen/llvm_intrinsics/types.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "types/table/table.h"
#include <assert.h>
#include <string.h>

extern DriverCtx driver;

static u32 hash_crc32_function_signature(StringId fn_name, TypeId ret_type, u32 param_count, TypeId params[]);

void intrinsic_table_init(IntrinsicTable* table) {
    TypeId void_type = driver.type_table.builtins.type_void;
    TypeId void_ptr = type_table_intern_pointer(void_type);
    TypeId i8_type = driver.type_table.builtins.type_i8;
    TypeId isize_type = driver.type_table.builtins.type_isize;

    IntrinsicEntry intrinsics[] = {
        {
            .function_name = string_intern_cstr("set"),
            .intrinsic_name = string_intern_cstr("llvm.memset"),
            .parameter_count = 3,
            .parameters = { void_ptr, i8_type, isize_type },
            .return_type = void_type,
        },
        {
            .function_name = string_intern_cstr("copy"),
            .intrinsic_name = string_intern_cstr("llvm.memcpy"),
            .parameter_count = 3,
            .parameters = { void_ptr, void_ptr, isize_type },
            .return_type = void_type,
        },
        {
            .function_name = string_intern_cstr("move"),
            .intrinsic_name = string_intern_cstr("llvm.memmove"),
            .parameter_count = 3,
            .parameters = { void_ptr, void_ptr, isize_type },
            .return_type = void_type,
        },
    };

    constexpr u32 intrinsic_count = sizeof(intrinsics) / sizeof(IntrinsicEntry);

    arena_init(&table -> arena, ARENA_KB(4), ALIGN_DEFAULT);

    table -> entries = arena_calloc(&table -> arena, sizeof(IntrinsicEntry) * intrinsic_count);
    table -> entry_count = 0;

    table -> buckets = arena_alloc(&table -> arena, sizeof(IntrinsicBucket) * 32);
    table -> bucket_count = 0;
    table -> bucket_capacity = 32;

    arena_memset(table -> buckets, 0xff, sizeof(IntrinsicBucket) * 32);

    for (u32 i = 0; i < intrinsic_count; i++) {
        assert(table -> entry_count <= intrinsic_count);

        intrinsic_intern(table, intrinsics[i]);
    }
}

void intrinsic_table_destroy(IntrinsicTable* table) {
    arena_destroy(&table -> arena);
}

IntrinsicId intrinsic_intern(IntrinsicTable* table, IntrinsicEntry intrinsic) {
    u32 hash = hash_crc32_function_signature(
        intrinsic.function_name,
        intrinsic.return_type,
        intrinsic.parameter_count,
        intrinsic.parameters
    );
    u32 mask  = table -> bucket_capacity - 1; 
    u32 index = hash & mask; 

    while (table -> buckets[index].id != INTRINSIC_ID_NONE) {
        IntrinsicBucket bucket = table -> buckets[index];

        if (bucket.hash == hash) {
            IntrinsicEntry entry = table -> entries[bucket.id];

            if (memcmp(&entry, &intrinsic, sizeof(entry)) == 0) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    IntrinsicId id = table -> entry_count++;

    table -> buckets[index].id = id;
    table -> buckets[index].hash = hash;

    table -> entries[id] = intrinsic;

    return id;
}

IntrinsicId intrinsic_lookup(IntrinsicTable* table, StringId fn, TypeId ret_type, u32 param_count, TypeId params[]) {
    u32 hash  = hash_crc32_function_signature(fn, ret_type, param_count, params);
    u32 mask  = table -> bucket_capacity - 1; 
    u32 index = hash & mask; 

    while (table -> buckets[index].id != INTRINSIC_ID_NONE) {
        IntrinsicBucket bucket = table -> buckets[index];

        if (bucket.hash == hash) {
            IntrinsicEntry entry = table -> entries[bucket.id];

            if (
                entry.function_name == fn && 
                entry.return_type == ret_type && 
                entry.parameter_count == param_count &&
                memcmp(entry.parameters, params, sizeof(TypeId) * param_count) == 0
            ) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    return INTRINSIC_ID_NONE;
}

static u32 hash_crc32_function_signature(StringId fn_name, TypeId ret_type, u32 param_count, TypeId params[]) {
    u32 hash = 0;

    hash = hash_crc32_u32(fn_name);
    hash = hash_crc32_u32_with_u32_base(hash, ret_type);

    for (u32 i = 0; i < param_count; i++) {
        hash = hash_crc32_u32_with_u32_base(hash, params[i]);
    }

    return hash;
}
