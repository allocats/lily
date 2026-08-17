#ifndef LILY_CLI_H
#define LILY_CLI_H

#include "utils/timer.h"

extern const char* ANSI_RED;
extern const char* ANSI_GREEN;
extern const char* ANSI_YELLOW;
extern const char* ANSI_BLUE;
extern const char* ANSI_MAGENTA;
extern const char* ANSI_CYAN;
extern const char* ANSI_BOLD;
extern const char* ANSI_RESET;

void cli_init_ansi_codes(void);
void cli_print_usage(const char* arg);
void cli_print_home_error(const char* arg);

void cli_print_compiler_stats(Timer* frontend_timer, Timer* backend_timer, Timer* linker_timer);

#endif // !LILY_CLI_H
