#include "token/token.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define MEOWRENA_IMPL
#include "meowrena/meowrena.h"
#undef MEOWRENA_IMPL

#include "cli/cli.h"
#include "cmd/cmd.h"
#include "utils/timer.h"
#include "utils/types.h"

#include "token/types.h"

i32 main(i32 argc, char** argv) {
    // compile time asserts to ensure that things are as expected 
    static_assert(8 == sizeof(Token) && "sizeof(Token) != 8 bytes");

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

    // timer for the frontend (lexing -> IR generation)
    Timer frontend_timer = {0};

    // timer for llvm
    Timer backend_timer = {0}; 

    // timer for linker (cc)
    Timer linker_timer = {0}; 

    timer_start(&frontend_timer);
    timer_end(&frontend_timer);

    timer_start(&backend_timer);
    timer_end(&backend_timer);

    timer_start(&linker_timer);
    timer_end(&linker_timer);


lily_done:
    bool has_errors = false;

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
