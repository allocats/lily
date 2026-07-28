#include "diagnostics/diagnostics.h"

#include "cli/cli.h"
#include "driver/types.h"
#include "files/files.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern LilyCtx driver_ctx;

void diagnostic_engine_init(void) {
    DiagnosticEngine* diag_engine = &driver_ctx.diagnostics;

    arena_init(&diag_engine -> arena, ARENA_KB(2), ALIGN_8);
    debug_printf("Driver: Allocated diagnostic's arena with 2KB\n");

    diag_engine -> diags    = arena_alloc_array(&diag_engine -> arena, Diagnostic, DIAG_DEFAULT_THRESHOLD);
    diag_engine -> capacity = DIAG_DEFAULT_THRESHOLD;
    diag_engine -> count    = 0;
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

static const char* get_line_col_indent(u32 line) {
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

    u32 digits = 1;

    while (line >= 10) {
        line /= 10;
        digits++;
    }

    if (UNLIKELY(digits >= sizeof(indents) / sizeof(indents[0]))) {
        return indents[sizeof(indents) / sizeof(indents[0]) - 1];
    }

    return indents[digits];
}

static const char* get_source_line(const char* buffer, u32 line, u32* len) {
    u32 current_line = 1;

    const char* cursor = (char*) buffer;

    while (current_line < line) {
        if (*cursor == '\n') {
            current_line += 1;
        }

        cursor++;
    }

    const char* start = cursor;

    while (*cursor != '\n') {
        cursor++;
    }

    *len = cursor - start;

    return start;
}

static Diagnostic* diagnostic_get_new(DiagnosticEngine* engine) {
    if (UNLIKELY(engine -> count >= engine -> capacity)) {
        u64 size = sizeof(Diagnostic) * engine -> capacity;

        engine -> diags = arena_realloc(&engine -> arena, engine -> diags, size, size * 2);
        engine -> capacity *= 2;

        debug_printf("Diagnostics realloc from %ld -> %ld bytes\n", size, size * 2);
    }

    return &engine -> diags[engine -> count++];
}

void diagnostic_add_generic(DiagnosticEngine* engine, DiagKind kind, char* fmt, ...) {
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
    diag -> is_generic = true;
    diag -> msg.pointer = buffer;
    diag -> msg.length  = bytes;
}

void diagnostic_add_token(
    DiagnosticEngine* engine,
    FileId file_id,
    DiagKind kind,
    Token* tok,
    u8 loc,
    char* msg,
    char* help
) {
    u32 line = 1;
    u32 col = 1;

    File* file = files_lookup_id(file_id);
    char* cursor = file -> buffer.pointer;

    while (cursor < tok -> lexeme.pointer) {
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

    diag -> is_generic = false;

    diag -> kind = kind;

    diag -> msg.pointer = msg;
    diag -> msg.length = strlen(msg);

    diag -> help.pointer = help;
    diag -> help.length  = help ? strlen(help) : 0;

    diag -> line = line;
    diag -> col = col;

    if (loc & DIAG_LOC_START_OF_TOK) {
        diag -> len = 1;
    } else if (loc & DIAG_LOC_END_OF_TOK) {
        diag -> len = 1;
        diag -> col += tok -> lexeme.length;
    } else if (loc & DIAG_LOC_WHOLE_TOK) {
        diag -> len = MIN(tok -> lexeme.length, len_to_end_of_line);
    } else {
        debug_printf("Unspecified location somehow lexeme: %.*s", tok -> lexeme.length, tok -> lexeme.pointer);
        diag -> len = MIN(tok -> lexeme.length, len_to_end_of_line);
    }

    diag -> file_id = file_id;
}

void diagnostics_print(DiagnosticEngine* engine) {
    FILE* fd = stdout;

    if (engine -> dump_path != null) {
        fd = fopen((char*) engine -> dump_path, "w+");

        if (fd == null) {
            // add diag for this
            fd = stdout;
        }
    }

    for (u32 i = 0; i < engine -> count; i++) {
        Diagnostic diag = engine -> diags[i];

        const char* level_colour = match_level_colour(diag.kind); 
        const char* level = match_level(diag.kind); 

        if (diag.is_generic) {
            fprintf(
                fd,
                "%s%s:%s %s%.*s%s\n",
                level_colour,
                level,
                ANSI_RESET,
                ANSI_BOLD,
                (i32) diag.msg.length,
                diag.msg.pointer,
                ANSI_RESET
            );

            continue;
        }

        File* file = files_lookup_id(diag.file_id);

        const char* line_indent = get_line_col_indent(diag.line);

        u32 source_line_length = 0;

        const char* source_line = get_source_line(file -> buffer.pointer, diag.line, &source_line_length);

        // Header 
        fprintf(
            fd,
            "%s%s:%s %s%.*s%s\n",
            level_colour,
            level,
            ANSI_RESET,
            ANSI_BOLD,
            (i32) diag.msg.length,
            diag.msg.pointer,
            ANSI_RESET
        );

        // Location
        fprintf(
            fd,
            " %s%s-->%s %.*s:%u:%u\n",
            ANSI_MAGENTA,
            ANSI_BOLD,
            ANSI_RESET,
            (int) file -> path.length,
            file -> path.pointer,
            diag.line,
            diag.col
        );

        // Source context
        fprintf(fd, "%s %s|%s\n", line_indent, ANSI_BOLD, ANSI_RESET);
        fprintf(fd, "%u %s|%s %.*s\n", diag.line, ANSI_BOLD, ANSI_RESET, source_line_length, source_line);
        fprintf(fd, "%s %s| %s", line_indent, ANSI_BOLD, ANSI_RESET);

        u32 spaces = diag.col - 1;
        fprintf(fd, "%*s", spaces, "");

        fprintf(fd, "%s%s", ANSI_GREEN, ANSI_BOLD);
        u32 caret_len = diag.len;

        for (u32 i = 0; i < caret_len; i++) {
            fprintf(fd, "^");
        }

        fprintf(fd, "%s", ANSI_RESET);

        if (diag.help.pointer) {
            fprintf(
                fd,
                "%s%s help: %.*s%s",
                ANSI_BOLD,
                ANSI_GREEN,
                (i32) diag.help.length,
                diag.help.pointer,
                ANSI_RESET
            );
        }

        fprintf(fd, "\n");
        fprintf(fd, "%s %s|%s\n\n", line_indent, ANSI_BOLD, ANSI_RESET);
    }

    if (fd != stdout) {
        fclose(fd);
    }
}
