#ifndef LILY_CLI_H
#define LILY_CLI_H

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

#endif // !LILY_CLI_H
