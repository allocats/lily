#include "cli/cli.h"

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
}
