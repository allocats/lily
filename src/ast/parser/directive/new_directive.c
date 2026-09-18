#include "ast/nodes/types.h"
#include "ast/parser/directive/directive.h"
#include "ast/parser/directive/types.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
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

static constexpr u32 directive_count = 9;

static DirectiveInfo directive_lut[directive_count];

static char scratch_relative[PATH_MAX];
static char scratch_stdlib[PATH_MAX];

static AstNodeId parse_directive_statement(Parser* p, StringId binding_name_id, u32 flags, DirectiveInfo directive, i32 start);

// see note in header file
void directive_infos_init() {
    assert(string_intern_cstr("import") == 0);
    
    directive_lut[string_intern_cstr("import")]  = (DirectiveInfo) {
        .kind = DIRECTIVE_STATEMENT,
        .as.kind = AST_IMPORT_DIRECTIVE,
    };

    directive_lut[string_intern_cstr("include")] = (DirectiveInfo) {
        .kind = DIRECTIVE_STATEMENT,
        .as.kind = AST_INCLUDE_DIRECTIVE,
    };

    directive_lut[string_intern_cstr("execute")] = (DirectiveInfo) {
        .kind = DIRECTIVE_STATEMENT,
        .as.kind = AST_EXECUTE_DIRECTIVE,
    };

    directive_lut[string_intern_cstr("foreign")] = (DirectiveInfo) {
        .kind = DIRECTIVE_ATTRIBUTE_FLAG,
        .as.flag = AST_FLAGS_IS_FOREIGN
    };

    directive_lut[string_intern_cstr("intrinsic")] = (DirectiveInfo) {
        .kind = DIRECTIVE_ATTRIBUTE_FLAG,
        .as.flag = AST_FLAGS_IS_INTRINSIC
    };

    directive_lut[string_intern_cstr("inline")] = (DirectiveInfo) {
        .kind = DIRECTIVE_ATTRIBUTE_FLAG,
        .as.flag = AST_FLAGS_IS_INLINE
    };

    directive_lut[string_intern_cstr("noinline")] = (DirectiveInfo) {
        .kind = DIRECTIVE_ATTRIBUTE_FLAG,
        .as.flag = AST_FLAGS_IS_NOINLINE
    };

    directive_lut[string_intern_cstr("align")] = (DirectiveInfo) {
        .kind = DIRECTIVE_ATTRIBUTE_VALUE,
    };

    directive_lut[string_intern_cstr("deprecated")] = (DirectiveInfo) {
        .kind = DIRECTIVE_ATTRIBUTE_VALUE,
    };

    assert(string_intern_cstr("deprecated") == directive_count - 1);
}

AstNodeId parse_directives(Parser* p, StringId name_id) {
    u32 start_index = p -> cursor - 3;
    u32 flags = AST_FLAGS_NONE;

    for (;;) {
        Token directive_tok = parser_advance(p); 

        if (directive_tok.kind != TOK_IDENT) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &directive_tok,
                DIAG_LOC_WHOLE_TOK,
                "expected identifier for directive",
                "add a valid identifier here"            
            );

            AstNodeId id = parser_create_node(
                p,
                AST_ERROR,
                AST_FLAGS_NONE,
                start_index - p -> cursor
            );
            return parser_error(p, id, RECOVERY_NONE);
        }

        // l;/,l. <- my dog typed this! need to preserve this, good boy Ollie <3

        StringId string_id = string_intern_token(p -> current_file -> id, directive_tok);

        if (string_id >= directive_count) {
            diagnostic_add_token(
                p -> current_file -> id,
                DIAG_ERROR,
                &directive_tok,
                DIAG_LOC_WHOLE_TOK,
                "unknown directive",
                "add a valid directive here"            
            );

            AstNodeId id = parser_create_node(
                p,
                AST_ERROR,
                AST_FLAGS_NONE,
                start_index - p -> cursor
            );
            return parser_error(p, id, RECOVERY_NONE);
        }

        DirectiveInfo directive = directive_lut[string_id];

        switch (directive.kind) {
            case DIRECTIVE_STATEMENT: {
                return parse_directive_statement(p, name_id, flags, directive, start_index);
            } break;

            case DIRECTIVE_ATTRIBUTE_FLAG: {
                if (flags & directive.as.flag) {
                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &directive_tok,
                        DIAG_LOC_WHOLE_TOK,
                        "duplicate attribute",
                        "remove this duplicated attribute"
                    );

                    AstNodeId id = parser_create_node(
                        p,
                        AST_ERROR,
                        AST_FLAGS_NONE,
                        start_index - p -> cursor
                    );
                    return parser_error(p, id, RECOVERY_NONE);
                }

                flags |= directive.as.flag;

                if (parser_check(p, TOK_HASHTAG)) {
                    parser_advance(p);
                }
            } break;

            default:
                UNREACHABLE("parse_directives()");
        }
    }
}

static AstNodeId parse_directive_statement(Parser* p, StringId binding_name_id, u32 flags, DirectiveInfo directive, i32 start) {
    AstNodeId id  = parser_create_node(p, directive.as.kind, flags, start - p -> cursor);
    AstNode* node = parser_get_node(p, id);
    
    switch (directive.as.kind) {
        case AST_IMPORT_DIRECTIVE: {
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

            if (path_token.length <= 0) {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &path_token,
                    DIAG_LOC_WHOLE_TOK,
                    "invalid import path",
                    "add a valid import path here"
                );

                return parser_error(p, id, RECOVERY_DECL);
            }

            StringId path_string_id = string_intern_token(p -> current_file -> id, path_token);

            path_token.start  -= 1;
            path_token.length += 2;

            node -> as.import_directive.path    = path_string_id;
            node -> as.import_directive.binding = binding_name_id;
            node -> as.import_directive.file_id = FILE_ID_NONE;

            str8 import_path_string = STRING_ID_LOOKUP(path_string_id).str;

            i32 n_relative = snprintf(
                scratch_relative,
                sizeof(scratch_relative),
                "%.*s/module.lily",
                import_path_string.len,
                import_path_string.ptr
            );

            str8 import_module_path_relative = {
                .ptr = scratch_relative,
                .len = n_relative
            };

            i32 n_stdlib = snprintf(
                scratch_stdlib,
                sizeof(scratch_stdlib),
                "%s/%.*s/module.lily",
                driver.stdlib_path,
                import_path_string.len,
                import_path_string.ptr
            );

            str8 import_module_path_stdlib = {
                .ptr = scratch_stdlib,
                .len = n_stdlib
            };

            FileId imported_file_id = file_intern(import_module_path_relative);

            if (imported_file_id == FILE_ID_NONE) {
                imported_file_id = file_intern(import_module_path_stdlib);

                if (imported_file_id == FILE_ID_NONE) {
                    diagnostic_add_token(
                        p -> current_file -> id,
                        DIAG_ERROR,
                        &path_token,
                        DIAG_LOC_WHOLE_TOK,
                        "imported file does not exist",
                        "add a valid path to the file you are trying to import"
                    );

                    node -> kind = AST_ERROR;
                    break;
                }
            }

            File* imported_file = file_lookup_id(imported_file_id);

            node = parser_get_node(p, id);
            node -> as.import_directive.file_id = imported_file_id;

            // means it has not yet been imported yet
            if (imported_file -> stage == FILE_ALLOCATED) {
                lex_and_parse(imported_file_id);
            } 

            break;
        }

        case AST_INCLUDE_DIRECTIVE: {
            if (binding_name_id != STRING_ID_NONE) {
                Token token = parser_peek_behind_by(p, 2);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "cannot bind an include",
                    "remove this binding"
                );
            }

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

            if (path_token.length <= 0) {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &path_token,
                    DIAG_LOC_WHOLE_TOK,
                    "invalid import path",
                    "add a valid import path here"
                );

                return parser_error(p, id, RECOVERY_DECL);
            }

            StringId path_token_id = string_intern_token(p -> current_file -> id, path_token);

            path_token.start  -= 1;
            path_token.length += 2;

            node -> as.include_directive.path = path_token_id;

            str8 import_path_string  = STRING_ID_LOOKUP(path_token_id).str;
            str8 current_path_string = p -> current_file -> path;

            char* last_slash = strrchr(current_path_string.ptr, '/');

            i32 n = 0;

            if (!last_slash) {
                n = snprintf(
                    scratch_relative,
                    sizeof(scratch_relative),
                    "%.*s",
                    // current_path_string.len,
                    // current_path_string.ptr,
                    import_path_string.len,
                    import_path_string.ptr
                );
            } else {
                n = snprintf(
                    scratch_relative,
                    sizeof(scratch_relative),
                    "%.*s/%.*s",
                    (i32) (last_slash - current_path_string.ptr),
                    current_path_string.ptr,
                    import_path_string.len,
                    import_path_string.ptr
                );
            }

            str8 final_input_path = {
                .ptr = scratch_relative,
                .len = n
            };

            FileId included_file_id = file_intern(final_input_path);

            node = parser_get_node(p, id);
            node -> as.include_directive.file_id = included_file_id;

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

            // means it has not yet been imported yet
            if (included_file -> stage == FILE_ALLOCATED) {
                lex_and_parse(included_file_id);
            } else if (included_file -> stage == FILE_LEXED) {
                parse_file(included_file_id);
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
            } else if (included_file -> stage == FILE_ERROR) {
                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &path_token,
                    DIAG_LOC_WHOLE_TOK,
                    "cannot include file",
                    null
                );

                node -> kind = AST_ERROR;
            }

            break;
        }

        case AST_EXECUTE_DIRECTIVE: {
            if (binding_name_id != STRING_ID_NONE) {
                Token token = parser_peek_behind_by(p, 2);

                diagnostic_add_token(
                    p -> current_file -> id,
                    DIAG_ERROR,
                    &token,
                    DIAG_LOC_WHOLE_TOK,
                    "cannot bind an execution",
                    "remove this binding"
                );
            }

            AstNodeId target_id = parse_expression(p, 0);

            if (IS_NODE_ERROR(p, target_id)) {
                return parser_error(p, id, RECOVERY_NONE);
            }

            node -> as.execute_directive.expr = target_id;
        } break;

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
