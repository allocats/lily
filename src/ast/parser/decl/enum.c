#include "ast/parser/decl/decl.h"
#include "ast/parser/types/ty.h"
#include "ast/parser/parser.h"
#include "ast/nodes/nodes.h"
#include "diagnostics/diagnostics.h"
#include "string_interner/interner.h"
#include "utils/debug.h"
#include "utils/macros.h"

AstNodeId parse_enum_decl(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_ENUM);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    node -> kind = AST_ENUM;

    node -> as.enum_decl.variants = arena_alloc(&p -> module -> ast.arena, sizeof (AstNodeId) * 4);
    node -> as.enum_decl.variant_capacity = 4;
    node -> as.enum_decl.variant_count = 0;

    Token* name = parser_advance(p);

    if (name -> kind != TOK_IDENT) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            name,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier",
            "add a valid identifier here"
        );

        return parser_error_decl(p, node);
    }

    node -> as.enum_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(name);

    if (parser_check(p, TOK_COLON)) {
        parser_advance(p);

        node -> as.enum_decl.type_expr = parse_type_expr(p);
    }

    if (!parser_check(p, TOK_LBRACE)) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            p -> id,
            DIAG_ERROR,
            parser_peek_previous(p),
            DIAG_LOC_END_OF_TOK,
            "expected '{'",
            "add a '{' here"
        );

        return parser_error_decl(p, node);
    }

    parser_advance(p);

    while (!parser_check(p, TOK_RBRACE)) {
        Token* variant_tok = parser_advance(p);

        if (variant_tok -> kind != TOK_IDENT) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                variant_tok,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier here"
            );

            return parser_error_decl(p, node);
        }

        if (!parser_check(p, TOK_SEMI)) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                variant_tok, 
                DIAG_LOC_END_OF_TOK,
                "expected ';'",
                "add a ';' here"
            );

            return parser_error_decl(p, node);
        }

        parser_advance(p);

        if (UNLIKELY(node -> as.enum_decl.variant_count >= node -> as.enum_decl.variant_capacity)) {
            u64 size = sizeof(AstNodeId) * node -> as.enum_decl.variant_capacity;

            node -> as.enum_decl.variants = arena_realloc(
                &p -> module -> ast.arena,
                node -> as.enum_decl.variants,
                size,
                size * 2
            );
            node -> as.enum_decl.variant_capacity *= 2;

            debug_printf(
                "Enum(%.*s) variants realloc from %ld -> %ld bytes\n",
                name -> lexeme.length,
                name -> lexeme.pointer,
                size,
                size * 2
            );
        }

        AstNodeId variant_id = parser_create_node(p, AST_VARIANT);
        AstNode* variant_node = ast_node_get(&p -> module -> ast, variant_id);

        node -> as.enum_decl.variants[node -> as.enum_decl.variant_count++] = variant_id;

        variant_node -> as.variant_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(variant_tok);

        // TODO: Add value for each variant
    }

    parser_advance(p);

    return id;
}
