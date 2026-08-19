#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "files/files.h"
#include "string_interner/interner.h"
#include "utils/debug.h"

#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FLAG_MATCHES(len, flag, str) ((len == sizeof(str) - 1) && strncmp(flag, str, len) == 0)

static u64 next_pow2(u64 x);

void driver_init(DriverCtx* driver, i32 argc, char** argv) {
    assert(driver != null);
    assert(argc > 0);
    assert(argv != null);

    diagnostic_engine_init();
    string_interner_init();

    u64 estimated_count = next_pow2(argc);
    file_interner_init(estimated_count);
    debug_printf("Allocating for %lu files", estimated_count);

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
                    } 
                    // else if (FLAG_MATCHES(arg_len, arg, "-dump-ast")) {
                    //     driver -> flags |= LILY_FLAGS_DUMP_AST;
                    // } else if (FLAG_MATCHES(arg_len, arg, "-dump-types")) {
                    //     driver -> flags |= LILY_FLAGS_DUMP_TYPES;
                    // }
                } break;
            }
        } else {
            str8 path = {
                .ptr = arg,
                .len = arg_len,
            };

            file_intern(path);
        }
    }
}

static u64 next_pow2(u64 x) {
	return x == 1 ? 1 : 1 << (64 - __builtin_clzl(x - 1));
}

// static void create_build_dir(void) {
//     mkdir("./.build/", 0700);
// }
//
// static void destroy_build_dir(void) {
//     struct dirent* entry;
//     DIR* dir = opendir("./.build/");
//
//     assert(dir != null);
//
//     while ((entry = readdir(dir)) != null) {
//         if (entry -> d_type != DT_REG) continue;
//
//         char* file_name = entry -> d_name;
//
//         u64 size = strlen("./.build/") + strlen(file_name) + 2;
//
//         char* complete_path = arena_alloc(driver_ctx.gpa, size);
//
//         snprintf(complete_path, size, "./.build/%s", file_name);
//
//         remove(complete_path);
//     }
//
//     closedir(dir);
//
//     rmdir("./.build/");
// }
