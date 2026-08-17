#include "cli/cli.h"
#include "utils/timer.h"

#include <stdio.h>
#include <unistd.h>

const char* ANSI_RED;
const char* ANSI_GREEN;
const char* ANSI_YELLOW;
const char* ANSI_BLUE;
const char* ANSI_MAGENTA;
const char* ANSI_CYAN;
const char* ANSI_BOLD;
const char* ANSI_RESET;

void cli_init_ansi_codes(void) {
    if (isatty(STDOUT_FILENO)) {
        ANSI_RED = "\x1b[31m";
        ANSI_GREEN = "\x1b[32m";
        ANSI_YELLOW = "\x1b[33m";
        ANSI_BLUE = "\x1b[34m";
        ANSI_MAGENTA = "\x1b[35m";
        ANSI_CYAN = "\x1b[36m";
        ANSI_BOLD = "\x1b[1m";
        ANSI_RESET = "\x1b[0m";
    } else {
        ANSI_RED = "";
        ANSI_GREEN = "";
        ANSI_YELLOW = "";
        ANSI_BLUE = "";
        ANSI_MAGENTA = "";
        ANSI_CYAN = "";
        ANSI_BOLD = "";
        ANSI_RESET = "";
    }
}

void cli_print_usage(const char* arg) {
    fprintf(stderr, "%s: %serror:%s %sno input files%s\n", arg, ANSI_RED, ANSI_RESET, ANSI_BOLD, ANSI_RESET);
    fflush(stderr);
}

void cli_print_home_error(const char* arg) {
    fprintf(stderr, "%s: %serror:%s %scannot get home path%s\n", arg, ANSI_RED, ANSI_RESET, ANSI_BOLD, ANSI_RESET);
    fflush(stderr);
}

void cli_print_compiler_stats(Timer* frontend_timer, Timer* backend_timer, Timer* linker_timer) {
    f64 frontend_time = timer_elapsed_seconds(frontend_timer);
    f64 backend_time = timer_elapsed_seconds(backend_timer);
    f64 linker_time = timer_elapsed_seconds(linker_timer);

    fprintf(stderr, "\nFrontend time: %.6fs\n", frontend_time);
    fprintf(stderr, "Backend time: %.6fs\n", backend_time);
    fprintf(stderr, "Linker time: %.6fs\n\n", linker_time);
    fprintf(stderr, "Total time: %.6fs\n\n", frontend_time + backend_time + linker_time);

    fflush(stderr);
}
