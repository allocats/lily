#include "ast/nodes/types.h"
#include "ast/parser/directive/directive.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>

static constexpr u32 directive_count = 3;

static AstNodeKind directive_lut[directive_count];

// see note in header file
void directive_ids_init() {
    assert(string_intern_cstr("import") == 0);
    
    directive_lut[string_intern_cstr("import")] = AST_IMPORT_DIRECTIVE;
    directive_lut[string_intern_cstr("paste")] = AST_PASTE_DIRECTIVE;
    directive_lut[string_intern_cstr("execute")] = AST_EXECUTE_DIRECTIVE;
}

AstNodeId parse_directive(Parser* p) {
    AstNodeId id = parser_create_node(p, AST_IMPORT_DIRECTIVE, AST_FLAGS_IS_TOP_DECL, -1);
    AstNode* node = parser_get_node(p, id);

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

        return parser_error(p, id, RECOVERY_NONE);
    }

    StringId string_id = string_intern_token(p -> current_file -> id, directive);    

    if (string_id >= directive_count) {
        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &directive,
            DIAG_LOC_WHOLE_TOK,
            "unknown directive",
            "add a valid directive here"            
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    AstNodeKind kind = directive_lut[string_id];

    node -> kind = kind;

    switch (kind) {
        case AST_IMPORT_DIRECTIVE: {
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

                return parser_error(p, id, RECOVERY_DECL);
            }

            node -> as.import_directive.path = string_intern_token(p -> current_file -> id, path);
            node -> as.import_directive.resolved = MODULE_ID_NONE;
            break;
        }

        case AST_PASTE_DIRECTIVE: {
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

                return parser_error(p, id, RECOVERY_DECL);
            }

            node -> as.paste_directive.path = string_intern_token(p -> current_file -> id, path);
            break;
        }

        case AST_EXECUTE_DIRECTIVE:
            AstNodeId target_id = parse_expression(p, 0);

            if (IS_NODE_ERROR(p, target_id)) {
                return parser_error(p, id, RECOVERY_NONE);
            }

            node -> as.execute_directive.expr = target_id;
            break;

        default:
            UNREACHABLE("Hit default case in directive kind switch statement");
    }

    if (!parser_check(p, TOK_SEMI)) {
        Token previous = parser_peek_previous(p);

        diagnostic_add_token(
            p -> current_file -> id,
            DIAG_ERROR,
            &previous,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"            
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    node -> tokens.end = p -> cursor;

    parser_advance(p);

    return id;
}
