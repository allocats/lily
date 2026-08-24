#include "ast/nodes/types.h"
#include "ast/parser/directive/directive.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/driver.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>

extern DriverCtx driver;

static constexpr u32 directive_count = 3;

static AstNodeKind directive_lut[directive_count];

// see note in header file
void directive_ids_init() {
    assert(string_intern_cstr("import") == 0);
    
    directive_lut[string_intern_cstr("import")]  = AST_IMPORT_DIRECTIVE;
    directive_lut[string_intern_cstr("include")] = AST_INCLUDE_DIRECTIVE;
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

        case AST_INCLUDE_DIRECTIVE: {
            Token path_token = parser_advance(p);

            if (path_token.kind != TOK_STRING_LIT) {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &path_token,
                    DIAG_LOC_WHOLE_TOK,
                    "expected string literal",
                    "add a valid string literal here"            
                );

                return parser_error(p, id, RECOVERY_DECL);
            }

            path_token.start  += 1;
            path_token.length -= 2;

            StringId path_token_id = string_intern_token(p -> current_file -> id, path_token);

            path_token.start  -= 1;
            path_token.length += 2;

            node -> as.include_directive.path = path_token_id;

            str8 import_path_string  = STRING_ID_LOOKUP(path_token_id).str;
            str8 current_path_string = p -> current_file -> path;

            char* last_slash = strrchr(current_path_string.ptr, '/');

            char buffer[PATH_MAX] = {0};
            i32 n = 0;

            if (!last_slash) {
                n = snprintf(
                    buffer,
                    sizeof(buffer),
                    "%.*s/%.*s",
                    current_path_string.len,
                    current_path_string.ptr,
                    import_path_string.len,
                    import_path_string.ptr
                );
            } else {
                n = snprintf(
                    buffer,
                    sizeof(buffer),
                    "%.*s/%.*s",
                    (i32) (last_slash - current_path_string.ptr),
                    current_path_string.ptr,
                    import_path_string.len,
                    import_path_string.ptr
                );
            }

            str8 final_input_path = {
                .ptr = buffer,
                .len = n
            };

            FileId included_file_id = file_intern(final_input_path);
            
            if (UNLIKELY(included_file_id == FILE_ID_NONE)) {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &path_token,
                    DIAG_LOC_WHOLE_TOK,
                    "included file does not exist",
                    "add a valid path to the file you are trying to include"
                );

                node -> kind = AST_ERROR;
                break;
            }

            File* included_file = file_lookup_id(included_file_id);

            node = parser_get_node(p, id);

            // means it has not yet been imported yet
            if (included_file -> stage == FILE_ALLOCATED) {
                lex_and_parse(included_file_id);
            } else if (included_file -> stage == FILE_PARSING) {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &path_token,
                    DIAG_LOC_WHOLE_TOK,
                    "circular include detected",
                    "remove one and find a workaround"
                );

                node -> kind = AST_ERROR;
            }

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

    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor;

    parser_advance(p);

    return id;
}
