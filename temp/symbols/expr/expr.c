#include "ast/nodes/types.h"
#include "modules/modules.h"
#include "modules/types.h"
#include "namespacing/types.h"
#include "symbols/symbols.h"
#include "symbols/types.h"

#include <stdio.h>

void resolve_identifier(Resolver* r, AstNode* ident) {
    SymbolId sym = SYMBOL_ID_NONE;

    if (ident -> as.ident.namespace_id == NAMESPACE_ID_NONE) {
        sym = table_get_sym(r, ident -> as.ident.name_id);
    } else {
        ModuleId module_id = module_lookup(ident -> as.ident.namespace_id);

        if (module_id == MODULE_ID_NONE) {
            printf("1");
            exit(1);
        }

        Module* module = MODULE_ID_LOOKUP_REF(module_id);

        SymbolTable* table = r -> table;

        r -> table = &module -> symbol_table;

        sym = table_get_sym(r, ident -> as.ident.name_id);

        if (sym == SYMBOL_ID_NONE) {
            printf("2");
            exit(2);
        }

        r -> table = table;
    }

    ident -> as.ident.symbol_id = sym;
}

void resolve_expr(Resolver* r, AstNodeId expr_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);
    Ast* ast = &module -> ast;

    AstNode* expr = &ast -> nodes[expr_id];

    switch (expr -> kind) {
        case AST_IDENT:
            resolve_identifier(r, expr);
            break;

        case AST_LITERAL:
            break;

        case AST_BINOP:
            resolve_expr(r, expr -> as.binary_op.left);
            resolve_expr(r, expr -> as.binary_op.right);
            break;

        case AST_UNARY:
            resolve_expr(r, expr -> as.unary_op.operand);
            break;

        case AST_ASSIGN:
            resolve_expr(r, expr -> as.assign.target);
            resolve_expr(r, expr -> as.assign.value_expr);
            break;

        case AST_FUNC_CALL:
            resolve_expr(r, expr -> as.func_call.ident);

            for (u32 i = 0; i < expr -> as.func_call.arg_count; i++) {
                resolve_expr(r, expr -> as.func_call.args[i]);
            }

            break;

        case AST_INDEX:
            resolve_expr(r, expr -> as.index.ident);
            resolve_expr(r, expr -> as.index.index);
            break;

        case AST_MEMBER_ACCESS:
            resolve_expr(r, expr -> as.member_access.ident);
            break;

        default:
            break;
    }
}
