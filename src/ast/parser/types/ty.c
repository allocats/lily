#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"

#include <stdlib.h>

TypeId parse_type(Parser* p) {
    u32 flags = 0;
    u32 pointer_depth = 0;

    if (parser_check(p, TOK_CONST)) {
        parser_advance(p);
    }

    Token* base_tok = parser_peek(p);
    TypeId type_id  = 0;

    if (base_tok -> kind == TOK_IDENT) {
        // type = intern
        parser_advance(p);
    } else {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            base_tok,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier or bulitin type",
            "add a valid type here" 
        );

        // return (TypeQual) { .id = TYPE_ID_INVALID, .flags = TYPE_QUAL_NONE };
        return 0;
    }

    while (parser_check(p, TOK_STAR)) {
        parser_advance(p);

        pointer_depth += 1;
    }

    if (pointer_depth > 0) {
        // type_id = intern
    }

    if (parser_check(p, TOK_LBRACKET)) {
        parser_advance(p);

        usize array_size = 0;

        if (!parser_check(p, TOK_RBRACKET)) {
            if (!parser_check(p, TOK_INTEGER_LIT) /* TODO: ident || !parser_check(p, TOK_IDENT) */) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    parser_peek(p),
                    DIAG_LOC_WHOLE_TOK,
                    "expected integer literal for array size",
                    "add a valid array size here"
                );

                // return (TypeQual) { .id = TYPE_ID_INVALID, .flags = TYPE_QUAL_NONE };
                return 0;
            }

            Token* size_tok = parser_peek(p);

            u8 c = *(size_tok -> lexeme.pointer + size_tok -> lexeme.length);
            *(size_tok -> lexeme.pointer + size_tok -> lexeme.length) = 0;

            array_size = (usize) strtoll((char*) size_tok -> lexeme.pointer, NULL, 10);

            *(size_tok -> lexeme.pointer + size_tok -> lexeme.length) = c;

            parser_advance(p);
        }

        if (!parser_check(p, TOK_RBRACKET)) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                parser_peek_previous(p),
                DIAG_LOC_END_OF_TOK,
                "expected ']'",
                "add a ']' here"
            );

            // return (TypeQual) { .id = TYPE_ID_INVALID, .flags = TYPE_QUAL_NONE };
            return 0;
        }

        parser_advance(p);

        // type = type_table_intern_array(&driver_ctx.types, type, array_size);
    }

    // return (TypeQual) { .id = type_id, .flags = flags };
    return 0;
}
