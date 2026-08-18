#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/tree/tree.h"
#include "parser/parser.h"
#include "files/files.h"
#include "ids.h"
#include "parser/types.h"

#include <assert.h>

void parse_file(FileId id) {
    assert(id > 0);
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

    }
}

AstNode* parser_create_node(Parser* p, AstNodeKind kind, u16 flags) {
    AstNodeId id = ast_alloc_node(&p -> current_file -> ast); 
    AstNode* node = ast_get_node(&p -> current_file -> ast, id);

    node -> id = id;
    node -> kind = kind;
    node -> flags = flags;

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

    return node;
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
