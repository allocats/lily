#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void resolve_stmt(Resolver* r, AstNodeId stmt_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);
    Ast* ast = &module -> ast;

    AstNode* stmt = &ast -> nodes[stmt_id];

    switch (stmt->kind) {
        case AST_LET:
            resolve_variable(r, stmt, stmt_id);
            break;

        case AST_CONST:
            resolve_constant(r, stmt, stmt_id);
            break;

        case AST_IF:
            resolve_if(r, stmt);
            break;

        case AST_WHILE:
            resolve_while(r, stmt);
            break;

        case AST_FOR:
            resolve_for(r, stmt);
            break;

        case AST_BLOCK:
            resolve_block(r, stmt_id);
            break;

        case AST_RETURN:
            resolve_expr(r, stmt->as.return_stmt.stmt);
            break;

        default:
            resolve_expr(r, stmt_id);
            break;
    }
}
