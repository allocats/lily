#include "ast/tree/tree.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/token.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern DriverCtx driver;

static constexpr f64 interner_load_factor = 0.75;
static constexpr u64 interner_init_buffers_arena_kb  = 512;
static constexpr u64 interner_init_interner_arena_kb = 4;

static_assert(interner_load_factor > 0.0f);
static_assert(interner_load_factor < 1.0f);
static_assert(interner_init_buffers_arena_kb > 0);
static_assert(interner_init_interner_arena_kb> 0);

static void files_buckets_resize(FileInterner* interner);
static void files_entries_resize(FileInterner* interner);

static str8 allocate_buffer(str8 path);

void file_interner_init(u32 count) {
    assert(count > 0);
    assert((count & (count - 1)) == 0); // assert that count is a power of two

    count *= 2;

    FileInterner* interner = &driver.file_interner;

    arena_init(&interner -> buffer_arena, ARENA_KB(interner_init_buffers_arena_kb), ALIGN_DEFAULT);
    debug_printf("Init file interner buffers arena with %luKB", interner_init_buffers_arena_kb);

    arena_init(&interner -> interner_arena, ARENA_KB(interner_init_interner_arena_kb), ALIGN_DEFAULT);
    debug_printf("Init file interner arena with %luKB", interner_init_interner_arena_kb);

    interner -> entries = arena_alloc_array(&interner -> interner_arena, File, count);
    interner -> entry_capacity = count;

    interner -> buckets = arena_alloc_array(&interner -> interner_arena, FileBucket, count);
    interner -> bucket_capacity = count;

    interner -> count = 0;

    interner -> resize_threshold_as_u32 = (u32)(count * interner_load_factor);

    arena_memset(interner -> entries, 0, sizeof(File) * count);
    arena_memset(interner -> buckets, U8_MAX, sizeof(FileBucket) * count);

    debug_printf("Allocated FileInterner -> entries with %lu bytes, capacity = %u", count * sizeof(File), count);
    debug_printf("Allocated FileInterner -> buckets with %lu bytes, capacity = %u", count * sizeof(FileId), count);

    path_normalizer_init();
}

FileId file_intern(str8 input_path) {
    assert(input_path.ptr != null);
    assert(input_path.len > 0);

    str8 path = get_absolute_path(input_path);

    FileInterner* interner = &driver.file_interner;
    
    if (UNLIKELY(interner -> count >= interner -> resize_threshold_as_u32)) {
        files_buckets_resize(interner);
    }

    u32 hash  = hash_crc32_str(path.ptr, path.len);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask; 

    while (interner -> buckets[index].id != FILE_ID_NONE) {
        FileBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            File* file = &interner -> entries[bucket.id]; 

            if (file -> path.len == path.len && memcmp(file -> path.ptr, path.ptr, path.len) == 0) {
                debug_printf("intern() found %d for 0x%x", bucket.id, hash);
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    str8 buffer = allocate_buffer(path);

    if (buffer.len == 0 && buffer.ptr == null) {
        debug_printf("intern() returned early due to failure");
        return FILE_ID_NONE;
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        files_entries_resize(interner);
    }

    FileId id  = interner -> count++;
    File* file = &interner -> entries[id];

    interner -> buckets[index].id = id;
    interner -> buckets[index].hash = hash;

    file -> id     = id;
    file -> path   = path;
    file -> hash   = hash; 
    file -> buffer = buffer;
    file -> stage  = FILE_ALLOCATED;

    file -> path_string_id = string_intern_str8(path);

    tokens_array_init(&file -> tokens);

    ast_init(&file -> ast);

    debug_printf("intern() returned %d for 0x%x", id, hash);

    debug_assert(id == file_lookup(input_path));

    return id;
}

FileId file_lookup(str8 input_path) {
    assert(input_path.ptr != null);
    assert(input_path.len > 0);

    str8 path = get_absolute_path(input_path);

    FileInterner* interner = &driver.file_interner;

    u32 hash  = hash_crc32_str(path.ptr, path.len);
    u32 index = hash & (interner -> bucket_capacity - 1); 

    while (interner -> buckets[index].id != FILE_ID_NONE) {
        FileBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            File* file = &interner -> entries[bucket.id]; 

            if (file -> path.len == path.len && memcmp(file -> path.ptr, path.ptr, path.len) == 0) {
                debug_printf("lookup() found file 0x%x at id=%u", hash, bucket.id);
                return bucket.id;
            }
        }

        index = (index + 1) & (interner -> bucket_capacity - 1);
    }
    
    debug_printf("lookup() didn't find file '%.*s'", STR8_FMT(path));

    return FILE_ID_NONE;
}

inline File* file_lookup_id(FileId id) {
    assert(id != FILE_ID_NONE);
    assert(id <  driver.file_interner.count);
    return &driver.file_interner.entries[id];
}

static void files_buckets_resize(FileInterner* interner) {
    u64 old_size = interner -> bucket_capacity * sizeof(FileBucket);
    u64 new_size = old_size * 2;

    u32 new_capacity = interner -> bucket_capacity * 2;

    FileBucket* new_buckets = arena_alloc(&interner -> interner_arena, new_size);
    arena_memset(new_buckets, U8_MAX, new_size);

    debug_printf("Interner -> buckets resize %lu -> %lu bytes", old_size , new_size);

    for (u32 i = 0; i < interner -> count; i++) {
        File* entry = &interner -> entries[i];

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (new_buckets[new_index].id != FILE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index].id = i;
        new_buckets[new_index].hash = entry -> hash;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;
    interner -> resize_threshold_as_u32 = (u32)(new_capacity * interner_load_factor);
}

static void files_entries_resize(FileInterner* interner) {
    u64 size = interner -> entry_capacity * sizeof(File); 

    interner -> entries = arena_realloc(&interner -> interner_arena, interner -> entries, size, size * 2);
    interner -> entry_capacity *= 2;

    debug_printf("Interner -> entries resize %lu -> %lu bytes", size, size * 2);
}

static str8 allocate_buffer(str8 path) {
    str8 buffer = {0};

    debug_printf("Allocating file: %s", path.ptr);

    i32 fd = open(path.ptr, O_RDONLY);
    if (fd < 0) {
        // errno, error diagnostics
        diagnostic_add_generic(
            DIAG_ERROR,
            "unable to open file: %.*s",
            path.len,
            path.ptr
        );

        goto exit;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        diagnostic_add_generic(
            DIAG_ERROR,
            "unable to stat file: %s",
            path.ptr
        );

        goto exit_fd_open;
    }

    u64 buffer_size = st.st_size + 1;

    FileInterner* interner = &driver.file_interner;

    buffer.ptr = arena_alloc(&interner -> buffer_arena, buffer_size);
    buffer.len = buffer_size;

    buffer.ptr[buffer_size - 1] = 0;

    u64 bytes_read = 0;

    while (bytes_read < buffer_size) {
        i64 n = (i64) read(fd, buffer.ptr + bytes_read, buffer_size - bytes_read);

        if (n == 0) break;

        // TODO: add errors
        assert(n >= 0 && "Read failed");
        bytes_read += n;
    }

    debug_printf(
        "Allocated file '%.*s' (%lu bytes) Hash = 0x%x",
        (i32) path.len,
        path.ptr,
        buffer_size,
        hash_crc32_str(path.ptr, path.len)
    );

exit_fd_open:
    close(fd);

exit:
    return buffer;
}
