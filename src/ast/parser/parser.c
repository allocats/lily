#include "ast/nodes/nodes.h"
#include "ast/parser/parser.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "files/types.h"
#include "utils/debug.h"

#include <assert.h>

typedef AstNodeId (*ParseFn)(Parser*);
static  ParseFn  TOP_LEVEL_PARSE_FUNCTIONS[TOKEN_KIND_COUNT] = {
    [TOK_IMPORT]   = parse_import_decl,
    [TOK_EXTERNAL] = parse_external_decl,
    [TOK_STRUCT]   = parse_struct_decl,
    [TOK_UNION]    = parse_union_decl,
    [TOK_MACRO]    = parse_macro_decl,
    [TOK_ENUM]     = parse_enum_decl,
    [TOK_FN]       = parse_function_decl,
    [TOK_CONST]    = parse_const_decl
};

void parser_parse_file(FileId id) {
    File* file = &driver_ctx.file_registry.entries[id];
    if (file -> stage == FILE_ERROR) return;

    debug_assert(file -> stage == FILE_LEXED);

    file -> stage = FILE_PARSING;

    TokenArray* tokens = &driver_ctx.file_registry.tokens[id];
    if (tokens -> count == 0) return;

    Parser p = {
        .id = id,
        .module = null,
        .cursor = 0,
        .tokens = tokens,
        .token_count = tokens -> count
    };

    Token* expected_module_token = parser_advance(&p);

    if (expected_module_token -> kind != TOK_MODULE) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            id,
            DIAG_ERROR,
            expected_module_token,
            DIAG_LOC_WHOLE_TOK,
            "expected module declaration",
            "module declaration must appear first in a file"
        );

        file -> stage = FILE_ERROR;
        return;
    }

    if (parse_module_decl(&p) == AST_NODE_ID_NONE) {
        diagnostic_add_token(
            &driver_ctx.diagnostics,
            id,
            DIAG_ERROR,
            expected_module_token,
            DIAG_LOC_WHOLE_TOK,
            "invalid module declaration",
            null
        );

        file -> stage = FILE_ERROR;
        return;
    }

    while (p.cursor < p.token_count) {
        Token* token = parser_advance(&p);
        if (token -> kind == TOK_EOF) break;

        ParseFn fn = TOP_LEVEL_PARSE_FUNCTIONS[token -> kind];

        if (fn) {
            fn(&p);
        } else {
            diagnostic_add_token(
                &driver_ctx.diagnostics,
                p.id,
                DIAG_ERROR,
                token,
                DIAG_LOC_WHOLE_TOK,
                "unexpected token",
                "expected (import | struct | union | enum | fn | macro | const)"
            );

            parser_recover_decl(&p);
        }
    }

    file -> stage = FILE_PARSED;
}


AstNodeId parser_create_node(Parser* p, AstKind kind) {
    AstNodeId id = ast_node_alloc(
        &p -> module -> ast.arena,
        &p -> module -> ast
    );

    AstNode* node = ast_node_get(
        &p -> module -> ast,
        id
    );

    node -> id = id;
    node -> kind = kind;
    node -> source_token = parser_peek(p);

    return id;
}

inline Token* parser_peek(Parser* p) {
    debug_assert(p -> cursor >= 0 && "parser underflow");
    debug_assert(p -> cursor <  p -> token_count && "parser overflow");

    return &p -> tokens -> items[p -> cursor];
}

inline Token* parser_peek_previous(Parser* p) {
    debug_assert(p -> cursor > 0 && "parser underflow");
    debug_assert(p -> cursor <=  p -> token_count && "parser overflow");

    return &p -> tokens -> items[p -> cursor - 1];
}

inline Token* parser_advance(Parser* p) {
    debug_assert(p -> cursor <  p -> token_count && "parser overflow");
    return &p -> tokens -> items[p -> cursor++];
}
 
inline bool parser_check(Parser* p, TokenKind kind) {
    return parser_peek(p) -> kind == kind;
}
