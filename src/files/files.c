#include "files/files.h"

#include "driver/types.h"
#include "diagnostics/diagnostics.h"
#include "hash/hash.h"
#include "token/token.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "types.h"

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define INTERNER_LOAD_FACTOR 0.75

extern LilyCtx driver_ctx;

static void files_buckets_resize(FileRegistry* interner);
static void files_entries_resize(FileRegistry* interner);
static void files_tokens_resize(FileRegistry* interner);

static str8 allocate_buffer(str8 path);

void file_registry_init(u32 count) {
    FileRegistry* file_registry = &driver_ctx.file_registry;

    arena_init(&file_registry -> buffers_arena, ARENA_MB(1), ALIGN_DEFAULT);
    debug_printf("Driver: Allocated file registry's buffer arena with 1MB\n");

    arena_init(&file_registry -> interner_arena, ARENA_KB(4), ALIGN_8);
    debug_printf("Driver: Allocated file registry's interner arena with 4KB\n");

    arena_init(&file_registry -> tokens_arena, ARENA_KB(1), ALIGN_8);
    debug_printf("Driver: Allocated file registry's tokens arena with 1KB\n");

    file_registry -> buckets = arena_alloc_array(&file_registry -> interner_arena, FileId, count);
    file_registry -> bucket_capacity = count;

    file_registry -> entries = arena_alloc_array(&file_registry -> interner_arena, File, count);
    file_registry -> entry_capacity = count;

    file_registry -> tokens = arena_alloc_array(&file_registry -> tokens_arena, TokenArray, count);

    file_registry -> count = 0;

    arena_memset(file_registry -> buckets, U8_MAX, sizeof(FileId) * count);
}

void files_load_stdlib(str8 path) {
    struct dirent* entry;
    DIR* dir = opendir(path.pointer);

    if (dir == null) {
        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_ERROR,
            "unable to open stdlib: %s",
            path
        );

        return;
    }

    while ((entry = readdir(dir)) != null) {
        if (entry -> d_type != DT_REG) continue;

        char* file_name = entry -> d_name;
        u64 size = path.length + strlen(file_name) + 2;

        char* complete_path = arena_alloc(driver_ctx.gpa, size);

        i32 n = snprintf(complete_path, size, "%s/%s", path.pointer, file_name);

        files_intern((str8) { .pointer = complete_path, .length = n });
    }

    closedir(dir);
}

FileId files_intern(str8 path) {
    FileRegistry* interner = &driver_ctx.file_registry;
    
    if (UNLIKELY(interner -> count >= interner -> bucket_capacity * INTERNER_LOAD_FACTOR)) {
        files_buckets_resize(interner);
    }

    u32 hash  = hash_fnv1a_str8(path);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask; 

    while (interner -> buckets[index] != FILE_ID_NONE) {
        FileId id  = interner -> buckets[index];
        File* file = &interner -> entries[id]; 

        if (
            file -> hash == hash &&
            file -> path.length == path.length &&
            memcmp(file -> path.pointer, path.pointer, path.length) == 0
        ) {
            debug_printf("File Registry: Intern FOUND %d for 0x%x\n", id, hash);
            return id;
        }

        index = (index + 1) & mask;
    }

    str8 buffer = allocate_buffer(path);

    if (buffer.length == 0) {
        debug_printf("File Registry: Intern returned early, empty file\n");
        return FILE_ID_NONE;
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        files_entries_resize(interner);
        files_tokens_resize(interner);
    }

    FileId id  = interner -> count++;
    File* file = &interner -> entries[id];

    interner -> buckets[index] = id;

    file -> path   = path;
    file -> hash   = hash; 
    file -> buffer = buffer;
    file -> stage  = FILE_LOADED;

    tokens_init(&interner -> tokens[id]);

    debug_printf("File Registry: Intern RETURNED %d for 0x%x\n", id, hash);
    return id;
}

FileId files_lookup_path(str8 path) {
    FileRegistry* interner = &driver_ctx.file_registry;

    u32 hash  = hash_fnv1a_str8(path);
    u32 index = hash & (interner -> bucket_capacity - 1); 

    while (interner -> buckets[index] != FILE_ID_NONE) {
        FileId id  = interner -> buckets[index];
        File* file = &interner -> entries[id]; 

        if (
            file -> hash == hash &&
            file -> path.length == path.length &&
            memcmp(file -> path.pointer, path.pointer, path.length) == 0
        ) {
            return id;
        }

        index = (index + 1) & (interner -> bucket_capacity - 1);
    }
    
    return FILE_ID_NONE;
}

inline File* files_lookup_id(FileId id) {
    return &driver_ctx.file_registry.entries[id];
}

static void files_buckets_resize(FileRegistry* interner) {
    u64 old_size = interner -> bucket_capacity * sizeof(FileId);
    u64 new_size = old_size * 2;

    u32 new_capacity = interner -> bucket_capacity * 2;

    FileId* new_buckets = arena_alloc(&interner -> interner_arena, new_size);
    arena_memset(new_buckets, U8_MAX, new_size);

    debug_printf("File interner: Buckets resize %ld -> %ld\n", old_size , new_size);

    for (u32 i = 0; i < interner -> count; i++) {
        File* entry = &interner -> entries[i];

        u32 new_index = entry -> hash & (new_capacity - 1);

        while (new_buckets[new_index] != FILE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index] = i;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;
}

static void files_entries_resize(FileRegistry* interner) {
    u64 size = interner -> entry_capacity * sizeof(File); 

    interner -> entries = arena_realloc(&interner -> interner_arena, interner -> entries, size, size * 2);
    interner -> entry_capacity *= 2;

    debug_printf("File interner: Entries resize %ld -> %ld\n", size, size * 2);
}

static void files_tokens_resize(FileRegistry* interner) {
    u64 size = interner -> entry_capacity * sizeof(TokenArray); 

    interner -> tokens = arena_realloc(&interner -> tokens_arena, interner -> tokens, size, size * 2);

    debug_printf("File interner: Token arrays resize %ld -> %ld\n", size, size * 2);
}

static str8 allocate_buffer(str8 path) {
    str8 buffer = {0};

    i32 fd = open(path.pointer, O_RDONLY);
    if (fd < 0) {
        // errno, error diagnostics
        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_ERROR,
            "unable to open file: %.*s",
            path.length,
            path.pointer
        );

        goto exit;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_ERROR,
            "unable to stat file: %s",
            path.pointer
        );

        goto exit_fd_open;
    }

    u64 buffer_size = st.st_size + 1;

    // Empty file
    if (UNLIKELY(buffer_size == 0)) {
        debug_printf("Found empty file '%s', not allocating...\n", path.pointer);

        diagnostic_add_generic(
            &driver_ctx.diagnostics,
            DIAG_WARNING,
            "%s is an empty file",
            path.pointer
        );

        goto exit_fd_open;
    }

    FileRegistry* registry = &driver_ctx.file_registry;

    buffer.pointer = arena_alloc(&registry -> buffers_arena, buffer_size);
    buffer.length = buffer_size;

    buffer.pointer[buffer_size - 1] = 0;

    u64 bytes_read = 0;

    while (bytes_read < buffer_size) {
        i64 n = (i64) read(fd, buffer.pointer + bytes_read, buffer_size - bytes_read);

        if (n == 0) break;

        // TODO: add errors
        assert(n >= 0 && "Read failed");
        bytes_read += n;
    }

    debug_printf(
        "Allocated file '%.*s' (%ld bytes) Hash = 0x%x\n",
        (i32) path.length,
        path.pointer,
        buffer_size,
        hash_fnv1a_str8(path)
    );

exit_fd_open:
    close(fd);

exit:
    return buffer;
}
