#include "tree.h"

#include "ast/nodes/types.h"
#include "driver/types.h"
#include "namespacing/namespacing.h"
#include "string_interner/interner.h"

#include <stdio.h>

extern LilyCtx driver_ctx;

static void ast_print_node(const Ast* ast, AstNodeId id, u32 indent);

static void ast_print_indent(u32 indent) {
    while (indent--) printf("  ");
}

static void ast_print_str(StringId id) {
    StringEntry str = STRING_ID_LOOKUP(id);

    printf("%.*s hash=0x%x", (int)str.str.length, (const char*)str.str.pointer, str.hash);
}

static void ast_print_str_no_hash(StringId id) {
    StringEntry str = STRING_ID_LOOKUP(id);

    printf("%.*s", (int)str.str.length, (const char*)str.str.pointer);
}

static void ast_print_namespace(NamespaceId id) {
    if (id == NAMESPACE_ID_NONE) return;

    NamespaceEntry entry = NAMESPACE_ID_LOOKUP(id);

    printf("(Namespace=%d Defined=%c) ", id, entry.defined ? 'Y' : 'N');

    for (u32 i = 0; i < entry.count; i++) {
        if (i) printf("::");

        ast_print_str_no_hash(entry.segments[i]);
    }
}

// Prints " [EXTERNAL]", " [VARIADIC]", both, or nothing if flags == AST_FLAGS_NONE.
static void ast_print_flags(u32 flags) {
    if (flags == AST_FLAGS_NONE) return;

    printf(" [");
    bool first = true;

    if (flags & AST_FLAGS_IS_EXTERNAL) {
        printf("EXTERNAL");
        first = false;
    }
    if (flags & AST_FLAGS_IS_VARIADIC) {
        printf("%sVARIADIC", first ? "" : ", ");
    }
    printf("]");
}

// Resolves an AstNodeId to its node. Returns NULL for AST_NODE_ID_NONE.
static const AstNode* ast_get_node(const Ast* ast, AstNodeId id) {
    if (id == AST_NODE_ID_NONE) return NULL;

    // ASSUMPTION: id is a direct index into ast->nodes. Adjust if your
    // real storage/lookup mechanism differs.
    return &ast->nodes[id];
}

void ast_print(Ast* ast) {
    if (!ast) return;

    for (u32 i = 0; i < ast->count; i++) {
        ast_print_node(ast, ast->nodes[i].id, 0);
        putchar('\n');
    }
}

static void ast_print_node(const Ast* ast, AstNodeId id, u32 indent) {
    if (id == AST_NODE_ID_NONE) {
        ast_print_indent(indent);
        printf("<null>\n");
        return;
    }

    const AstNode* node = ast_get_node(ast, id);

    if (!node) {
        ast_print_indent(indent);
        printf("<invalid id=%u>\n", id);
        return;
    }

    ast_print_indent(indent);

    // Every node gets its id printed up front, e.g. "#3 FUNCTION foo"
    printf("#%u ", node->id);

    switch (node->kind)
    {
        case AST_ERROR:
        {
            printf("ERROR");
            ast_print_flags(node->flags);
            printf("\n");
            break;
        }

        case AST_MODULE:
        {
            printf("MODULE ");
            ast_print_namespace(node->as.module_decl.namespace_id);
            ast_print_flags(node->flags);
            printf("\n");
            break;
        }

        case AST_IMPORT:
        {
            printf("IMPORT ");
            ast_print_namespace(node->as.import_decl.namespace_id);
            ast_print_flags(node->flags);
            printf("\n");
            break;
        }

        case AST_FUNCTION:
        {
            const AstFunctionDecl* fn = &node->as.func_decl;

            printf("FUNCTION ");
            ast_print_str(fn->name_id);
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_indent(indent + 1);
            printf("Return: Type=%d\n", fn->return_type_expr);

            ast_print_indent(indent + 1);
            printf("Params:\n");

            for (u32 i = 0; i < fn->param_count; i++)
                ast_print_node(ast, fn->params[i], indent + 2);

            ast_print_indent(indent + 1);
            printf("Body:\n");

            ast_print_node(ast, fn->block, indent + 2);

            break;
        }

        case AST_PARAM:
        {
            const AstParam* p = &node->as.param_decl;

            printf("PARAM ");
            ast_print_str(p->name_id);
            printf(" : Type=%d", p->type_expr);
            ast_print_flags(node->flags);
            printf("\n");

            break;
        }

        case AST_STRUCT:
        {
            const AstStructDecl* s = &node->as.struct_decl;

            printf("STRUCT ");
            ast_print_str(s->name_id);
            ast_print_flags(node->flags);
            printf("\n");

            for (u32 i = 0; i < s->field_count; i++)
                ast_print_node(ast, s->fields[i], indent + 1);

            break;
        }

        case AST_UNION:
        {
            const AstUnionDecl* u = &node->as.union_decl;

            printf("UNION ");
            ast_print_str(u->name_id);
            ast_print_flags(node->flags);
            printf("\n");

            for (u32 i = 0; i < u->field_count; i++)
                ast_print_node(ast, u->fields[i], indent + 1);

            break;
        }

        case AST_FIELD:
        {
            const AstField* f = &node->as.field_decl;

            printf("FIELD ");
            ast_print_str(f->name_id);
            printf(" : Type=%d", f->type_expr);
            ast_print_flags(node->flags);
            printf("\n");

            break;
        }

        case AST_ENUM:
        {
            const AstEnumDecl* e = &node->as.enum_decl;

            printf("ENUM ");
            ast_print_str(e->name_id);
            printf(" (Type=%d)", e->type_expr);
            ast_print_flags(node->flags);
            printf("\n");

            for (u32 i = 0; i < e->variant_count; i++)
                ast_print_node(ast, e->variants[i], indent + 1);

            break;
        }

        case AST_VARIANT:
        {
            const AstVariant* v = &node->as.variant_decl;

            printf("VARIANT ");
            ast_print_str(v->name_id);
            ast_print_flags(node->flags);
            printf("\n");

            if (v->value_expr != AST_NODE_ID_NONE) {
                ast_print_indent(indent + 1);
                printf("Value:\n");
                ast_print_node(ast, v->value_expr, indent + 2);
            }

            break;
        }

        case AST_BLOCK:
        {
            printf("BLOCK");
            ast_print_flags(node->flags);
            printf("\n");

            for (u32 i = 0; i < node->as.block.stmt_count; i++)
                ast_print_node(ast, node->as.block.stmts[i], indent + 1);

            break;
        }

        case AST_DEFER:
        {
            printf("DEFER");
            ast_print_flags(node->flags);
            printf("\n");
            ast_print_node(ast, node->as.defer_stmt.stmt, indent + 1);
            break;
        }

        case AST_RETURN:
        {
            printf("RETURN");
            ast_print_flags(node->flags);
            printf("\n");
            ast_print_node(ast, node->as.return_stmt.stmt, indent + 1);
            break;
        }

        case AST_LET:
        {
            const AstVarDecl* v = &node->as.var_decl;

            printf("LET ");
            ast_print_str(v->name_id);
            printf(" : Type=%d", v->type_expr);
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_node(ast, v->value_expr, indent + 1);

            break;
        }

        case AST_CONST:
        {
            const AstVarDecl* v = &node->as.const_decl;

            printf("CONST ");
            ast_print_str(v->name_id);
            printf(" : Type=%d", v->type_expr);
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_node(ast, v->value_expr, indent + 1);

            break;
        }

        case AST_BRANCH:
        {
            const AstBranch* b = &node->as.branch;

            printf("BRANCH");
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_indent(indent + 1);
            printf("Condition:\n");
            ast_print_node(ast, b->condition, indent + 2);

            ast_print_indent(indent + 1);
            printf("Body:\n");
            ast_print_node(ast, b->block, indent + 2);

            break;
        }

        case AST_IF:
        {
            const AstIf* stmt = &node->as.if_stmt;

            printf("IF");
            ast_print_flags(node->flags);
            printf("\n");

            for (u32 i = 0; i < stmt->branch_count; i++)
            {
                ast_print_indent(indent + 1);
                printf("Branch %u:\n", i);
                ast_print_node(ast, stmt->branches[i], indent + 2);
            }

            if (stmt->else_block != AST_NODE_ID_NONE)
            {
                ast_print_indent(indent + 1);
                printf("Else:\n");
                ast_print_node(ast, stmt->else_block, indent + 2);
            }

            break;
        }

        case AST_FOR:
        {
            const AstFor* loop = &node->as.for_loop;

            printf("FOR");
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_indent(indent + 1);
            printf("Init:\n");
            ast_print_node(ast, loop->init, indent + 2);

            ast_print_indent(indent + 1);
            printf("Condition:\n");
            ast_print_node(ast, loop->cond, indent + 2);

            ast_print_indent(indent + 1);
            printf("Step:\n");
            ast_print_node(ast, loop->step, indent + 2);

            ast_print_indent(indent + 1);
            printf("Body:\n");
            ast_print_node(ast, loop->block, indent + 2);

            break;
        }

        case AST_WHILE:
        {
            const AstWhile* loop = &node->as.while_loop;

            printf("WHILE");
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_indent(indent + 1);
            printf("Condition:\n");
            ast_print_node(ast, loop->condition, indent + 2);

            ast_print_indent(indent + 1);
            printf("Body:\n");
            ast_print_node(ast, loop->block, indent + 2);

            break;
        }

        case AST_BINOP:
        {
            printf("BINOP '%s'", TOKEN_KIND_STRS[node->as.binary_op.op]);
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_node(ast, node->as.binary_op.left, indent + 1);
            ast_print_node(ast, node->as.binary_op.right, indent + 1);

            break;
        }

        case AST_UNARY:
        {
            printf("UNARY '%s'", TOKEN_KIND_STRS[node->as.unary_op.op]);
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_node(ast, node->as.unary_op.operand, indent + 1);

            break;
        }

        case AST_ASSIGN:
        {
            printf("ASSIGN '%s'", TOKEN_KIND_STRS[node->as.assign.op]);
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_node(ast, node->as.assign.target, indent + 1);
            ast_print_node(ast, node->as.assign.value_expr, indent + 1);

            break;
        }

        case AST_FUNC_CALL:
        {
            printf("CALL");
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_indent(indent + 1);
            printf("Function:\n");
            ast_print_node(ast, node->as.func_call.ident, indent + 2);

            if (node->as.func_call.arg_count)
            {
                ast_print_indent(indent + 1);
                printf("Arguments:\n");

                for (u32 i = 0; i < node->as.func_call.arg_count; i++)
                    ast_print_node(ast, node->as.func_call.args[i], indent + 2);
            }

            break;
        }

        case AST_IDENT:
        {
            printf("IDENT ");
            ast_print_namespace(node->as.ident.namespace_id);
            if (node->as.ident.namespace_id != NAMESPACE_ID_NONE) printf("::");
            ast_print_str(node->as.ident.name_id);
            ast_print_flags(node->flags);
            printf("\n");
            break;
        }

        case AST_LITERAL:
        {
            printf("LITERAL %s ", AST_LITERAL_KIND_STRS[node->as.literal.kind]);

            switch (node->as.literal.kind) {
                case LITERAL_STRING:
                    ast_print_str(node->as.literal.as.string);
                    break;

                case LITERAL_CHAR:
                    printf("'%c'", node->as.literal.as.character);
                    break;

                case LITERAL_INTEGER:
                    printf("%ld", node->as.literal.as.integer);
                    break;

                case LITERAL_FLOATING:
                    printf("%lf", node->as.literal.as.floating);
                    break;

                case LITERAL_NULL:
                    printf("null");
                    break;

                case LITERAL_BOOL:
                    printf(node->as.literal.as.boolean ? "true" : "false");
                    break;

                default:
                    printf("LITERAL TO DO PRINT BLAH");
                    break;
            }

            ast_print_flags(node->flags);
            printf("\n");

            break;
        }

        case AST_INDEX:
        {
            printf("INDEX");
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_indent(indent + 1);
            printf("Target:\n");
            ast_print_node(ast, node->as.index.ident, indent + 2);

            ast_print_indent(indent + 1);
            printf("Index:\n");
            ast_print_node(ast, node->as.index.index, indent + 2);

            break;
        }

        case AST_MEMBER_ACCESS:
        {
            printf("MEMBER %s ", node->as.member_access.pointer_access ? "->" : ".");
            ast_print_str(node->as.member_access.field_id);
            ast_print_flags(node->flags);
            printf("\n");

            ast_print_node(ast, node->as.member_access.ident, indent + 1);

            break;
        }

        default:
        {
            printf("<unknown node kind=%d>\n", node->kind);
            break;
        }
    }
}
