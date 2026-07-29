#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "diagnostics/diagnostics.h"
#include "modules/modules.h"
#include "namespacing/namespacing.h"
#include "string_interner/interner.h"
#include "utils/debug.h"

AstNodeId parse_module_decl(Parser* p) {
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

            // return parser_error_decl(p, node);
            return AST_NODE_ID_NONE;
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

            // return parser_error_decl(p, node);
            return AST_NODE_ID_NONE;
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

            // return parser_error_decl(p, node);
            return AST_NODE_ID_NONE;
        }

        parser_advance(p);
    }

    parser_advance(p);

    NamespaceId ns_id = namespace_intern(segments, segment_count);
    ModuleId module_id = module_intern(ns_id);
    Module* module = MODULE_ID_LOOKUP_REF(module_id);

    module_file_append(module, p -> id);

    p -> module = module;

    AstNodeId id  = parser_create_node(p, AST_MODULE);
    AstNode* node = ast_node_get(&p -> module -> ast, id);
    node -> as.module_decl.namespace_id = ns_id;

    NamespaceEntry* entry = NAMESPACE_ID_LOOKUP_REF(ns_id);
    entry -> defined = true;

    debug_printf(
        "Parsing file %d into module namespace %d module ptr %p\n",
        p->id,
        p->module->namespace_id,
        p->module
    );
    
    return id;
}
