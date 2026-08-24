#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/decl/decl.h"
#include "ast/parser/directive/directive.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types.h"
#include "ast/tree/tree.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "token/types.h"
#include "utils/debug.h"
#include "utils/types.h"

#include <assert.h>

extern DriverCtx driver;

void parse_file(FileId id) {
    assert(id != FILE_ID_NONE);
    assert(id < driver.file_interner.count);

    File* file = file_lookup_id(id);

    if (file -> stage == FILE_PARSED) {
        return;
    }

    file -> stage = FILE_PARSING;

    Parser p = {
        .current_file = file,
        .tokens_array = &file -> tokens,
        .token_count = file -> tokens.count,
        .cursor = 0,
        .parsing_type = false
    };

    while (p.cursor < p.token_count) {
        Token token = parser_peek(&p);

        if (token.kind == TOK_EOF) {
            break;
        } else if (token.kind == TOK_HASHTAG) {
            parser_advance(&p);
            parse_directive(&p); 
            
            // can realloc as `#paste` calls file_intern() and lex_and_parse()
            p.current_file = file_lookup_id(id);
        } else if (token.kind == TOK_IDENT) {
            Token op = parser_peek_ahead_by(&p, 1);

            AstNodeId node_id = AST_NODE_ID_NONE;

            switch (op.kind) {
                case TOK_COLON:
                    node_id = parse_variable_decl(&p);
                    break;

                case TOK_COLON_COLON:
                    node_id = parse_top_level_decl(&p);
                    break;

                default:    
                    diagnostic_add_token(
                        p.current_file -> id,
                        DIAG_ERROR,
                        &op,
                        DIAG_LOC_WHOLE_TOK,
                        "invalid top level declaration",
                        "expected (':' | '::') after identifier"
                    );

                    node_id = parser_create_node(&p, AST_ERROR, AST_FLAGS_NONE, 0);
                    break;
            }

            if (IS_NODE_ERROR((&p), node_id)){
                parser_error(&p, node_id, RECOVERY_DECL);
            }

        } else {
            diagnostic_add_token(
                p.current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "unexpected top level token",
                "expected (#directve | identifier)"
            );

            AstNodeId node_id = parser_create_node(&p, AST_ERROR, AST_FLAGS_NONE, 0);
            parser_error(&p, node_id, RECOVERY_DECL);
        }
    }

    p.current_file -> stage = FILE_PARSED;
}

// going to keep this AstNodeId for dangling lifetime issues, 
// always get id then get the node pointer
AstNodeId parser_create_node(Parser* p, AstNodeKind kind, u16 flags, u32 start_offset) {
    AstNodeId id = ast_alloc_node(&p -> current_file -> ast); 
    AstNode* node = ast_get_node(&p -> current_file -> ast, id);

    node -> id = id;
    node -> kind = kind;
    node -> flags = flags;
    node -> tokens.start = p -> cursor + start_offset;
    node -> tokens.end = node -> tokens.start;

    Arena* gpa = &p -> current_file -> ast.gpa;

    switch (kind) {
        case AST_FUNCTION_DECL:
            ast_id_list_init(gpa, &node -> as.function_decl.parameters, 4);
            break;

        case AST_FUNCTION_CALL:
            ast_id_list_init(gpa, &node -> as.function_call.arguments, 4);
            break;

        case AST_MACRO_DECL:
            ast_id_list_init(gpa, &node -> as.macro_decl.parameters, 4);
            break;

        case AST_MACRO_CALL:
            ast_id_list_init(gpa, &node -> as.macro_call.arguments, 4);
            break;

        case AST_STRUCT_DECL:
            ast_id_list_init(gpa, &node -> as.struct_decl.fields, 8);
            break;

        case AST_UNION_DECL:
            ast_id_list_init(gpa, &node -> as.union_decl.fields, 8);
            break;

        case AST_ENUM_DECL:
            ast_id_list_init(gpa, &node -> as.enum_decl.variants, 8);
            break;

        case AST_BLOCK:
            ast_id_list_init(gpa, &node -> as.block.statements, 16);
            break;

        case AST_SWITCH_CASE:
            ast_id_list_init(gpa, &node -> as.switch_case.patterns, 1);
            break;

        case AST_SWITCH_STMT:
            ast_id_list_init(gpa, &node -> as.switch_stmt.cases, 4);
            break;

        case AST_IF_STMT:
            ast_id_list_init(gpa, &node -> as.if_stmt.branches, 2);
            break;

        case AST_STRUCT_LITERAL:
            ast_id_list_init(gpa, &node -> as.struct_literal.inits, 4);
            break;

        case AST_TYPE_FUNCTION:
            ast_id_list_init(gpa, &node -> as.type_function.parameters, 4);
            break;

        default:
            break;
    }

    return id;
}

inline AstNode* parser_get_node(Parser* p, AstNodeId id) {
    return ast_get_node(&p -> current_file -> ast, id);
}

inline u64 parser_current_index(Parser* p) {
    return p -> cursor;
}

inline Token parser_peek(Parser* p) {
    debug_assert(p -> cursor < p -> token_count);
    return p -> tokens_array -> items[p -> cursor];
}

inline Token parser_peek_previous(Parser* p) {
    debug_assert(p -> cursor - 1 > 0);
    return p -> tokens_array -> items[p -> cursor - 1];
}

inline Token parser_peek_ahead_by(Parser* p, u32 count) {
    debug_assert(p -> cursor + count < p -> token_count);
    return p -> tokens_array -> items[p -> cursor + count];
}

inline Token parser_advance(Parser* p) {
    debug_assert(p -> cursor < p -> token_count);
    return p -> tokens_array -> items[p -> cursor++];
}

inline bool parser_check(Parser* p, TokenKind kind) {
    debug_assert(p -> cursor < p -> token_count);
    return p -> tokens_array -> items[p -> cursor].kind == kind;
}

inline bool parser_check_ahead_by(Parser* p, TokenKind kind, u32 count) {
    debug_assert(p -> cursor + count < p -> token_count);
    return p -> tokens_array -> items[p -> cursor + count].kind == kind;
}
