#include "ast/parser/directive/directive.h"
#include "ast/parser/parser.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/driver.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "lexer/lexer.h"
#include "string_interner/interner.h"
#include "token/types.h"
#include "types/table/table.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <dirent.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FLAG_MATCHES(len, flag, str) ((len == sizeof(str) - 1) && strncmp(flag, str, len) == 0)

static str8 path_join(Arena* arena, str8 dir, const char* name);
static void create_build_dir(void);
static void destroy_build_dir(void);
static StdlibFiles get_stdlib_files(str8 path);

static Arena scratch = {0};
static Arena stdlib_arena = {0};

static char stdlib_path[PATH_MAX] = {0};

static constexpr char stdlib_dir[] = ".local/lily";
static constexpr char build_path[] = "./.build";

void driver_init(DriverCtx* driver, i32 argc, char** argv, const char* home_dir) {
    assert(driver != null);
    assert(argc > 0);
    assert(argv != null);

    arena_init(&scratch, ARENA_KB(1), ALIGN_DEFAULT);
    debug_printf("Init Driver's static scratch arena with 1KB");

    arena_init(&stdlib_arena, ARENA_KB(2), ALIGN_DEFAULT);
    debug_printf("Init Driver's stdlib arena with 2KB");

    create_build_dir();

    diagnostic_engine_init();
    string_interner_init();

    // asserts that the first string in the interner is "import"
    directive_ids_init();

    type_table_init();

    i32 n = snprintf(stdlib_path, sizeof(stdlib_path), "%s/%s", home_dir, stdlib_dir);
    StdlibFiles stdlib_files = get_stdlib_files((str8) { .ptr = stdlib_path, .len = n });

    driver -> stdlib_path = stdlib_path;

    // get an estimated number of files and round it up
    u64 estimated_count = NEXT_POWER_OF_TWO(argc + stdlib_files.count);
    debug_printf("Allocating for %lu files", estimated_count);

    file_interner_init(estimated_count);

    for (u32 i = 0; i < stdlib_files.count; i++) {
        file_intern(stdlib_files.paths[i]);
    }

    for (i32 i = 0; i < argc; i++) {
        char* arg = argv[i];
        u64 arg_len = strlen(arg);

        if (arg_len == 1) {
            diagnostic_add_generic(DIAG_WARNING, "invalid argument: %s", arg);
            continue;
        }

        if (*arg == '-') {
            switch (arg[1]) {
                case 'd': {
                    if (FLAG_MATCHES(arg_len, arg, "-dump-tokens")) {
                        driver -> flags |= DRIVER_FLAGS_DUMP_TOKENS;
                    } else if (FLAG_MATCHES(arg_len, arg, "-dump-ast")) {
                        driver -> flags |= DRIVER_FLAGS_DUMP_AST;
                    } 
                } break;
            }
        } else {
            str8 path = {
                .ptr = arg,
                .len = arg_len,
            };

            if (file_intern(path) == AST_NODE_ID_NONE) {
                diagnostic_add_generic(
                    DIAG_ERROR,
                    "unable to open file: %.*s",
                    path.len,
                    path.ptr
                );
            }
        }
    }
}

void driver_destroy(DriverCtx* driver) {
    destroy_build_dir();

    arena_destroy(&driver -> diagnostic_engine.arena);
    arena_destroy(&driver -> string_interner.arena);

    for (u32 i = 0; i < driver -> file_interner.count; i++) {
        File* file = &driver -> file_interner.entries[i];

        arena_destroy(&file -> tokens.arena);
        arena_destroy(&file -> ast.gpa);
        arena_destroy(&file -> ast.nodes_arena);
    }

    arena_destroy(&driver -> symbol_table.symbol_data_arena);
    arena_destroy(&driver -> symbol_table.symbol_array_arena);
    arena_destroy(&driver -> symbol_table.scope_data_arena);
    arena_destroy(&driver -> symbol_table.scope_array_arena);

    arena_destroy(&driver -> type_table.nominal_arena);
    arena_destroy(&driver -> type_table.structural_arena);
    arena_destroy(&driver -> type_table.entry_arena);
    arena_destroy(&driver -> type_table.gpa);

    arena_destroy(&driver -> file_interner.interner_arena);
    arena_destroy(&driver -> file_interner.buffer_arena);

    arena_destroy(&scratch);
    arena_destroy(&stdlib_arena);

    path_normalizer_destroy();
}

static str8 path_join(Arena* arena, str8 dir, const char* name) {
    u64 size = dir.len + strlen(name) + 2;
    char* buf = arena_alloc(arena, size);
    i32 n = snprintf(buf, size, "%.*s/%s", (i32)dir.len, dir.ptr, name);
    return (str8){ .ptr = buf, .len = n };
}

static void create_build_dir(void) {
    mkdir("./.build/", 0700);
}

static void destroy_build_dir(void) {
    struct dirent* entry;
    DIR* dir = opendir("./.build/");

    assert(dir != null);

    while ((entry = readdir(dir)) != null) {
        if (entry -> d_type != DT_REG) continue;

        char* file_name = entry -> d_name;

        str8 path = path_join(
            &scratch,
            (str8) { .ptr = (char*) build_path, .len = sizeof(build_path) - 1 },
            file_name
        );

        remove(path.ptr);

        arena_reset(&scratch);
    }

    closedir(dir);

    rmdir("./.build/");
}

static void push_stdlib_file(StdlibFiles* files, str8 path) {
    if (files->count >= files->capacity) {
        u64 old_size = files->capacity * sizeof(str8);
        u64 new_size = old_size * 2;

        files->paths = arena_realloc(&stdlib_arena, files->paths, old_size, new_size);
        files->capacity *= 2;
    }

    files->paths[files->count] = path;
    files->count += 1;
}

static void collect_stdlib_files(StdlibFiles* files, str8 path) {
    struct dirent* entry;
    DIR* dir = opendir(path.ptr);

    if (dir == null) {
        diagnostic_add_generic(
            DIAG_ERROR,
            "unable to open stdlib: %s",
            path
        );

        return;
    }

    while ((entry = readdir(dir)) != null) {
        char* name = entry -> d_name;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        str8 complete_path = path_join(&scratch, path, name);

        if (entry -> d_type == DT_DIR) {
            collect_stdlib_files(files, complete_path);
        } else if (entry -> d_type == DT_REG) {
            push_stdlib_file(files, complete_path);
        }
    }

    closedir(dir);
}

static StdlibFiles get_stdlib_files(str8 path) {
    StdlibFiles files = {
        .paths = arena_alloc(&stdlib_arena, sizeof(str8) * 16),
        .count = 0,
        .capacity = 16
    };

    collect_stdlib_files(&files, path);

    return files;
}

inline void lex_and_parse(FileId id) {
    File* file = file_lookup_id(id);

    if (file -> stage != FILE_ALLOCATED) return;

    lex_file(id);
    
    file = file_lookup_id(id);

    if (file -> stage != FILE_LEXED) return;

    parse_file(id);
}
