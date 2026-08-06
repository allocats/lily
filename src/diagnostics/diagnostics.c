#include "diagnostics/diagnostics.h"

#include "ast/nodes/types.h"
#include "cli/cli.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "resolver/types.h"
#include "symbols/types.h"
#include "types/types.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern LilyCtx driver_ctx;

static FileId module_node_file(Module* module, AstNodeId id) {
    FileId file_id = 0;

    for (u32 i = 0; i < module->file_count; i++) {
        if (id >= module->ast_offsets[i]) {
            file_id = module->files[i];
        } else if (id < module -> ast_offsets[i]) {
            return file_id;
        }
    }

    return file_id;
}

void diagnostic_engine_init(void) {
    DiagnosticEngine* diag_engine = &driver_ctx.diagnostics;

    arena_init(&diag_engine -> arena, ARENA_KB(2), ALIGN_8);
    debug_printf("Driver: Allocated diagnostic's arena with 2KB\n");

    diag_engine -> diags    = arena_alloc_array(&diag_engine -> arena, Diagnostic, DIAG_DEFAULT_THRESHOLD);
    diag_engine -> capacity = DIAG_DEFAULT_THRESHOLD;
    diag_engine -> count    = 0;

    // TODO: Make this configurable through cli
    diag_engine -> threshold_value = DIAG_DEFAULT_THRESHOLD;
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
    diag -> is_generic = true;
    diag -> msg.pointer = buffer;
    diag -> msg.length  = n;
}

void diagnostic_add_token(
    DiagnosticEngine* engine,
    FileId file_id,
    DiagKind kind,
    Token* tok,
    u8 loc,
    const char* msg,
    const char* help
) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

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

    diag -> msg.pointer = (char*) msg;
    diag -> msg.length = strlen(msg);

    diag -> help.pointer = (char*) help;
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

static const char* symbol_exists_match_def(SymbolKind kind) {
    switch (kind) {
        case SYM_STRUCT:
            return "struct is already defined";
        case SYM_UNION:
            return "union is already defined";
        case SYM_ENUM:
            return "enum is already defined";
        case SYM_FIELD:
            return "field is already defined";
        case SYM_VARIANT:
            return "variant is already defined";
        case SYM_FUNCTION:
            return "function is already defined";
        case SYM_PARAMETER:
            return "parameter is already defined";
        default:
            return "symbol is arleady defined";
    }
}

static const char* symbol_exists_match_redef(SymbolKind kind) {
    switch (kind) {
        case SYM_STRUCT:
            return "struct is redefined";
        case SYM_UNION:
            return "union is redefined";
        case SYM_ENUM:
            return "enum is redefined";
        case SYM_FIELD:
            return "field is redefined";
        case SYM_VARIANT:
            return "variant is redefined";
        case SYM_FUNCTION:
            return "function is redefined";
        case SYM_PARAMETER:
            return "parameter is redefined";
        default:
            return "symbol is redefined";
    }
}

static const char* symbol_exists_match_def_help(SymbolKind kind) {
    switch (kind) {
        case SYM_STRUCT:
            return "struct is redefined here";
        case SYM_UNION:
            return "union is redefined here";
        case SYM_ENUM:
            return "enum is redefined here";
        case SYM_FIELD:
            return "field is redefined here";
        case SYM_VARIANT:
            return "variant is redefined here";
        case SYM_FUNCTION:
            return "function is redefined here";
        case SYM_PARAMETER:
            return "parameter is redefined here";
        default:
            return "symbol is redefined here";
    }
}

static const char* symbol_exists_match_redef_help(SymbolKind kind) {
    switch (kind) {
        case SYM_STRUCT:
            return "previous struct definition is here";
        case SYM_UNION:
            return "previous union definition is here";
        case SYM_ENUM:
            return "previous enum definition is here";
        case SYM_FIELD:
            return "previous field definition is here";
        case SYM_VARIANT:
            return "previous variant definition is here";
        case SYM_FUNCTION:
            return "previous function definition is here";
        case SYM_PARAMETER:
            return "previous parameter definition is here";
        default:
            return "previous symboldefinition is here";
    }
}

void diagnostic_add_symbol_already_defined(
    DiagnosticEngine* engine,
    Module* module,
    SymbolId symbol_id,
    AstNodeId new_node_id
) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    SymbolTable* table = &module->symbol_table;
    Symbol* symbol = &table->symbols[symbol_id];

    AstNodeId old_node_id = symbol -> declaration;

    AstNode* old_node = &module->ast.nodes[old_node_id];
    AstNode* new_node = &module->ast.nodes[new_node_id];

    FileId old_file = module_node_file(module, old_node_id);
    FileId new_file = module_node_file(module, new_node_id);

    const char* def_msg   = symbol_exists_match_def(symbol -> kind);
    const char* redef_msg = symbol_exists_match_redef(symbol -> kind);

    const char* def_help_msg   = symbol_exists_match_def_help(symbol -> kind);
    const char* redef_help_msg = symbol_exists_match_redef_help(symbol -> kind);

    diagnostic_add_token(
        engine,
        new_file,
        DIAG_ERROR,
        new_node->source_token,
        DIAG_LOC_WHOLE_TOK,
        redef_msg,
        def_help_msg
    );

    diagnostic_add_token(
        engine,
        old_file,
        DIAG_NOTE,
        old_node->source_token,
        DIAG_LOC_WHOLE_TOK,
        def_msg, 
        redef_help_msg
    );
}

void diagnostic_add_symbol_is_builtin(
    DiagnosticEngine* engine,
    Module* module,
    SymbolId symbol_id,
    AstNodeId node_id
) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    SymbolTable* table = &module -> symbol_table;
    Symbol* symbol = &table -> symbols[symbol_id];

    AstNode* node = &module -> ast.nodes[node_id];

    FileId file = module_node_file(module, node_id);

    Token* token = node -> source_token;

    char* is_a_msg = null;
    u32 size = token -> lexeme.length + 1;

    switch (symbol -> kind) {
        case SYM_TYPE:
            is_a_msg = "is a builtin type";
            size += sizeof("is a builtin type");
            break;

        default:
            is_a_msg = "is a builtin symbol";
            size += sizeof("is a builtin symbol");
            break;
    }

    char* msg = arena_alloc(&engine -> arena, size);

    snprintf(msg, size, "%.*s %s", token->lexeme.length, token->lexeme.pointer, is_a_msg);

    diagnostic_add_token(
        engine,
        file,
        DIAG_ERROR,
        token,
        DIAG_LOC_WHOLE_TOK,
        msg,
        null
    );
}

void diagnostic_add_resolver_type_cycle(DiagnosticEngine* engine, i32 resolver_id) {
    ResolveItem item = driver_ctx.resolver_stack.items[resolver_id];

    TypeEntry* type = &driver_ctx.type_table.entries[item.as.type];
    Module* module = &driver_ctx.module_registry.entries[item.module_id];
    SymbolTable* table = &driver_ctx.module_registry.entries[type -> declaration.module_id].symbol_table;
    Symbol* type_sym = &table -> symbols[type -> declaration.symbol_id];
    AstNode* node = &module -> ast.nodes[type_sym -> declaration];

    FileId file = module_node_file(module, type_sym -> declaration);

    diagnostic_add_token(
        engine,
        file,
        DIAG_ERROR,
        node -> source_token,
        DIAG_LOC_WHOLE_TOK,
        "type recursively includes itself",
        "add some indirection if you wish to recursively embed the struct! (e.g. Foo*)"
    );
}

void diagnostic_add_resolver_symbol_cycle(DiagnosticEngine* engine, i32 resolver_id) {
    ResolveItem item = driver_ctx.resolver_stack.items[resolver_id];

    Module* module = &driver_ctx.module_registry.entries[item.module_id];
    Symbol* symbol = &module -> symbol_table.symbols[item.as.symbol];
    AstNode* node = &module -> ast.nodes[symbol -> declaration];

    FileId file = module_node_file(module, symbol -> declaration);

    diagnostic_add_token(
        engine,
        file,
        DIAG_ERROR,
        node -> source_token,
        DIAG_LOC_WHOLE_TOK,
        "symbol recursively includes itself",
        "stop that"
    );
}

void diagnostic_add_return_type_invalid(DiagnosticEngine* engine, Module* module, SymbolId symbol_id) {
    SymbolTable* table = &module -> symbol_table;
    Symbol* symbol = &table -> symbols[symbol_id];

    FileId file = module_node_file(module, symbol -> declaration);

    AstNodeId func_id = symbol -> declaration;
    AstNode* func_node = &module -> ast.nodes[func_id];

    AstNode* return_type_expr = &module -> ast.nodes[func_node -> as.func_decl.return_type_expr];

    diagnostic_add_token(
        engine,
        file,
        DIAG_ERROR,
        func_node -> source_token,
        DIAG_LOC_WHOLE_TOK,
        "invalid return type",
        "add a valid type here"
    );
}

bool diagnostics_print(DiagnosticEngine* engine) {
    u32 count = MIN(engine -> count, engine -> threshold_value);

    if (count == 0) return true;
    
    FILE* fd = stderr;;

    if (engine -> dump_path != null) {
        fd = fopen((char*) engine -> dump_path, "w+");

        if (fd == null) {
            // add diag for this
            fd = stderr;
        }
    }

    bool no_errors = true;

    for (u32 i = 0; i < count; i++) {
        Diagnostic diag = engine -> diags[i];

        const char* level_colour = match_level_colour(diag.kind); 
        const char* level = match_level(diag.kind); 

        if (diag.kind == DIAG_ERROR) no_errors = false;

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

    fflush(fd);

    if (fd != stderr) {
        fclose(fd);
    }

    return no_errors;
}
