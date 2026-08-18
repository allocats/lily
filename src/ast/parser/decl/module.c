#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/parser.h"
#include "ast/tree/tree.h"
#include "diagnostics/diagnostics.h"
#include "ids.h"
#include "namespacing/namespacing.h"
#include "string_interner/interner.h"
#include "utils/debug.h"
#include "utils/macros.h"

AstNodeId parse_module_decl(Parser* p) {
    AstNodeId id = parser_create_node(p, AST_MODULE_DECL, AST_FLAGS_IS_TOP_DECL, -1);

    StringId* segments = arena_alloc(&p -> gpa, sizeof(StringId) * 4);
    u16 segment_count = 0;
    u16 segment_capacity = 4;

    debug_printf("Segments allocated with %lu bytes", sizeof(StringId) * 4);

    while (p -> cursor < p -> token_count) {
        Token segment = parser_peek(p);

        if (segment.kind != TOK_IDENT) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &segment,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier",
                "add a valid identifier here"
            );

            return parser_error_decl(p, id);
        }

        if (segment_count >= U16_MAX) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &segment,
                DIAG_LOC_END_OF_TOK,
                "max namespacing depth achieved (65535)",
                "shorten the namespacing like bro... max of 65535 segments is supported"
            );

            return parser_error_decl(p, id);
        }

        if (UNLIKELY(segment_count >= segment_capacity)) {
            u64 old_size = segment_count * sizeof(StringId);
            u64 new_size = old_size * 2;

            segments = arena_realloc(&p -> gpa, segments, old_size, new_size);
            segment_capacity *= 2;
        }

        segments[segment_count++] = string_intern_token(p -> current_file -> id, segment);
        
        parser_advance(p);

        if (parser_check(p, TOK_SEMI)) break;

        if (!parser_check(p, TOK_DOT) && !parser_check(p, TOK_SEMI)) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &segment,
                DIAG_LOC_END_OF_TOK,
                "expected '.' or ';'",
                "add a '.' or ';' here"
            );

            return parser_error_decl(p, id);
        }

        parser_advance(p);
    }

    parser_advance(p);

    NamespaceId ns_id = namespace_intern(segments, segment_count);

    AstNode* node = ast_get_node(&p -> current_file -> ast, id);

    node -> as.module_decl.id = ns_id;
    node -> tokens.end = p -> cursor;

    // TODO: ponder modules
    // ModuleId module_id = module_intern(ns_id);
    // Module* module = MODULE_ID_LOOKUP_REF(module_id);
    //
    // module_file_append(module, p -> id);
    //
    // p -> module = module;
    //
    // NamespaceEntry* entry = NAMESPACE_ID_LOOKUP_REF(ns_id);
    // entry -> defined = true;
    //
    // debug_printf(
    //     "Parsing file %d into module namespace %d module ptr %p\n",
    //     p->id,
    //     p->module->namespace_id,
    //     p->module
    // );

    return id;
}
