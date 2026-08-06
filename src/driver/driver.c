#include "driver/driver.h"

#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "modules/modules.h"
#include "namespacing/namespacing.h"
#include "string_interner/interner.h"
#include "symbols/symbols.h"
#include "types/ty.h"
#include "utils/debug.h"

#include <string.h>

#define FLAG_MATCHES(len, flag, str) ((len == sizeof(str) - 1) && strncmp(flag, str, len) == 0)

#define STDLIB_FILE_COUNT 1

static u64 next_pow2(u64 x) {
	return x == 1 ? 1 : 1 << (64 - __builtin_clzl(x - 1));
}

void driver_init(LilyCtx* driver, Arena* gpa, str8 stdlib_path, i32 argc, char** argv) {
    arena_init(gpa, ARENA_KB(8), ALIGN_8);
    debug_printf("Driver: Init gpa with 8KB\n");

    driver -> gpa = gpa;

    u64 estimated_count = next_pow2(argc + STDLIB_FILE_COUNT);
    debug_printf("Driver: Allocating for %d files\n", estimated_count);

    file_registry_init(estimated_count);
    diagnostic_engine_init();
    string_interner_init();
    namespace_interner_init();
    module_registry_init();
    symbol_table_builtins_init();
    type_table_init();

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
                    } else if (FLAG_MATCHES(arg_len, arg, "-dump-types")) {
                        driver -> flags |= LILY_FLAGS_DUMP_TYPES;
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

    //arena_print_stats(&driver -> string_interner.arena, "String Interner Arena");
    arena_destroy(&driver -> string_interner.arena);

    for (u32 i = 0; i < driver -> file_registry.count; i++) {
        //arena_print_stats(&driver -> file_registry.tokens[i].arena, "Tokens Arena");
        arena_destroy(&driver -> file_registry.tokens[i].arena);
    }

    //arena_print_stats(&driver -> file_registry.buffers_arena, "Buffers Arena");
    arena_destroy(&driver -> file_registry.buffers_arena);

    //arena_print_stats(&driver -> file_registry.interner_arena, "File Interner Arena");
    arena_destroy(&driver -> file_registry.interner_arena);

    //arena_print_stats(&driver -> file_registry.tokens_arena, "File Registry TokenArray Arena");
    arena_destroy(&driver -> file_registry.tokens_arena);

    for (u32 i = 0; i < driver -> module_registry.count; i++) {
        Module* module = &driver -> module_registry.entries[i];

        //arena_print_stats(&module -> ast.gpa_arena, "Module AST GPA Arena");
        arena_destroy(&module -> ast.gpa_arena);

        //arena_print_stats(&module -> ast.nodes_arena, "Module AST Nodes Arena");
        arena_destroy(&module -> ast.nodes_arena);

        //arena_print_stats(&module -> symbol_table.arena, "Module Symbol Table Arena");
        arena_destroy(&module -> symbol_table.arena);

        //arena_print_stats(&module -> gpa, "Module GPA Arena");
        arena_destroy(&module -> gpa);
    }

    //arena_print_stats(&driver -> builtins.arena, "Builtins Arena");
    arena_destroy(&driver -> builtins.arena);

    //arena_print_stats(&driver -> type_table.arena, "Type Table Arena");
    arena_destroy(&driver -> type_table.arena);

    //arena_print_stats(&driver -> module_registry.arena, "Module Registry Arena");
    arena_destroy(&driver -> module_registry.arena);

    //arena_print_stats(&driver -> namespace_interner.arena, "Namespace Interner Arena");
    arena_destroy(&driver -> namespace_interner.arena);

    //arena_print_stats(driver -> gpa, "GPA Arena");
    arena_destroy(driver -> gpa);
}
