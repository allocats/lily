#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver/types.h"
#include "string_interner/interner.h"
#include "symbols/symbols/types.h"
#include "symbols/table/types.h"
#include "utils/types.h"

extern DriverCtx driver;

static char *SYMBOL_KIND_STRINGS[] = {
    [SYMBOL_ENUM]      = "SYMBOL_ENUM",
    [SYMBOL_FIELD]     = "SYMBOL_FIELD",
    [SYMBOL_FUNCTION]  = "SYMBOL_FUNCTION",
    [SYMBOL_IMPORT]    = "SYMBOL_IMPORT",
    [SYMBOL_PARAMETER] = "SYMBOL_PARAMETER",
    [SYMBOL_STRUCT]    = "SYMBOL_STRUCT",
    [SYMBOL_TYPE]      = "SYMBOL_TYPE",
    [SYMBOL_UNION]     = "SYMBOL_UNION",
    [SYMBOL_VARIABLE]  = "SYMBOL_VARIABLE",
    [SYMBOL_VARIANT]   = "SYMBOL_VARIANT",
    [SYMBOL_ERROR]     = "SYMBOL_ERROR",
};

#define SYMBOL_KIND_STRINGS_COUNT (sizeof(SYMBOL_KIND_STRINGS) / sizeof(SYMBOL_KIND_STRINGS[0]))

static void symbol_print_separator(FILE *out) {
    fprintf(out, "\n======================================================================\n");
}

static void symbol_print_string_id(FILE *out, char *label, StringId id) {
    fprintf(out, "%s: StringId=%u", label, (u32)id);

    if (id != STRING_ID_NONE && id < driver.string_interner.count) {
        fprintf(out, "  value=\"%.*s\"", STR8_PRINT(id));
    } else if (id == STRING_ID_NONE) {
        fprintf(out, "  value=<none>");
    } else {
        fprintf(out, "  value=<invalid StringId>");
    }

    fprintf(out, "\n");
}

// NOTE: `flags` on Symbol is documented as copied verbatim from AstNode's
// flags field, so we decode it with the same AST_FLAGS_* bits.
static void symbol_print_flags(FILE *out, u16 flags) {
    fprintf(out, "flags:          0x%04x", (unsigned)flags);

    if (flags == AST_FLAGS_NONE) {
        fprintf(out, " [NONE]\n");
        return;
    }

    fprintf(out, " [");

    bool first = true;

#define PRINT_FLAG(flag)                                             \
    do {                                                             \
        if (flags & (flag)) {                                        \
            if (!first) fprintf(out, " | ");                         \
            fprintf(out, #flag);                                     \
            first = false;                                           \
        }                                                            \
    } while (0)

    PRINT_FLAG(AST_FLAGS_IS_TOP_DECL);
    PRINT_FLAG(AST_FLAGS_IS_CONSTANT);
    PRINT_FLAG(AST_FLAGS_IS_EXTERNAL);
    PRINT_FLAG(AST_FLAGS_IS_VARIADIC);

#undef PRINT_FLAG

    fprintf(out, "]\n");
}

static void symbol_print_header(FILE *out, Symbol *symbol, u32 index) {
    char *kind_name = "<unknown>";

    if ((u32)symbol -> kind < SYMBOL_KIND_STRINGS_COUNT && SYMBOL_KIND_STRINGS[symbol -> kind] != NULL) {
        kind_name = SYMBOL_KIND_STRINGS[symbol -> kind];
    }

    fprintf(
        out,
        "\n"
        "┌──────────────────────────────────────────────────────────────\n"
        "│ SYMBOL #%u\n"
        "├──────────────────────────────────────────────────────────────\n",
        index
    );

    fprintf(out, "│ address:        %p\n", (void*) symbol);
    fprintf(out, "│ id:             %u\n", (u32)symbol -> id);
    fprintf(out, "│ kind:           %u (%s)\n", (u32)symbol -> kind, kind_name);
    fprintf(out, "│ sizeof(symbol): %zu\n", sizeof(*symbol));

    // ResolveState's named values live in resolver_stack/types.h, which
    // wasn't available when this was generated, so it's printed raw here.
    fprintf(out, "│ state:          %u (ResolveState, raw)\n", (u32)symbol -> state);

    fprintf(out, "│ ");
    symbol_print_flags(out, symbol -> flags);

    fprintf(out, "│ file_id:        %u\n", (u32)symbol -> file_id);

    symbol_print_string_id(out, "│ name", symbol -> name_id);

    fprintf(out, "│ ast_node_id:    %u\n", (u32)symbol -> ast_node_id);
    fprintf(out, "│ hash:           0x%08x\n", symbol -> hash);
}

static void symbol_print_payload(FILE *out, Symbol *symbol) {
    switch (symbol -> kind) {

        case SYMBOL_IMPORT:
            fprintf(out, "│ payload: import_symbol\n");
            fprintf(out, "│   scope_id: ScopeId=%u\n", (u32)symbol -> as.import_symbol.scope_id);
            fprintf(out, "│   file_id:  FileId=%u\n", (u32)symbol -> as.import_symbol.file_id);
            fprintf(out, "│   type_id:  TypeId=%u\n", (u32)symbol -> as.import_symbol.type_id);
            break;

        case SYMBOL_FUNCTION:
            fprintf(out, "│ payload: function_symbol\n");

            fprintf(
                out,
                "│   parameters:\n"
                "│     ids:   %p\n"
                "│     count: %u\n",
                (void*) symbol -> as.function_symbol.parameters,
                symbol -> as.function_symbol.parameter_count
            );

            for (u32 i = 0; i < symbol -> as.function_symbol.parameter_count; i++) {
                fprintf(out, "│       [%u] SymbolId=%u\n", i, (u32)symbol -> as.function_symbol.parameters[i]);
            }

            fprintf(out, "│   return_type_id:    TypeId=%u\n", (u32)symbol -> as.function_symbol.return_type_id);
            fprintf(out, "│   signature_state:    %u (ResolveState, raw)\n", (u32)symbol -> as.function_symbol.signature_state);
            fprintf(out, "│   scope_id:          ScopeId=%u\n", (u32)symbol -> as.function_symbol.scope_id);
            break;

        case SYMBOL_PARAMETER:
            fprintf(out, "│ payload: parameter_symbol\n");
            fprintf(out, "│   type_id: TypeId=%u\n", (u32)symbol -> as.parameter_symbol.type_id);
            break;

        case SYMBOL_ENUM:
            fprintf(out, "│ payload: enum_symbol\n");

            fprintf(
                out,
                "│   variants:\n"
                "│     ids:   %p\n"
                "│     count: %u\n",
                (void*) symbol -> as.enum_symbol.variants,
                symbol -> as.enum_symbol.variant_count
            );

            for (u32 i = 0; i < symbol -> as.enum_symbol.variant_count; i++) {
                fprintf(out, "│       [%u] SymbolId=%u\n", i, (u32)symbol -> as.enum_symbol.variants[i]);
            }

            fprintf(out, "│   resolved_type_id: TypeId=%u\n", (u32)symbol -> as.enum_symbol.resolved_type_id);
            break;

        case SYMBOL_VARIANT:
            fprintf(out, "│ payload: variant_symbol\n");
            fprintf(out, "│   type_id: TypeId=%u\n", (u32)symbol -> as.variant_symbol.type_id);
            fprintf(out, "│   value:   %lld\n", (long long)symbol -> as.variant_symbol.value);
            break;

        case SYMBOL_UNION:
        case SYMBOL_STRUCT:
            fprintf(out, "│ payload: %s\n", symbol -> kind == SYMBOL_UNION ? "union_symbol" : "struct_symbol");

            fprintf(
                out,
                "│   fields:\n"
                "│     ids:   %p\n"
                "│     count: %u\n",
                (void*) symbol -> as.struct_symbol.fields,
                symbol -> as.struct_symbol.field_count
            );

            for (u32 i = 0; i < symbol -> as.struct_symbol.field_count; i++) {
                fprintf(out, "│       [%u] SymbolId=%u\n", i, (u32)symbol -> as.struct_symbol.fields[i]);
            }

            fprintf(out, "│   resolved_type_id: TypeId=%u\n", (u32)symbol -> as.struct_symbol.resolved_type_id);
            break;

        case SYMBOL_FIELD:
            fprintf(out, "│ payload: field_symbol\n");
            fprintf(out, "│   type_id: TypeId=%u\n", (u32)symbol -> as.field_symbol.type_id);
            fprintf(out, "│   offset:  %u\n", symbol -> as.field_symbol.offset);
            break;

        case SYMBOL_VARIABLE:
            fprintf(out, "│ payload: variable_symbol\n");
            fprintf(out, "│   type_id: TypeId=%u\n", (u32)symbol -> as.variable_symbol.type_id);
            break;

        case SYMBOL_TYPE:
            fprintf(out, "│ payload: type_symbol\n");
            fprintf(out, "│   type_id: TypeId=%u\n", (u32)symbol -> as.type_symbol.type_id);
            break;

        case SYMBOL_ERROR:
            fprintf(out, "│ payload: <SYMBOL_ERROR>\n");
            break;

        default:
            fprintf(out, "│ payload: <unhandled SymbolKind=%u>\n", (u32)symbol -> kind);
            break;
    }

    fprintf(out, "└──────────────────────────────────────────────────────────────\n");
}

static void symbol_print(FILE *out, Symbol *symbol, u32 index) {
    symbol_print_header(out, symbol, index);
    symbol_print_payload(out, symbol);
}

void symbol_table_print(char *path, SymbolTable *table) {
    FILE *out = stderr;
    bool close_output = false;

    if (path != NULL) {
        FILE *file = fopen(path, "w");

        if (file != NULL) {
            out = file;
            close_output = true;

            fprintf(stderr, "Dumped Symbol Table to %s\n", path);
        }
    }

    fprintf(
        out,
        "\n"
        "######################################################################\n"
        "#                         SYMBOL TABLE DUMP                        #\n"
        "######################################################################\n"
        "\n"
    );

    fprintf(
        out,
        "SymbolTable\n"
        "├── address:           %p\n"
        "├── symbols:           %p\n"
        "├── symbol_count:      %u\n"
        "├── symbol_capacity:   %u\n"
        "├── symbol_size:       %zu bytes\n"
        "├── allocated_size:    %zu bytes\n"
        "├── scopes:            %p\n"
        "├── scope_count:       %u\n"
        "├── scope_capacity:    %u\n"
        "├── symbol_array_arena:%p\n"
        "├── symbol_data_arena: %p\n"
        "├── scope_array_arena: %p\n"
        "└── scope_data_arena:  %p\n",
        (void*) table,
        (void*) table -> symbols,
        table -> symbol_count,
        table -> symbol_capacity,
        sizeof(Symbol),
        sizeof(Symbol) * (usize)table -> symbol_capacity,
        (void*) table -> scopes,
        table -> scope_count,
        table -> scope_capacity,
        (void*) &table -> symbol_array_arena,
        (void*) &table -> symbol_data_arena,
        (void*) &table -> scope_array_arena,
        (void*) &table -> scope_data_arena
    );

    fprintf(
        out,
        "\n"
        "----------------------------------------------------------------------\n"
        "ARENA: symbol_array_arena\n"
        "address: %p\n"
        "----------------------------------------------------------------------\n",
        (void*) &table -> symbol_array_arena
    );
    arena_print_stats(out, &table -> symbol_array_arena, "symbol_array_arena");

    fprintf(
        out,
        "\n"
        "----------------------------------------------------------------------\n"
        "ARENA: symbol_data_arena\n"
        "address: %p\n"
        "----------------------------------------------------------------------\n",
        (void*) &table -> symbol_data_arena
    );
    arena_print_stats(out, &table -> symbol_data_arena, "symbol_data_arena");

    fprintf(
        out,
        "\n"
        "----------------------------------------------------------------------\n"
        "ARENA: scope_array_arena\n"
        "address: %p\n"
        "----------------------------------------------------------------------\n",
        (void*) &table -> scope_array_arena
    );
    arena_print_stats(out, &table -> scope_array_arena, "scope_array_arena");

    fprintf(
        out,
        "\n"
        "----------------------------------------------------------------------\n"
        "ARENA: scope_data_arena\n"
        "address: %p\n"
        "----------------------------------------------------------------------\n",
        (void*) &table -> scope_data_arena
    );
    arena_print_stats(out, &table -> scope_data_arena, "scope_data_arena");

    symbol_print_separator(out);

    fprintf(
        out,
        "SCOPE ARRAY\n"
        "count=%u capacity=%u scopes=%p\n"
        "(per-scope field dump not included: symbols/scope/types.h wasn't\n"
        " available when this printer was generated — share it and the\n"
        " per-scope breakdown can be added the same way symbols are below.)\n",
        table -> scope_count,
        table -> scope_capacity,
        (void*) table -> scopes
    );

    symbol_print_separator(out);

    fprintf(
        out,
        "SYMBOL ARRAY\n"
        "count=%u capacity=%u symbols=%p\n",
        table -> symbol_count,
        table -> symbol_capacity,
        (void*) table -> symbols
    );

    symbol_print_separator(out);

    for (u32 i = 0; i < table -> symbol_count; i++) {
        symbol_print(out, &table -> symbols[i], i);
        fprintf(out, "\n");
    }

    symbol_print_separator(out);

    fprintf(
        out,
        "SYMBOL TABLE SUMMARY\n"
        "----------------------------------------------------------------------\n"
        "table address:        %p\n"
        "symbols address:      %p\n"
        "symbol count:         %u\n"
        "symbol capacity:      %u\n"
        "symbol size:          %zu bytes\n"
        "allocated symbol mem: %zu bytes\n"
        "used symbol mem:      %zu bytes\n"
        "unused symbol slots:  %u\n"
        "scopes address:       %p\n"
        "scope count:          %u\n"
        "scope capacity:       %u\n",
        (void*) table,
        (void*) table -> symbols,
        table -> symbol_count,
        table -> symbol_capacity,
        sizeof(Symbol),
        sizeof(Symbol) * (usize)table -> symbol_capacity,
        sizeof(Symbol) * (usize)table -> symbol_count,
        table -> symbol_capacity - table -> symbol_count,
        (void*) table -> scopes,
        table -> scope_count,
        table -> scope_capacity
    );

    fprintf(
        out,
        "----------------------------------------------------------------------\n"
        "END SYMBOL TABLE DUMP\n"
        "######################################################################\n"
        "\n"
    );

    if (close_output) {
        fclose(out);
    }
}
