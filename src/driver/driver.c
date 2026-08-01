#include "driver/driver.h"

#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "modules/modules.h"
#include "namespacing/namespacing.h"
#include "string_interner/interner.h"
#include "utils/debug.h"

#include <string.h>

#define FLAG_MATCHES(len, flag, str) ((len == sizeof(str) - 1) && strncmp(flag, str, len) == 0)

#define STDLIB_FILE_COUNT 1

void driver_init(LilyCtx* driver, Arena* gpa, str8 stdlib_path, i32 argc, char** argv) {
    arena_init(gpa, ARENA_KB(8), ALIGN_8);
    debug_printf("Driver: Init gpa with 8KB\n");

    driver -> gpa = gpa;

    i32 estimated_count = ARENA_ALIGN_UP(ALIGN_2, argc + STDLIB_FILE_COUNT);
    debug_printf("Driver: Allocating for %d files\n", estimated_count);

    file_registry_init(estimated_count);
    diagnostic_engine_init();
    string_intnerner_init();
    namespace_interner_init();
    module_registry_init();

    files_load_stdlib(stdlib_path);

    for (i32 i = 0; i < argc; i++) {
        char* arg = argv[i];
        u64 arg_len = strlen(arg);

        if (arg_len == 1) {
            diagnostic_add_generic(&driver -> diagnostics, DIAG_WARNING, "invalid argument: %s", arg);
            continue;
        }

        if (*arg == '-') {
            switch (arg[1]) {
                case 'd': {
                    if (FLAG_MATCHES(arg_len, arg, "-dump-tokens")) {
                        driver -> flags |= LILY_FLAGS_DUMP_TOKENS;
                    } else if (FLAG_MATCHES(arg_len, arg, "-dump-ast")) {
                        driver -> flags |= LILY_FLAGS_DUMP_AST;
                    }
                } break;
            }
        } else {
            str8 path = {
                .pointer = arg,
                .length = arg_len,
            };

            files_intern(path);
        }
    }
}

void driver_destroy(LilyCtx* driver) {
    arena_destroy(&driver -> diagnostics.arena);

    arena_destroy(&driver -> string_interner.arena);

    for (u32 i = 0; i < driver -> file_registry.count; i++) {
        arena_destroy(&driver -> file_registry.tokens[i].arena);
    }

    arena_destroy(&driver -> file_registry.buffers_arena);
    arena_destroy(&driver -> file_registry.interner_arena);
    arena_destroy(&driver -> file_registry.tokens_arena);

    for (u32 i = 0; i < driver -> module_registry.count; i++) {
        Module* module = &driver -> module_registry.entries[i];

        for (u32 k = 0; k < module -> symbol_table.scope_capacity; k++) {
            arena_destroy(&module -> symbol_table.scopes[k].arena);
        }

        arena_destroy(&module -> ast.arena);
        arena_destroy(&module -> symbol_table.arena);
        arena_destroy(&module -> gpa);
    }

    arena_destroy(&driver -> module_registry.arena);
    arena_destroy(&driver -> namespace_interner.arena);

    arena_destroy(driver -> gpa);
}
