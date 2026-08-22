#include "ast/nodes/nodes.h"
#include "ast/nodes/types.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/stmts/stmts.h"
#include "ast/parser/types.h"
#include "ids.h"
#include "token/types.h"
#include <assert.h>

AstNodeId parse_block(Parser* p) {
    AstNodeId id  = parser_create_node(p, AST_BLOCK, AST_FLAGS_NONE, 0);
    AstNode* node = parser_get_node(p, id); 

    parser_advance(p); // advance past '{'

    while (p -> cursor < p -> token_count) {
        if (parser_check(p, TOK_R_BRACE)) {
            break;
        }

        AstNodeId stmt_id = parse_statement(p);

        node = parser_get_node(p, id);

        ast_id_list_append(&node -> as.block.statements, &p -> current_file -> ast, stmt_id);

        if (IS_NODE_ERROR(p, stmt_id)) {
            parser_recover(p, RECOVERY_STMT); 

            if (parser_check(p, TOK_SEMI)) {
                parser_advance(p);
            }
        }
    }

    node = parser_get_node(p, id);
    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past '}'

    return id;
}
