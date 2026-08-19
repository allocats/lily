#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/directive/directive.h"
#include "ast/tree/tree.h"
#include "ast/parser/parser.h"
#include "ast/parser/decl/decl.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "files/files.h"
#include "ids.h"
#include "token/types.h"

#include <assert.h>

void parse_file(FileId id) {
    assert(id >= 0);
    assert(id < FILE_ID_NONE);

    File* file = file_lookup_id(id);

    // should not be possible to be here with any other stage
    assert(file -> stage == FILE_LEXED);

    Parser p = {
        .current_file = file,
        .tokens_array = &file -> tokens,
        .token_count = file -> tokens.count,
        .cursor = 0
    };

    while (p.cursor < p.token_count) {
        Token token = parser_peek(&p);

        if (token.kind == TOK_EOF) {
            break;
        } else if (token.kind == TOK_HASHTAG) {
            parse_directive(&p); 
        } else if (token.kind == TOK_IDENT) {
            // parse_expression();
        } else {
            diagnostic_add_token(
                p.current_file -> id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "unexpected top level token",
                "expected (#directve | identifier)"
            );

            parser_recover_decl(&p);
        }
    }
}

// going to keep this AstNodeId for dangling lifetime issues, 
// always get id then get the node pointer
AstNodeId parser_create_node(
    Parser* p,
    AstNodeKind kind,
    u16 flags,
    u32 start_offset
) {
    AstNodeId id = ast_alloc_node(&p -> current_file -> ast); 
    AstNode* node = ast_get_node(&p -> current_file -> ast, id);

    node -> id = id;
    node -> kind = kind;
    node -> flags = flags;
    node -> tokens.start = p -> cursor + start_offset;

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

        default:
            break;
    }

    return id;
}

inline u64 parser_current_index(Parser* p) {
    return p -> cursor;
}

inline Token parser_peek(Parser* p) {
    return p -> tokens_array -> items[p -> cursor];
}

inline Token parser_advance(Parser* p) {
    return p -> tokens_array -> items[p -> cursor++];
}

inline bool parser_check(Parser* p, TokenKind kind) {
    return p -> tokens_array -> items[p -> cursor].kind == kind;
}
