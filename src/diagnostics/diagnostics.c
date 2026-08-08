#include "diagnostics/diagnostics.h"

#include "ast/nodes/types.h"
#include "cli/cli.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "resolver/types.h"
#include "string_interner/interner.h"
#include "symbols/types.h"
#include "token/types.h"
#include "types/ty.h"
#include "types/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern LilyCtx driver_ctx;

static void diagnostic_add_token_span(
    DiagnosticEngine* engine,
    FileId file_id,
    DiagKind kind,
    Span span,
    const char* msg,
    const char* help
);

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

static void diagnostic_add_token_span(
    DiagnosticEngine* engine,
    FileId file_id,
    DiagKind kind,
    Span span,
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

    TokenArray* tokens = &driver_ctx.file_registry.tokens[file_id];

    Token* start_token = &tokens -> items[span.start];
    Token* end_token = &tokens -> items[span.end];

    while (cursor < start_token -> lexeme.pointer) {
        if (*cursor == '\n') {
            line += 1;
            col = 1;
        } else {
            col++;
        }

        cursor++;
    }

    u32 length = end_token -> lexeme.pointer + end_token -> lexeme.length - start_token -> lexeme.pointer;
    u32 length_to_end_of_line = 0;

    while (*cursor != '\0' && *cursor != '\n') {
        length_to_end_of_line++;
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

    diag -> len = MIN(length, length_to_end_of_line);

    diag -> file_id = file_id;

}

void diagnostics_add_unknown_namespace(DiagnosticEngine* engine, Module* module, AstNodeId ident_id) {
    FileId file_id = module_node_file(module, ident_id);

    AstNode* node = &module -> ast.nodes[ident_id];

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);
    char* help = arena_alloc(&engine -> arena, size);
    char* namespace = arena_alloc(&engine -> arena, size);

    u32 count = node -> token_span.end - node -> token_span.start - 1;

    i32 n = 0;

    for (u32 i = 0; i < count; i++) {
        Token* token = &driver_ctx.file_registry.tokens[file_id].items[node -> token_span.start + i];

        if (n < 0 || (u32)n >= size) {
            break;
        }

        i32 written = snprintf(
            namespace + n,
            size - (u32) n,
            "%.*s",
            token -> lexeme.length,
            token -> lexeme.pointer
        );

        if (written < 0) {
            break;
        }

        n += written;
    }

    snprintf(msg, size, "use of undeclared namespace: %s", namespace);
    snprintf(help, size, "try importing: %s", namespace);

    Span span = {
        .start = node -> token_span.start,
        .end = node -> token_span.end - 2
    };

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        span,
        msg,
        help
    );
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

    diagnostic_add_token_span(
        engine,
        file,
        DIAG_ERROR,
        return_type_expr -> token_span,
        "invalid return type",
        "add a valid return type for this function"
    );
}

void diagnostic_add_void_function_returns_value(DiagnosticEngine* engine, Module* module, AstNodeId id) {
    AstNode* node = &module -> ast.nodes[id];
    AstNode* expr = &module -> ast.nodes[node -> as.return_stmt.stmt];

    FileId file_id = module_node_file(module, id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        expr -> token_span,
        "function with no return value returns an expression",
        "either remove this return expression or add a return type to the function"
    );
}

void diagnostic_add_function_expects_but_returns(
    DiagnosticEngine* engine,
    Module* module,
    StringId fn_name,
    AstNodeId return_stmt_id,
    TypeId return_type,
    TypeId found_type
) {
    AstNode* stmt = &module -> ast.nodes[return_stmt_id]; 

    FileId file_id = module_node_file(module, return_stmt_id);

    StringId return_type_name = TYPE_ID_LOOKUP_REF(return_type) -> name;
    StringId found_type_name = TYPE_ID_LOOKUP_REF(found_type) -> name;

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    snprintf(
        msg,
        size,
        "function %.*s return type expects '%.*s', but found type '%.*s'",
        STR8_PRINT(fn_name),
        STR8_PRINT(return_type_name),
        STR8_PRINT(found_type_name)
    );

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        stmt -> token_span,
        msg,
        null
    );
}

void diagnostic_add_cannot_reassign_constant(DiagnosticEngine* engine, Module* module, AstNodeId binop_id) {
    AstNode* binop = &module -> ast.nodes[binop_id];
    AstNode* lhs = &module -> ast.nodes[binop -> as.binary_op.left];

    FileId file_id = module_node_file(module, binop_id);

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    i32 n = snprintf(msg, size, "cannot reassign constant: ");

    u32 count = lhs -> token_span.end - lhs -> token_span.start + 1;

    for (u32 i = 0; i < count; i++) {
        Token* token = &driver_ctx.file_registry.tokens[file_id].items[lhs -> token_span.start + i];

        if (n < 0 || (u32)n >= size) {
            break;
        }

        i32 written = snprintf(
            msg + n,
            size - (u32) n,
            "%.*s",
            token -> lexeme.length,
            token -> lexeme.pointer
        );

        if (written < 0) {
            break;
        }

        n += written;
    }

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        binop -> token_span,
        msg,
        null
    );
}

void diagnostic_add_pointer_compound_assign_requires_integer(DiagnosticEngine* engine, Module* module, AstNodeId rhs_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* rhs = &module -> ast.nodes[rhs_id];

    FileId file_id = module_node_file(module, rhs_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        rhs -> token_span,
        "pointer compound assignment requires an integer offset",
        "change this expression to an integer type"
    );
}

void diagnostic_add_assignment_type_mismatch(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId rhs_id,
    TypeId expected_type,
    TypeId found_type
) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* rhs = &module -> ast.nodes[rhs_id];

    FileId file_id = module_node_file(module, rhs_id);

    StringId expected_type_name = TYPE_ID_LOOKUP_REF(expected_type) -> name;
    StringId found_type_name = TYPE_ID_LOOKUP_REF(found_type) -> name;

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    snprintf(
        msg,
        size,
        "cannot assign value of type '%.*s' to variable of type '%.*s'",
        STR8_PRINT(found_type_name),
        STR8_PRINT(expected_type_name)
    );

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        rhs -> token_span,
        msg,
        null
    );
}

void diagnostic_add_pointer_subtraction_type_mismatch(DiagnosticEngine* engine, Module* module, AstNodeId binop_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* binop = &module -> ast.nodes[binop_id];

    FileId file_id = module_node_file(module, binop_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        binop -> token_span,
        "cannot subtract pointers of different types",
        "ensure both pointers point to the same base type"
    );
}

void diagnostic_add_pointer_arithmetic_requires_integer(DiagnosticEngine* engine, Module* module, AstNodeId rhs_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* rhs = &module -> ast.nodes[rhs_id];

    FileId file_id = module_node_file(module, rhs_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        rhs -> token_span,
        "pointer arithmetic requires an unsigned integer offset",
        "change this expression to an unsigned integer type"
    );
}

void diagnostic_add_comparison_type_mismatch(DiagnosticEngine* engine, Module* module, AstNodeId binop_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* binop = &module -> ast.nodes[binop_id];

    FileId file_id = module_node_file(module, binop_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        binop -> token_span,
        "comparison operands must be the same type",
        "change one of the operands so both sides match"
    );
}

void diagnostic_add_logical_operator_requires_bool(DiagnosticEngine* engine, Module* module, AstNodeId binop_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* binop = &module -> ast.nodes[binop_id];

    FileId file_id = module_node_file(module, binop_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        binop -> token_span,
        "logical operators require bool operands",
        "ensure both operands resolve to 'bool'"
    );
}

void diagnostic_add_cannot_reference_rvalue(DiagnosticEngine* engine, Module* module, AstNodeId operand_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* operand = &module -> ast.nodes[operand_id];

    FileId file_id = module_node_file(module, operand_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        operand -> token_span,
        "cannot take reference of an rvalue",
        "store this value in a variable before taking its address"
    );
}

void diagnostic_add_cannot_dereference_non_pointer(DiagnosticEngine* engine, Module* module, AstNodeId unary_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* unary = &module -> ast.nodes[unary_id];

    FileId file_id = module_node_file(module, unary_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        unary -> token_span,
        "can only dereference pointers",
        "remove the '*' or change this expression to a pointer type"
    );
}

void diagnostic_add_call_argument_count_mismatch(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId call_id,
    u32 expected,
    u32 found,
    bool is_variadic
) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* call = &module -> ast.nodes[call_id];

    FileId file_id = module_node_file(module, call_id);

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    snprintf(
        msg,
        size,
        "expected %s%u argument%s, but found %u",
        is_variadic ? "at least " : "",
        expected,
        expected == 1 ? "" : "s",
        found
    );

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        call -> token_span,
        msg,
        null
    );
}

void diagnostic_add_argument_type_mismatch(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId arg_id,
    TypeId expected_type,
    TypeId found_type,
    u32 arg_index
) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* arg = &module -> ast.nodes[arg_id];

    FileId file_id = module_node_file(module, arg_id);

    StringId expected_type_name = TYPE_ID_LOOKUP_REF(expected_type) -> name;
    StringId found_type_name = TYPE_ID_LOOKUP_REF(found_type) -> name;

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    snprintf(
        msg,
        size,
        "argument %u expects type '%.*s', but found type '%.*s'",
        arg_index,
        STR8_PRINT(expected_type_name),
        STR8_PRINT(found_type_name)
    );

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        arg -> token_span,
        msg,
        null
    );
}

void diagnostic_add_arrow_access_on_non_pointer(DiagnosticEngine* engine, Module* module, AstNodeId access_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* access = &module -> ast.nodes[access_id];

    FileId file_id = module_node_file(module, access_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        access -> token_span,
        "can only use '->' on pointers",
        "use '.' instead"
    );
}

void diagnostic_add_dot_access_on_pointer(DiagnosticEngine* engine, Module* module, AstNodeId access_id) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* access = &module -> ast.nodes[access_id];

    FileId file_id = module_node_file(module, access_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        access -> token_span,
        "cannot use '.' on a pointer",
        "use '->' instead"
    );
}

void diagnostic_add_member_access_invalid_base_type(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId access_id,
    TypeId base_type
) {
    if (engine -> count >= engine -> threshold_value) {
        engine -> count++;
        return;
    }

    AstNode* access = &module -> ast.nodes[access_id];

    FileId file_id = module_node_file(module, access_id);

    StringId base_type_name = TYPE_ID_LOOKUP_REF(base_type) -> name;

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    snprintf(
        msg,
        size,
        "nothing to access on type '%.*s'",
        STR8_PRINT(base_type_name)
    );

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        access -> token_span,
        msg,
        null
    );
}

void diagnostic_add_type_cannot_be_void(DiagnosticEngine* engine, Module* module, AstNodeId type_expr_id) {
    AstNode* node = &module -> ast.nodes[type_expr_id];

    FileId file_id = module_node_file(module, type_expr_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        node -> token_span,
        "type cannot be 'void'",
        "either add indirection (void*) or change the type"
    );
}

void diagnostic_add_type_is_not_an_integer(DiagnosticEngine* engine, Module* module, AstNodeId type_expr_id) {
    AstNode* node = &module -> ast.nodes[type_expr_id];

    FileId file_id = module_node_file(module, type_expr_id);

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        node -> token_span,
        "type does not resolve to an integer",
        "change this type expression"
    );
}

void diagnostic_add_type_does_not_exist(DiagnosticEngine* engine, Module* module, AstNodeId type_expr_id) {
    AstNode* node = &module -> ast.nodes[type_expr_id];

    FileId file_id = module_node_file(module, type_expr_id);

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    i32 n = snprintf(msg, size, "unknown type: ");

    u32 count = node -> token_span.end - node -> token_span.start + 1;

    for (u32 i = 0; i < count; i++) {
        Token* token = &driver_ctx.file_registry.tokens[file_id].items[node -> token_span.start + i];

        if (n < 0 || (u32)n >= size) {
            break;
        }

        i32 written = snprintf(
            msg + n,
            size - (u32) n,
            "%.*s",
            token -> lexeme.length,
            token -> lexeme.pointer
        );

        if (written < 0) {
            break;
        }

        n += written;
    }

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        node -> token_span,
        msg,
        "unknown type here"
    );
}

void diagnostic_add_use_of_undeclared_identifier(DiagnosticEngine* engine, Module* module, AstNode* node) {
    FileId file_id = module_node_file(module, node -> id);

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    i32 n = snprintf(msg, size, "use of undeclared identifier: ");

    u32 count = node -> token_span.end - node -> token_span.start + 1;

    for (u32 i = 0; i < count; i++) {
        Token* token = &driver_ctx.file_registry.tokens[file_id].items[node -> token_span.start + i];

        if (n < 0 || (u32)n >= size) {
            break;
        }

        i32 written = snprintf(
            msg + n,
            size - (u32) n,
            "%.*s",
            token -> lexeme.length,
            token -> lexeme.pointer
        );

        if (written < 0) {
            break;
        }

        n += written;
    }

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        node -> token_span,
        msg,
        "undeclared identifier here"
    );
}

void diagnostic_add_use_of_undeclared_member(
    DiagnosticEngine* engine,
    Module* module,
    AstNodeId access_id,
    StringId object_name,
    StringId member
) {
    FileId file_id = module_node_file(module, access_id);

    AstNode* stmt = &module -> ast.nodes[access_id];

    u32 size = 256;
    char* msg = arena_alloc(&engine -> arena, size);

    snprintf(
        msg,
        size,
        "object '%.*s' does not have a member named '%.*s'",
        STR8_PRINT(object_name),
        STR8_PRINT(member)
    );

    diagnostic_add_token_span(
        engine,
        file_id,
        DIAG_ERROR,
        stmt -> token_span,
        msg,
        "undeclared member access here"
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
