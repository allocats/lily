#define MEOWRENA_IMPL
#include "meowrena/meowrena.h"
#undef  MEOWRENA_IMPL

#include "ast/nodes/types.h"
#include "ast/tree/tree.h"
#include "cli/cli.h"
#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "driver/types.h"
#include "files/types.h"
#include "symbols/register/register.h"
#include "symbols/table/table.h"
#include "token/token.h"
#include "token/types.h"
#include "types/resolve/resolve.h"
#include "utils/timer.h"
#include "utils/types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

DriverCtx driver = {0};

static inline void lex_and_parse_files(void) {
    u32 file_count = driver.file_interner.count;

    for (u32 i = 0; i < file_count; i++) {
        lex_and_parse(i);
    }
}

static inline void register_top_level_symbols(void) {
    u32 file_count = driver.file_interner.count;

    for (u32 i = 0; i < file_count; i++) {
        register_top_level_symbols_for_file(i);
    }
}

i32 main(i32 argc, char** argv) {
    // compile time asserts to ensure that things are as expected 
    static_assert(64 == sizeof(AstNode) && "sizeof(AstNode) != 64 bytes");
    static_assert(32 == sizeof(Scope) && "sizeof(Scope) != 32 bytes");
    static_assert(40 == sizeof(Symbol) && "sizeof(Symbol) != 40 bytes");
    static_assert(8  == sizeof(Token) && "sizeof(Token) != 8 bytes");

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

    driver_init(&driver, argc - 1, argv + 1, home_dir);

    // timer for the frontend (lexing -> IR generation)
    Timer frontend_timer = {0};

    timer_start(&frontend_timer);

    lex_and_parse_files();

    u32 file_count = driver.file_interner.count;
    u32 total_declaration_count = 0;

    for (u32 i = 0; i < file_count; i++) {
        total_declaration_count += driver.file_interner.entries[i].ast.declaration_count;
    }

    symbol_table_init(total_declaration_count);

    register_top_level_symbols();

    resolve_top_level_types();

    // resolve_symbols();
    //
    // type_check_modules();

    timer_end(&frontend_timer);

    if (driver.diagnostic_engine.error_count > 0) {
        goto lily_done;
    }

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
            ast_print(buffer, i);
        }
    }

lily_done:
    bool has_errors = diagnostics_print();

    cli_print_compiler_stats(&frontend_timer, &backend_timer, &linker_timer);

    i32 exit_code = 0;

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

        exit_code = 0;
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

        exit_code = 1;
    }

    driver_destroy(&driver);

    return exit_code;
}
