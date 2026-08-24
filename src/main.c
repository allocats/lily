#define MEOWRENA_IMPL
#include "meowrena/meowrena.h"
#undef MEOWRENA_IMPL

#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "ast/tree/tree.h"
#include "cli/cli.h"
#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "lexer/lexer.h"
#include "token/token.h"
#include "token/types.h"
#include "utils/timer.h"
#include "utils/types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

DriverCtx driver = {0};

i32 main(i32 argc, char** argv) {
    // compile time asserts to ensure that things are as expected 
    static_assert(8  == sizeof(Token) && "sizeof(Token) != 8 bytes");
    static_assert(64 == sizeof(AstNode) && "sizeof(AstNode) != 64 bytes");

    // loads terminal colours or none if unable to
    cli_init_ansi_codes();

    if (argc < 2) {
        cli_print_usage(argv[0]);
        return 1;
    }
    
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
        cli_print_home_error(argv[0]);
        return 1;
    }

    driver_init(&driver, argc - 1, argv + 1);

    // timer for the frontend (lexing -> IR generation)
    Timer frontend_timer = {0};

    timer_start(&frontend_timer);

    for (u32 i = 0; i < driver.file_interner.count; i++) {
        // lex_and_parse(i);
        lex_file(i);

        File* file = file_lookup_id(i);

        if (file -> stage != FILE_LEXED) continue;

        parse_file(i);
    }

    timer_end(&frontend_timer);

    // timer for llvm
    Timer backend_timer = {0}; 

    timer_start(&backend_timer);
    timer_end(&backend_timer);

    // timer for linker (cc)
    Timer linker_timer = {0}; 

    timer_start(&linker_timer);
    timer_end(&linker_timer);

    if (driver.flags & DRIVER_FLAGS_DUMP_TOKENS) {
        for (u32 i = 0; i < driver.file_interner.count; i++) {
            tokens_print(i);
        }
    }

    if (driver.flags & DRIVER_FLAGS_DUMP_AST) {
        for (u32 i = 0; i < driver.file_interner.count; i++) {
            char buffer[16];
            snprintf(buffer, sizeof(buffer), "ast_%u.txt", i);
            ast_print(buffer, &driver.file_interner.entries[i].ast);
        }
    }

lily_done:
    bool has_errors = diagnostics_print();

    cli_print_compiler_stats(&frontend_timer, &backend_timer, &linker_timer);

    if (!has_errors) {
        printf(
            "%s%s%s:%s compiled %ssuccessfully%s\n",
            ANSI_BOLD,
            ANSI_CYAN,
            argv[0],
            ANSI_RESET,
            ANSI_BOLD,
            ANSI_RESET
        );
    } else {
        printf(
            "%s%s%s:%s compiler %sfailed%s\n",
            ANSI_BOLD,
            ANSI_RED,
            argv[0],
            ANSI_RESET,
            ANSI_BOLD,
            ANSI_RESET
        );
    }

    return 0;
}
