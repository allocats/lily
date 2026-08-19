#include "ast/parser/decl/decl.h"
#include "ast/parser/directive/directive.h"
#include "ast/parser/parser.h"
#include "ast/tree/tree.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>

static constexpr u32 directive_count = 2;

static AstNodeKind directive_lut[directive_count];

// see note in header file
void directive_ids_init() {
    assert(string_intern_cstr("import") == 0);
    
    directive_lut[string_intern_cstr("import")] = AST_IMPORT_DIRECTIVE;
    directive_lut[string_intern_cstr("paste")] = AST_PASTE_DIRECTIVE;
}

AstNodeId parse_directive(Parser* p) {
    parser_advance(p); // advance past '#'

    AstNodeId id = parser_create_node(p, AST_IMPORT_DIRECTIVE, AST_FLAGS_IS_TOP_DECL, -1);
    AstNode* node = ast_get_node(&p -> current_file -> ast, id);

    Token directive = parser_advance(p); 

    if (directive.kind != TOK_IDENT) {
        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &directive,
            DIAG_LOC_WHOLE_TOK,
            "expected directive identifier",
            "add a valid identifier here"            
        );

        return parser_error_decl(p, id);
    }

    StringId string_id = string_intern_token(p -> current_file -> id, directive);    

    if (string_id > directive_count) {
        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &directive,
            DIAG_LOC_WHOLE_TOK,
            "unknown directive",
            "add a valid directive here"            
        );

        return parser_error_decl(p, id);
    }

    Token path = parser_advance(p);

    if (path.kind != TOK_STRING_LIT) {
        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &path,
            DIAG_LOC_WHOLE_TOK,
            "expected string literal",
            "add a valid string literal here"            
        );

        return parser_error_decl(p, id);
    }

    AstNodeKind kind = directive_lut[string_id];

    node -> kind = kind;

    switch (kind) {
        case AST_IMPORT_DIRECTIVE:
            node -> as.import_directive.path = string_intern_token(p -> current_file -> id, path);
            node -> as.import_directive.resolved = MODULE_ID_NONE;
            break;

        case AST_PASTE_DIRECTIVE:
            node -> as.paste_directive.path = string_intern_token(p -> current_file -> id, path);
            break;

        default:
            UNREACHABLE("Hit default case in directive kind switch statement");
    }

    if (!parser_check(p, TOK_SEMI)) {
        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &path,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"            
        );

        return parser_error_decl(p, id);
    }

    node -> tokens.end = p -> cursor;

    parser_advance(p);

    return id;
}
