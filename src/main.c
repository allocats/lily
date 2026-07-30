#define MEOWRENA_IMPL
#include "meowrena/meowrena.h"
#undef MEOWRENA_IMPL

#include "ast/parser/parser.h"
#include "ast/tree/tree.h"
#include "cli/cli.h"
#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "files/types.h"
#include "lexer/lexer.h"
#include "symbols/symbols.h"
#include "token/token.h"
#include "utils/macros.h"
#include "utils/timer.h"
#include "utils/types.h"

LilyCtx driver_ctx = {0};

i32 main(i32 argc, char** argv) {
    static_assert(sizeof(Token) == 16 && "Token != 16 bytes\n");

    cli_init_ansi_codes();

    if (UNLIKELY(argc < 2)) {
        cli_print_usage(argv[0]);
        return 1;
    }

    driver_init(&driver_ctx, argc - 1, argv + 1);

    Timer timer = {0};

    timer_start(&timer);

    for (u32 i = 0; i < driver_ctx.file_registry.count; i++) {
        lexer_tokenize_file(i);
        parser_parse_file(i);
    }

    // register top level symbols
    for (u32 i = 0; i < driver_ctx.module_registry.count; i++) {
        symbols_register(i);
    }

    // resolve symbols
    for (u32 i = 0; i < driver_ctx.module_registry.count; i++) {
        // symbols_resolve(i);
    }

    timer_end(&timer);

    if (driver_ctx.flags & LILY_FLAGS_DUMP_TOKENS) {
        for (u32 i = 0; i < driver_ctx.file_registry.count; i++) {
            File* file = &driver_ctx.file_registry.entries[i];

            printf("[%.*s]:\n", (i32) file -> path.length, file -> path.pointer);
            tokens_print(&driver_ctx.file_registry.tokens[i]);
            printf("\n\n");
        }
    } 

    if (driver_ctx.flags & LILY_FLAGS_DUMP_AST) {
        for (u32 i = 0; i < driver_ctx.module_registry.count; i++) {
            Module* module = &driver_ctx.module_registry.entries[i];
            ast_print(&module -> ast);
        }
    } 

    if (diagnostics_print(&driver_ctx.diagnostics)) {
        printf(
            "%s%s%s:%s compiled %ssuccessfully%s in %lfms\n",
            ANSI_BOLD,
            ANSI_CYAN,
            argv[0],
            ANSI_RESET,
            ANSI_BOLD,
            ANSI_RESET,
            time_elapsed_in_ms(&timer)
        );
    } else {
        printf(
            "%s%s%s:%s compiler %sfailed%s took %lfms\n",
            ANSI_BOLD,
            ANSI_RED,
            argv[0],
            ANSI_RESET,
            ANSI_BOLD,
            ANSI_RESET,
            time_elapsed_in_ms(&timer)
        );
    }

    driver_destroy(&driver_ctx);
    return 0;
}
