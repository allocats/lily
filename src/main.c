#include "types/ty.h"
#define MEOWRENA_IMPL
#include "meowrena/meowrena.h"
#undef MEOWRENA_IMPL

#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "ast/tree/tree.h"
#include "cli/cli.h"
#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "lexer/lexer.h"
#include "symbols/symbols.h"
#include "token/token.h"
#include "utils/timer.h"
#include "utils/types.h"

#define STDLIB_PATH ".local/lily/std"
#define STDLIB_PATH_SIZE 1024

// todo: one day write it all myself :p or llvm LOL
#define LINKER_PATH "cc"
#define QBE_PATH    "qbe"

LilyCtx driver_ctx = {0};
char lily_stdlib_path[STDLIB_PATH_SIZE] = {0};

static Arena gpa = {0};

i32 main(i32 argc, char** argv) {
    // compile time asserts used to ensure behaviour is as expected
    static_assert(sizeof(Token) == 16 && "Token != 16 bytes\n");
    static_assert(sizeof(AstNode) == 64 && "AstNode != 64 bytes\n");

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

    // get real path to the stdlib
    i32 n = snprintf(lily_stdlib_path, sizeof(lily_stdlib_path), "%s/%s", home_dir, STDLIB_PATH);

    str8 lily_stdlib_path_str8 = {
        .pointer = lily_stdlib_path,
        .length = n
    };

    // sets up the entire compiler, loads stdlib
    driver_init(&driver_ctx, &gpa, lily_stdlib_path_str8, argc - 1, argv + 1);

    // timer for the frontend (lexing -> IR generation)
    Timer frontend_timer = {0};
    timer_start(&frontend_timer);

    // lex and parse files into modules
    // TODO: multithread this!
    for (u32 i = 0; i < driver_ctx.file_registry.count; i++) {
        lexer_tokenize_file(i);
        parser_parse_file(i);
    }

    // register top level symbols
    for (u32 i = 0; i < driver_ctx.module_registry.count; i++) {
        symbols_register_top_level_declarations(i);
    }

    resolve_types();

    // resolve symbols
    for (u32 i = 0; i < driver_ctx.module_registry.count; i++) {
        symbols_resolve(i);
    }

    timer_end(&frontend_timer);

    Timer backend_timer = {0}; 
    timer_start(&backend_timer);
    timer_end(&backend_timer);

    Timer linker_timer = {0}; 
    timer_start(&linker_timer);
    timer_end(&linker_timer);

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

    if (driver_ctx.flags & LILY_FLAGS_DUMP_TYPES) {
        print_type_table(&driver_ctx.type_table);
    }

    bool has_errors = diagnostics_print(&driver_ctx.diagnostics);

    cli_print_compiler_stats(&frontend_timer, &backend_timer, &linker_timer);

    if (has_errors) {
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

    // going to use execvp() to call qbe and cc for linking
    //execvp(qbe);
    //execvp(linker);

    driver_destroy(&driver_ctx);
    return 0;
}
