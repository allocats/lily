#include "ast/tree/tree.h"
#include "cli/cli.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "resolver_stack/types.h"
#include "string_interner/interner.h"
#include "symbols/table/table.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern DriverCtx driver;

static constexpr u64 diagnostic_init_arena_size_kb = 4;
static constexpr u64 diagnostic_default_threshold  = 32;

static constexpr u64 diagnostic_max_length = 256;

static constexpr u32 diagnostic_multiline_max_lines = 6;

static_assert(sizeof(Diagnostic) * diagnostic_default_threshold < ARENA_KB(diagnostic_init_arena_size_kb));
static_assert(diagnostic_init_arena_size_kb > 0);

void diagnostic_engine_init(void) {
    DiagnosticEngine* diag_engine = &driver.diagnostic_engine;

    arena_init(&diag_engine -> arena, ARENA_KB(diagnostic_init_arena_size_kb), ALIGN_DEFAULT);
    debug_printf("Init diagnostic's arena with %luKB", diagnostic_init_arena_size_kb);

    diag_engine -> diags    = arena_alloc_array(&diag_engine -> arena, Diagnostic, diagnostic_default_threshold);
    diag_engine -> capacity = diagnostic_default_threshold;
    diag_engine -> count    = 0;

    // TODO: Make this configurable through cli
    diag_engine -> threshold_value = diagnostic_default_threshold;
}

static const char* match_level_colour(DiagKind kind) {
    switch (kind) {
        case DIAG_NOTE: {
            return ANSI_BLUE;
        } break;

        case DIAG_WARNING: {
            return ANSI_YELLOW;
        } break;

        case DIAG_ERROR: {
            return ANSI_RED;
        } break;
    }
}

static const char* match_level(DiagKind kind) {
    switch (kind) {
        case DIAG_NOTE: {
            return "note";
        } break;

        case DIAG_WARNING: {
            return "warning";
        } break;

        case DIAG_ERROR: {
            return "error";
        } break;
    }
}

static u32 count_digits(u32 n) {
    u32 digits = 1;

    while (n >= 10) {
        n /= 10;
        digits++;
    }

    return digits;
}

static const char* spaces_indent(u32 width) {
    static const char* indents[] = {
        "",
        " ",
        "  ",
        "   ",
        "    ",
        "     ",
        "      ",
        "       ",
        "        ",
        "         ",
        "          "
    };

    if (UNLIKELY(width >= sizeof(indents) / sizeof(indents[0]))) {
        return indents[sizeof(indents) / sizeof(indents[0]) - 1];
    }

    return indents[width];
}

static const char* get_line_col_indent(u32 line) {
    return spaces_indent(count_digits(line));
}

static void buffer_offset_line_col(const char* buffer, u32 offset, u32* out_line, u32* out_col) {
    u32 line = 1;
    u32 col = 1;

    for (u32 i = 0; i < offset; i++) {
        if (buffer[i] == '\n') {
            line += 1;
            col = 1;
        } else {
            col++;
        }
    }

    *out_line = line;
    *out_col = col;
}

static str8 get_source_line(const char* buffer, u32 line) {
    assert(buffer);

    u32 current_line = 1;

    char* cursor = (char*) buffer;

    while (current_line < line) {
        if (*cursor == '\n') {
            current_line += 1;
        }

        cursor++;
    }

    char* start = cursor;

    while (*cursor != '\n') {
        cursor++;
    }

    return (str8) {
        .ptr = start,
        .len = cursor - start
    };
}

static Diagnostic* diagnostic_get_new(DiagnosticEngine* engine) {
    assert(engine != null);

    if (UNLIKELY(engine -> count >= engine -> capacity)) {
        u64 size = sizeof(Diagnostic) * engine -> capacity;

        engine -> diags = arena_realloc(&engine -> arena, engine -> diags, size, size * 2);
        engine -> capacity *= 2;

        debug_printf("Diagnostics engine -> diags realloc from %lu -> %lu bytes", size, size * 2);
    }

    return &engine -> diags[engine -> count++];
}

void diagnostic_add_generic(DiagKind kind, char* fmt, ...) {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    if (kind == DIAG_ERROR) engine -> error_count++;

    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    char* buffer = null;
    u64 bytes = 0;
    u64 n = 0;

    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(buffer, bytes, (char*) fmt, ap);
    va_end(ap);

    assert(n > 0 && "diagnostic_add_generic() vsnprintf returned less than 0");

    bytes = n + 1;
    buffer = arena_alloc(&engine -> arena, bytes);

    va_start(ap, fmt);
    n = vsnprintf((char*) buffer, bytes, (char*) fmt, ap);
    va_end(ap);

    assert(n > 0 && "diagnostic_add_generic() vsnprintf returned less than 0");

    Diagnostic* diag = diagnostic_get_new(engine); 

    diag -> kind = kind;
    diag -> presentation = DIAG_PRESENTATION_GENERIC;
    diag -> msg.ptr = buffer;
    diag -> msg.len = n;
}

void diagnostic_add_token(
    FileId file_id,
    DiagKind kind,
    Token* tok,
    u8 loc,
    const char* msg,
    const char* help
) {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    if (kind == DIAG_ERROR) engine -> error_count++;

    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    u32 line = 1;
    u32 col = 1;

    File* file = file_lookup_id(file_id);
    char* cursor = file -> buffer.ptr;
    char* tok_start = file -> buffer.ptr + tok -> start;

    while (cursor < tok_start) {
        if (*cursor == '\n') {
            line += 1;
            col = 1;
        } else {
            col++;
        }

        cursor++;
    }

    u32 len_to_end_of_line = 0;

    while (*cursor != '\0' && *cursor != '\n') {
        len_to_end_of_line++;
        cursor++;
    }

    Diagnostic* diag = diagnostic_get_new(engine); 

    diag -> presentation = DIAG_PRESENTATION_SINGLE;

    diag -> kind = kind;

    diag -> msg.ptr = (char*) msg;
    diag -> msg.len = strlen(msg);

    diag -> help.ptr = (char*) help;
    diag -> help.len = help ? strlen(help) : 0;

    diag -> line = line;
    diag -> col = col;

    if (loc & DIAG_LOC_START_OF_TOK) {
        diag -> len = 1;
    } else if (loc & DIAG_LOC_END_OF_TOK) {
        diag -> len = 1;
        diag -> col += tok -> length;
    } else if (loc & DIAG_LOC_WHOLE_TOK) {
        diag -> len = MIN(tok -> length, len_to_end_of_line);
    } else {
        debug_printf("Unspecified location somehow lexeme: %.*s", tok -> length, tok_start);
        diag -> len = MIN(tok -> length, len_to_end_of_line);
    }

    diag -> file_id = file_id;
}

void diagnostic_add_token_span(
    FileId file_id,
    DiagKind kind,
    SpanU32 span,
    const char* msg,
    const char* help
) {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    if (kind == DIAG_ERROR) engine -> error_count++;

    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    File* file = file_lookup_id(file_id);

    Token* start_tok = &file -> tokens.items[span.start];
    Token* end_tok   = &file -> tokens.items[span.end];

    char* cursor = file -> buffer.ptr;
    char* tok_start = file -> buffer.ptr + start_tok -> start;

    u32 line = 1;
    u32 col = 1;

    while (cursor < tok_start) {
        if (*cursor == '\n') {
            line += 1;
            col = 1;
        } else {
            col++;
        }

        cursor++;
    }

    u32 span_end = end_tok -> start + end_tok -> length;
    u32 len = span_end - start_tok -> start;
    
    Diagnostic* diag = diagnostic_get_new(engine);

    diag -> presentation = DIAG_PRESENTATION_SINGLE;
    diag -> kind = kind;

    diag -> msg.ptr = (char*) msg;
    diag -> msg.len = strlen(msg);

    diag -> help.ptr = (char*) help;
    diag -> help.len = help ? strlen(help) : 0;

    diag -> line = line;
    diag -> col = col;
    diag -> len = len;
    diag -> file_id = file_id;
}

void diagnostic_add_node_field(
    FileId file_id,
    DiagKind kind,
    SpanU32 outer,
    SpanU32 inner,
    const char* msg,
    const char* help
) {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    if (kind == DIAG_ERROR) engine -> error_count++;

    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    File* file = file_lookup_id(file_id);

    Token* outer_start_tok = &file -> tokens.items[outer.start];
    Token* outer_end_tok   = &file -> tokens.items[outer.end];
    Token* inner_start_tok = &file -> tokens.items[inner.start];
    Token* inner_end_tok   = &file -> tokens.items[inner.end];

    Diagnostic* diag = diagnostic_get_new(engine);

    diag -> presentation = DIAG_PRESENTATION_MULTILINE;
    diag -> kind = kind;
    diag -> file_id = file_id;

    diag -> msg.ptr = (char*) msg;
    diag -> msg.len = strlen(msg);

    diag -> help.ptr = (char*) help;
    diag -> help.len = help ? strlen(help) : 0;

    u32 unused_col;

    buffer_offset_line_col(file -> buffer.ptr, outer_start_tok -> start, &diag -> multiline.outer_start_line, &unused_col);

    buffer_offset_line_col(
        file -> buffer.ptr,
        outer_end_tok -> start + (outer_end_tok -> length > 0 ? outer_end_tok -> length - 1 : 0),
        &diag -> multiline.outer_end_line,
        &unused_col
    );

    buffer_offset_line_col(
        file -> buffer.ptr,
        inner_start_tok -> start,
        &diag -> multiline.inner_start_line,
        &diag -> multiline.inner_start_col
    );

    buffer_offset_line_col(
        file -> buffer.ptr,
        inner_end_tok -> start + (inner_end_tok -> length > 0 ? inner_end_tok -> length - 1 : 0),
        &diag -> multiline.inner_end_line,
        &diag -> multiline.inner_end_col
    );
}

void diagnostic_add_symbol_redefined(FileId file_id, AstNodeId node_id, SymbolId symbol_id, StringId name_id) {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    File* defined_file = file_lookup_id(symbol -> file_id);
    File* redefined_file = file_lookup_id(file_id);

    AstNode* defined_node = ast_get_node(&defined_file -> ast, symbol -> ast_node_id);
    AstNode* redefined_node = ast_get_node(&redefined_file -> ast, node_id);

    char* defined_msg = arena_alloc(&driver.diagnostic_engine.arena, diagnostic_max_length);

    StringEntry defined_name = STRING_ID_LOOKUP(symbol -> name_id);

    snprintf(defined_msg, diagnostic_max_length, "%.*s is defined here", STR8_FMT(defined_name.str));

    diagnostic_add_token(
        defined_file -> id,
        DIAG_NOTE,
        &defined_file -> tokens.items[defined_node -> tokens.start],
        DIAG_LOC_WHOLE_TOK,
        defined_msg,
        null
    );

    char* redefined_msg = arena_alloc(&driver.diagnostic_engine.arena, diagnostic_max_length);

    StringEntry redefined_mame = STRING_ID_LOOKUP(name_id);

    snprintf(redefined_msg, diagnostic_max_length, "%.*s is redefined here", STR8_FMT(redefined_mame.str));

    diagnostic_add_token(
        redefined_file -> id,
        DIAG_ERROR,
        &redefined_file -> tokens.items[redefined_node -> tokens.start],
        DIAG_LOC_WHOLE_TOK,
        redefined_msg,
        null
    );
}

void diagnostic_add_symbol_does_not_exist(FileId file_id, AstNodeId node_id, StringId name_id) {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    File* file = file_lookup_id(file_id);
    AstNode* node = &file -> ast.nodes[node_id];

    StringEntry str = STRING_ID_LOOKUP(name_id);

    char* msg = arena_alloc(&engine -> arena, diagnostic_max_length); 

    snprintf(msg, diagnostic_max_length, "\'%.*s\' does not exist", STR8_FMT(str.str));

    diagnostic_add_token_span(
        file_id,
        DIAG_ERROR,
        node -> tokens,
        msg,
        null 
    );
}

void diagnostic_add_symbol_cycle(ResolveQuery query) {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    SymbolId symbol_id = query.as.symbol;
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    File* file = file_lookup_id(symbol -> file_id);

    AstNode* node = &file -> ast.nodes[symbol -> ast_node_id];

    diagnostic_add_token_span(
        symbol -> file_id,
        DIAG_ERROR,
        node -> tokens,
        "symbol recursively includes itself",
        "add indirection if you wish to recursively embed the symbol (e.g. Foo*)"
    );
}

static void print_source_row(FILE* fd, File* file, u32 width, u32 line) {
    str8 source_line = get_source_line(file -> buffer.ptr, line);

    fprintf(
        fd,
        "%*u %s|%s %.*s\n",
        width,
        line,
        ANSI_BOLD,
        ANSI_RESET,
        (i32) source_line.len,
        source_line.ptr
    );
}

static void print_caret_row(FILE* fd, u32 width, u32 col, u32 caret_len, str8 help) {
    fprintf(fd, "%s %s|%s ", spaces_indent(width), ANSI_BOLD, ANSI_RESET);

    u32 spaces = col - 1;
    fprintf(fd, "%*s", spaces, "");

    fprintf(fd, "%s%s", ANSI_GREEN, ANSI_BOLD);

    for (u32 i = 0; i < caret_len; i++) {
        fprintf(fd, "^");
    }

    fprintf(fd, "%s", ANSI_RESET);

    if (help.ptr) {
        fprintf(fd, "%s%s help: %.*s%s", ANSI_BOLD, ANSI_GREEN, (i32) help.len, help.ptr, ANSI_RESET);
    }

    fprintf(fd, "\n");
}

static void print_block_line(FILE* fd, File* file, u32 width, u32 line, DiagMultilineData m, str8 help) {
    print_source_row(fd, file, width, line);

    if (line < m.inner_start_line || line > m.inner_end_line) {
        return;
    }

    bool is_first = line == m.inner_start_line;
    bool is_last  = line == m.inner_end_line;

    u32 col = is_first ? m.inner_start_col : 1;
    u32 end_col;

    if (is_last) {
        end_col = m.inner_end_col;
    } else {
        str8 source_line = get_source_line(file -> buffer.ptr, line);
        end_col = source_line.len;
    }

    u32 caret_len = end_col >= col ? (end_col - col + 1) : 1;

    print_caret_row(fd, width, col, caret_len, is_last ? help : (str8) { .ptr = null, .len = 0 });
}

static void diagnostics_print_multiline(FILE* fd, Diagnostic diag) {
    File* file = file_lookup_id(diag.file_id);
    DiagMultilineData m = diag.multiline;

    const char* level_colour = match_level_colour(diag.kind);
    const char* level = match_level(diag.kind);

    // header
    fprintf(
        fd,
        "%s%s:%s %s%.*s%s\n",
        level_colour,
        level,
        ANSI_RESET,
        ANSI_BOLD,
        (i32) diag.msg.len,
        diag.msg.ptr,
        ANSI_RESET
    );

    // hocation: the actual offending span
    fprintf(
        fd,
        " %s%s-->%s %.*s:%u:%u\n",
        ANSI_MAGENTA,
        ANSI_BOLD,
        ANSI_RESET,
        (int) file -> path.len,
        file -> path.ptr,
        m.inner_start_line,
        m.inner_start_col
    );

    u32 width = count_digits(m.outer_end_line);
    const char* blank_indent = spaces_indent(width);

    fprintf(fd, "%s %s|%s\n", blank_indent, ANSI_BOLD, ANSI_RESET);

    u32 total_lines = m.outer_end_line - m.outer_start_line + 1;

    if (total_lines <= diagnostic_multiline_max_lines) {
        for (u32 line = m.outer_start_line; line <= m.outer_end_line; line++) {
            print_block_line(fd, file, width, line, m, diag.help);
        }
    } else {
        print_source_row(fd, file, width, m.outer_start_line);

        if (m.inner_start_line > m.outer_start_line + 1) {
            fprintf(fd, "%*s %s|%s\n", width, "...", ANSI_BOLD, ANSI_RESET);
        }

        for (u32 line = m.inner_start_line; line <= m.inner_end_line; line++) {
            if (line == m.outer_start_line) {
                // already printed above (the field lives on the decl's own line).
                continue;
            }

            print_block_line(fd, file, width, line, m, diag.help);
        }
    }

    fprintf(fd, "%s %s|%s\n\n", blank_indent, ANSI_BOLD, ANSI_RESET);
}

bool diagnostics_print() {
    DiagnosticEngine* engine = &driver.diagnostic_engine;

    bool has_errors = engine -> error_count != 0 ? true : false;

    u32 count = MIN(engine -> count, engine -> threshold_value);
    
    if (count == 0) return false;
    
    FILE* fd = stderr;

    if (engine -> dump_path != null) {
        fd = fopen((char*) engine -> dump_path, "w+");

        if (fd == null) {
            // TODO: add diag for this
            fd = stderr;
        }
    }

    fprintf(fd, "\n");

    for (u32 i = 0; i < count; i++) {
        Diagnostic diag = engine -> diags[i];

        const char* level_colour = match_level_colour(diag.kind); 
        const char* level = match_level(diag.kind); 

        if (diag.presentation == DIAG_PRESENTATION_GENERIC) {
            fprintf(
                fd,
                "%s%s:%s %s%.*s%s\n\n",
                level_colour,
                level,
                ANSI_RESET,
                ANSI_BOLD,
                (i32) diag.msg.len,
                diag.msg.ptr,
                ANSI_RESET
            );

            continue;
        }

        if (diag.presentation == DIAG_PRESENTATION_MULTILINE) {
            diagnostics_print_multiline(fd, diag);
            continue;
        }

        File* file = file_lookup_id(diag.file_id);

        const char* line_indent = get_line_col_indent(diag.line);

        str8 source_line = get_source_line(file -> buffer.ptr, diag.line);

        // Header 
        fprintf(
            fd,
            "%s%s:%s %s%.*s%s\n",
            level_colour,
            level,
            ANSI_RESET,
            ANSI_BOLD,
            (i32) diag.msg.len,
            diag.msg.ptr,
            ANSI_RESET
        );

        // Location
        fprintf(
            fd,
            " %s%s-->%s %.*s:%u:%u\n",
            ANSI_MAGENTA,
            ANSI_BOLD,
            ANSI_RESET,
            (int) file -> path.len,
            file -> path.ptr,
            diag.line,
            diag.col
        );

        // Source context
        fprintf(fd, "%s %s|%s\n", line_indent, ANSI_BOLD, ANSI_RESET);
        fprintf(fd, "%u %s|%s %.*s\n", diag.line, ANSI_BOLD, ANSI_RESET, source_line.len, source_line.ptr);
        fprintf(fd, "%s %s| %s", line_indent, ANSI_BOLD, ANSI_RESET);

        u32 spaces = diag.col - 1;
        fprintf(fd, "%*s", spaces, "");

        fprintf(fd, "%s%s", ANSI_GREEN, ANSI_BOLD);
        u32 caret_len = diag.len == 0 ? 1 : diag.len;

        for (u32 i = 0; i < caret_len; i++) {
            fprintf(fd, "^");
        }

        fprintf(fd, "%s", ANSI_RESET);

        if (diag.help.ptr) {
            fprintf(
                fd,
                "%s%s help: %.*s%s",
                ANSI_BOLD,
                ANSI_GREEN,
                (i32) diag.help.len,
                diag.help.ptr,
                ANSI_RESET
            );
        }

        fprintf(fd, "\n");
        fprintf(fd, "%s %s|%s\n\n", line_indent, ANSI_BOLD, ANSI_RESET);
    }

    fflush(fd);

    if (fd != stderr) {
        fclose(fd);
    }

    return has_errors;
}
