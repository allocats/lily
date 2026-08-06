#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/parser.h"
#include "ast/parser/types/ty.h"
#include "diagnostics/diagnostics.h"
#include "string_interner/interner.h"
#include "utils/debug.h"
#include "utils/macros.h"

AstNodeId parse_union_decl(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_UNION);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    node -> as.union_decl.fields = arena_alloc(&p -> module -> ast.gpa_arena, sizeof(AstNodeId) * 4);
    node -> as.union_decl.field_capacity = 4;
    node -> as.union_decl.field_count = 0;

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

    node -> as.union_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(name);

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
        Token* field_name = parser_advance(p);

        if (field_name -> kind != TOK_IDENT) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                field_name,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier here"
            );

            return parser_error_decl(p, node);
        }

        if (!parser_check(p, TOK_COLON)) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                field_name,
                DIAG_LOC_END_OF_TOK,
                "expected ':'",
                "add a ':' here"
            );

            return parser_error_decl(p, node);
        }

        parser_advance(p);

        AstNodeId field_type_expr = parse_type_expr(p);

        if (field_type_expr == AST_NODE_ID_NONE) {
            return parser_error_decl(p, node);
        }

        if (!parser_check(p, TOK_SEMI)) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                parser_peek_previous(p),
                DIAG_LOC_END_OF_TOK,
                "expected ';'",
                "add a ';' here"
            );

            return parser_error_decl(p, node);
        }

        parser_advance(p);

        if (UNLIKELY(node -> as.union_decl.field_count >= node -> as.union_decl.field_capacity)) {
            u64 size = sizeof(AstNodeId) * node -> as.union_decl.field_capacity;

            node -> as.union_decl.fields = arena_realloc(
                &p -> module -> ast.gpa_arena,
                node -> as.union_decl.fields,
                size,
                size * 2
            );
            node -> as.union_decl.field_capacity *= 2;

            debug_printf(
                "Union(%.*s) fields realloc from %ld -> %ld bytes\n",
                name -> lexeme.length,
                name -> lexeme.pointer,
                size,
                size * 2
            );
        }

        AstNodeId field_id  = parser_create_node(p, AST_FIELD);
        AstNode* field_node = ast_node_get(&p -> module -> ast, field_id);

        field_node -> source_token = field_name;
        field_node -> as.field_decl.name_id = STRING_INTERNER_LOOKUP_TOKEN(field_name);
        field_node -> as.field_decl.type_expr = field_type_expr;

        node -> as.union_decl.fields[node -> as.union_decl.field_count++] = field_id;
    }

    parser_advance(p);

    node -> token_span.end = p -> cursor;

    return id;
}
