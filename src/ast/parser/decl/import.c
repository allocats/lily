#include "ast/nodes/nodes.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/parser.h"
#include "diagnostics/diagnostics.h"
#include "namespacing/namespacing.h"
#include "string_interner/interner.h"

AstNodeId parse_import_decl(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_IMPORT);
    AstNode* node = ast_node_get(&p -> module -> ast, id);

    StringId segments[NAMESPACE_MAX_DEPTH] = { STRING_ID_NONE };
    u32 segment_count = 0;

    while (p -> cursor < p -> token_count) {
        Token* segment = parser_peek(p);

        if (segment -> kind != TOK_IDENT) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                segment,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier here"
            );

            return parser_error_decl(p, node);
        }

        if ((segment_count) >= NAMESPACE_MAX_DEPTH) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                segment,
                DIAG_LOC_END_OF_TOK,
                "max namespacing depth achieved (8)",
                "shorten the namespacing, max of 8 segments is supported"
            );

            return parser_error_decl(p, node);
        }

        segments[segment_count++] = STRING_INTERNER_LOOKUP_TOKEN(segment);
        
        parser_advance(p);

        if (parser_check(p, TOK_SEMI)) break;

        if (!parser_check(p, TOK_COLON_COLON) && !parser_check(p, TOK_SEMI)) {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p -> id,
                DIAG_ERROR,
                segment,
                DIAG_LOC_END_OF_TOK,
                "expected '::' or ';'",
                "add a '::' or ';' here"
            );

            return parser_error_decl(p, node);
        }

        parser_advance(p);
    }

    parser_advance(p);

    node -> as.import_decl.namespace_id = namespace_intern(segments, segment_count);

    return id;
}
