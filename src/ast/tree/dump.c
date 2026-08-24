#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "ast/nodes/types.h"
#include "ast/tree/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "string_interner/interner.h"
#include "token/types.h"
#include "utils/types.h"

extern DriverCtx driver;

static void ast_print_separator(FILE *out) {
    fprintf(out, "\n======================================================================\n");
}

static void ast_print_string_id(FILE *out, const char *label, StringId id) {
    fprintf(out, "%s: StringId=%u", label, (u32)id);

    if (id < driver.string_interner.count) {
        fprintf(out, "  value=\"%.*s\"",
                STR8_PRINT(id));
    } else {
        fprintf(out, "  value=<invalid StringId>");
    }

    fprintf(out, "\n");
}

static void ast_print_flags(FILE *out, u16 flags) {
    fprintf(out, "flags:          0x%04x", (unsigned)flags);

    if (flags == AST_FLAGS_NONE) {
        fprintf(out, " [NONE]\n");
        return;
    }

    fprintf(out, " [");

    bool first = true;

#define PRINT_FLAG(flag)                                             \
    do {                                                             \
        if (flags & (flag)) {                                        \
            if (!first) fprintf(out, " | ");                         \
            fprintf(out, #flag);                                     \
            first = false;                                           \
        }                                                            \
    } while (0)

    PRINT_FLAG(AST_FLAGS_IS_TOP_DECL);
    PRINT_FLAG(AST_FLAGS_IS_CONSTANT);
    PRINT_FLAG(AST_FLAGS_IS_EXTERNAL);
    PRINT_FLAG(AST_FLAGS_IS_VARIADIC);

#undef PRINT_FLAG

    fprintf(out, "]\n");
}

static void ast_print_node_header(FILE *out, const AstNode *node, u32 index) {
    const char *kind_name = "<unknown>";

    if ((u32)node -> kind < AST_KINDS_COUNT) {
        kind_name = AST_NODE_KIND_STRINGS[node -> kind];
    }

    fprintf(
        out,
        "\n"
        "┌──────────────────────────────────────────────────────────────\n"
        "│ NODE #%u\n"
        "├──────────────────────────────────────────────────────────────\n",
        index
    );

    fprintf(
        out,
        "│ address:        %p\n",
        (void*) node
    );

    fprintf(
        out,
        "│ id:             %u\n",
        (u32)node -> id
    );

    fprintf(
        out,
        "│ kind:           %u (%s)\n",
        (u32)node -> kind,
        kind_name
    );

    fprintf(
        out,
        "│ sizeof(node):   %zu\n",
        sizeof(*node)
    );

    fprintf(
        out,
        "│ resolved_type:  %u\n",
        (u32)node -> resolved_type
    );

    fprintf(out, "│ ");

    ast_print_flags(out, node -> flags);

    fprintf(out, "│ token span:\n");
    fprintf(
        out,
        "│   start:        %u\n"
        "│   end:          %u\n"
        "│   length:       %u\n",
        node -> tokens.start,
        node -> tokens.end,
        node -> tokens.end - node -> tokens.start
    );
}

static void ast_print_payload(FILE *out, const AstNode *node) {
    switch (node -> kind) {

        /* --------------------------------------------------------
         * Directives
         * -------------------------------------------------------- */

        case AST_EXECUTE_DIRECTIVE:
            fprintf(out, "│ payload: AstExecuteDirective\n");
            fprintf(out, "│   expr: AstNodeId=%u\n", (u32)node -> as.execute_directive.expr);
            break;

        case AST_IMPORT_DIRECTIVE:
            fprintf(out, "│ payload: AstImportDirective\n");
            fprintf(out, "│   binding: AstNodeId=%u\n", (u32)node -> as.import_directive.binding);

            ast_print_string_id(out, "│   path", node -> as.import_directive.path);

            fprintf(out, "│   resolved: ModuleId=%u\n", (u32)node -> as.import_directive.resolved);
            break;

        case AST_INCLUDE_DIRECTIVE:
            fprintf(out, "│ payload: AstIncludeDirective\n");

            ast_print_string_id(out, "│   path", node -> as.include_directive.path);
            break;


        /* --------------------------------------------------------
         * Function / macro
         * -------------------------------------------------------- */

        case AST_PARAMETER:
            fprintf(out, "│ payload: AstParameterDecl\n");

            ast_print_string_id(out, "│   name", node -> as.parameter_decl.name);

            fprintf(out, "│   type_expr: AstNodeId=%u\n", (u32)node -> as.parameter_decl.type_expr);
            break;

        case AST_FUNCTION_DECL:
        case AST_MACRO_DECL:
            fprintf(out, "│ payload: %s\n", node -> kind == AST_FUNCTION_DECL ? "AstFunctionDecl" : "AstMacroDecl");

            fprintf(
                out,
                "│   parameters:\n"
                "│     ids:      %p\n"
                "│     count:    %u\n"
                "│     capacity: %u\n",
                (void*) node -> as.function_decl.parameters.ids,
                node -> as.function_decl.parameters.count,
                node -> as.function_decl.parameters.capacity
            );

            for (u32 i = 0; i < node -> as.function_decl.parameters.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.function_decl.parameters.ids[i]);
            }

            ast_print_string_id(out, "│   name", node -> as.function_decl.name);

            fprintf(
                out,
                "│   return_type_expr: AstNodeId=%u\n" 
                "│   block:            AstNodeId=%u\n",
                (u32)node -> as.function_decl.return_type_expr,
                (u32)node -> as.function_decl.block
            );
            break;

        case AST_FUNCTION_CALL:
        case AST_MACRO_CALL:
            fprintf(out, "│ payload: %s\n", node -> kind == AST_FUNCTION_CALL ? "AstFunctionCall" : "AstMacroCall");

            fprintf(
                out,
                "│   arguments:\n" 
                "│     ids:      %p\n" 
                "│     count:    %u\n" 
                "│     capacity: %u\n",
                (void*) node -> as.function_call.arguments.ids,
                node -> as.function_call.arguments.count,
                node -> as.function_call.arguments.capacity
            );

            for (u32 i = 0; i < node -> as.function_call.arguments.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.function_call.arguments.ids[i]);
            }

            fprintf(out, "│   identifier: AstNodeId=%u\n", (u32)node -> as.function_call.identifier);
            break;


        /* --------------------------------------------------------
         * Struct / union / enum
         * -------------------------------------------------------- */

        case AST_FIELD:
            fprintf(out, "│ payload: AstField\n");

            ast_print_string_id(out, "│   name", node -> as.field.name);

            fprintf(out, "│   type_expr: AstNodeId=%u\n", (u32)node -> as.field.type_expr);
            break;

        case AST_STRUCT_DECL:
        case AST_UNION_DECL:
            fprintf(out, "│ payload: %s\n", node -> kind == AST_STRUCT_DECL ? "AstStructDecl" : "AstUnionDecl");

            fprintf(
                out,
                "│   fields:\n" 
                "│     ids:      %p\n" 
                "│     count:    %u\n" 
                "│     capacity: %u\n",
                (void*) node -> as.struct_decl.fields.ids,
                node -> as.struct_decl.fields.count,
                node -> as.struct_decl.fields.capacity
            );

            for (u32 i = 0; i < node -> as.struct_decl.fields.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.struct_decl.fields.ids[i]);
            }

            ast_print_string_id(out, "│   name", node -> as.struct_decl.name);
            break;

        case AST_VARIANT:
            fprintf(out, "│ payload: AstVariant\n");

            ast_print_string_id(out, "│   name", node -> as.variant.name);

            fprintf(out, "│   value_expr: AstNodeId=%u\n", (u32)node -> as.variant.value_expr);
            break;

        case AST_ENUM_DECL:
            fprintf(out, "│ payload: AstEnumDecl\n");

            fprintf(
                out,
                "│   variants:\n" 
                "│     ids:      %p\n" 
                "│     count:    %u\n" 
                "│     capacity: %u\n",
                (void*) node -> as.enum_decl.variants.ids,
                node -> as.enum_decl.variants.count,
                node -> as.enum_decl.variants.capacity
            );

            for (u32 i = 0; i < node -> as.enum_decl.variants.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.enum_decl.variants.ids[i]);
            }

            fprintf(out, "│   type_expr: AstNodeId=%u\n", (u32)node -> as.enum_decl.type_expr);

            ast_print_string_id(out, "│   name", node -> as.enum_decl.name);
            break;


        /* --------------------------------------------------------
         * Statements / blocks
         * -------------------------------------------------------- */

        case AST_BLOCK:
            fprintf(out, "│ payload: AstBlock\n");

            fprintf(
                out,
                "│   statements:\n" 
                "│     ids:      %p\n" 
                "│     count:    %u\n" 
                "│     capacity: %u\n",
                (void*) node -> as.block.statements.ids,
                node -> as.block.statements.count,
                node -> as.block.statements.capacity
            );

            for (u32 i = 0; i < node -> as.block.statements.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.block.statements.ids[i]);
            }
            break;

        case AST_VARIABLE_DECL:
            fprintf(out, "│ payload: AstVariableDecl\n");

            ast_print_string_id(out, "│   name", node -> as.variable_decl.name);

            fprintf(
                out,
                "│   type_expr:  AstNodeId=%u\n" 
                "│   value_expr: AstNodeId=%u\n",
                (u32)node -> as.variable_decl.type_expr,
                (u32)node -> as.variable_decl.value_expr
            );
            break;

        case AST_RETURN_STMT:
            fprintf(out, "│ payload: AstReturnStmt\n");
            fprintf(out, "│   expr: AstNodeId=%u\n", (u32)node -> as.return_stmt.expr);
            break;

        case AST_DEFER_STMT:
            fprintf(out, "│ payload: AstDeferStmt\n");
            fprintf(out, "│   expr: AstNodeId=%u\n", (u32)node -> as.defer_stmt.stmt);
            break;


        /* --------------------------------------------------------
         * Switch
         * -------------------------------------------------------- */

        case AST_SWITCH_CASE:
            fprintf(out, "│ payload: AstSwitchCase\n");

            fprintf(
                out,
                "│   patterns:\n" 
                "│     ids:      %p\n" 
                "│     count:    %u\n" 
                "│     capacity: %u\n",
                (void*) node -> as.switch_case.patterns.ids,
                node -> as.switch_case.patterns.count,
                node -> as.switch_case.patterns.capacity
            );

            for (u32 i = 0; i < node -> as.switch_case.patterns.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.switch_case.patterns.ids[i]);
            }

            fprintf(out, "│   block: AstNodeId=%u\n", (u32)node -> as.switch_case.block);
            break;

        case AST_SWITCH_STMT:
            fprintf(out, "│ payload: AstSwitchStatement\n");

            fprintf(
                out,
                "│   value:        AstNodeId=%u\n" 
                "│   default_case: AstNodeId=%u\n",
                (u32)node -> as.switch_stmt.value,
                (u32)node -> as.switch_stmt.default_case
            );

            fprintf(
                out,
                "│   cases:\n" 
                "│     ids:      %p\n" 
                "│     count:    %u\n" 
                "│     capacity: %u\n",
                (void*) node -> as.switch_stmt.cases.ids,
                node -> as.switch_stmt.cases.count,
                node -> as.switch_stmt.cases.capacity
            );

            for (u32 i = 0; i < node -> as.switch_stmt.cases.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.switch_stmt.cases.ids[i]);
            }
            break;


        /* --------------------------------------------------------
         * Branching / loops
         * -------------------------------------------------------- */

        case AST_BRANCH:
            fprintf(out, "│ payload: AstBranch\n");

            fprintf(
                out,
                "│   condition: AstNodeId=%u\n" "│   block:     AstNodeId=%u\n",
                (u32)node -> as.branch.condition,
                (u32)node -> as.branch.block
            );
            break;

        case AST_IF_STMT:
            fprintf(out, "│ payload: AstIfStmt\n");

            fprintf(
                out,
                "│   branches:\n" 
                "│     ids:      %p\n" 
                "│     count:    %u\n" 
                "│     capacity: %u\n",
                (void*) node -> as.if_stmt.branches.ids,
                node -> as.if_stmt.branches.count,
                node -> as.if_stmt.branches.capacity
            );

            for (u32 i = 0; i < node -> as.if_stmt.branches.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.if_stmt.branches.ids[i]);
            }

            fprintf(out, "│   else_block: AstNodeId=%u\n", (u32)node -> as.if_stmt.else_block);
            break;

        case AST_FOR_LOOP:
            fprintf(out, "│ payload: AstForLoop\n");

            fprintf(
                out,
                "│   init:  AstNodeId=%u\n" 
                "│   cond:  AstNodeId=%u\n" 
                "│   step:  AstNodeId=%u\n" 
                "│   block: AstNodeId=%u\n",
                (u32)node -> as.for_loop.init,
                (u32)node -> as.for_loop.cond,
                (u32)node -> as.for_loop.step,
                (u32)node -> as.for_loop.block
            );
            break;

        case AST_WHILE_LOOP:
            fprintf(out, "│ payload: AstWhileLoop\n");

            fprintf(
                out,
                "│   cond:  AstNodeId=%u\n" 
                "│   block: AstNodeId=%u\n",
                (u32)node -> as.while_loop.cond,
                (u32)node -> as.while_loop.block
            );
            break;

        case AST_CONTINUE_STMT:
            fprintf(out, "│ payload: AstContinueStmt\n");
            fprintf(out, "│   (no payload fields)\n");
            break;

        case AST_BREAK_STMT:
            fprintf(out, "│ payload: AstBreakStmt\n");
            fprintf(out, "│   (no payload fields)\n");
            break;


        /* --------------------------------------------------------
         * Expressions
         * -------------------------------------------------------- */

        case AST_BINARY_OP:
            fprintf(out, "│ payload: AstBinaryOp\n");

            fprintf(
                out,
                "│   op:    TokenKind=%s\n" 
                "│   left:  AstNodeId=%u\n" 
                "│   right: AstNodeId=%u\n",
                TOKEN_KIND_STRS[node -> as.binary_op.op],
                (u32)node -> as.binary_op.left,
                (u32)node -> as.binary_op.right
            );
            break;

        case AST_UNARY_OP:
            fprintf(out, "│ payload: AstUnaryOp\n");

            fprintf(
                out,
                "│   op:      TokenKind=%s\n" 
                "│   operand: AstNodeId=%u\n",
                TOKEN_KIND_STRS[node -> as.unary_op.op],
                (u32)node -> as.unary_op.operand
            );
            break;

        case AST_IDENTIFIER:
            fprintf(out, "│ payload: AstIdentifier\n");

            ast_print_string_id(
                out,
                "│   name",
                node -> as.identifier.name
            );
            break;

        case AST_LITERAL:
            fprintf(out, "│ payload: AstLiteral\n");
            fprintf(out, "│   kind: LiteralKind=%u", (u32)node -> as.literal.kind);

            switch (node -> as.literal.kind) {
                case LITERAL_STRING:
                    fprintf(out, " (LITERAL_STRING)\n");
                    ast_print_string_id(
                        out,
                        "│   string",
                        node -> as.literal.as.string
                    );
                    break;

                case LITERAL_CHAR:
                    fprintf(
                        out,
                        " (LITERAL_CHAR)\n" "│   character: %lld\n",
                        (long long)node -> as.literal.as.character
                    );
                    break;

                case LITERAL_INTEGER:
                    fprintf(
                        out,
                        " (LITERAL_INTEGER)\n" "│   integer: %lld\n",
                        (long long)node -> as.literal.as.integer
                    );
                    break;

                case LITERAL_FLOAT:
                    fprintf(out, " (LITERAL_FLOAT)\n" "│   floating: %.17g\n", node -> as.literal.as.floating);
                    break;

                case LITERAL_BOOL:
                    fprintf(
                        out,
                        " (LITERAL_BOOL)\n" "│   boolean: %s\n",
                        node -> as.literal.as.boolean ? "true" : "false"
                    );
                    break;

                case LITERAL_NULL:
                    fprintf(out, " (LITERAL_NULL)\n" "│   value: null\n");
                    break;

                default:
                    fprintf(out, " (UNKNOWN)\n");
                    break;
            }
            break;

        case AST_MEMBER_ACCESS:
            fprintf(out, "│ payload: AstMemberAccess\n");

            fprintf(
                out,
                "│   used_pointer_access: %s\n"
                "│   object:              AstNodeId=%u\n"
                "│   member:              AstNodeId=%u\n",
                node -> as.member_access.used_pointer_access ? "true" : "false",
                (u32)node -> as.member_access.object,
                (u32)node -> as.member_access.member
            );
            break;

        case AST_FIELD_INIT:
            fprintf(out, "│ payload: AstFieldInit\n");

            fprintf(
                out,
                "│   field: AstNodeId=%u\n" 
                "│   value: AstNodeId=%u\n",
                (u32)node -> as.field_init.field,
                (u32)node -> as.field_init.value
            );
            break;

        case AST_STRUCT_LITERAL:
            fprintf(out, "│ payload: AstStructLiteral\n");
            fprintf(out, "│   struct_type: AstNodeId=%u\n", (u32)node -> as.struct_literal.struct_type);

            fprintf(
                out,
                "│   inits:\n"
                "│     ids:      %p\n"
                "│     count:    %u\n"
                "│     capacity: %u\n",
                (void*) node -> as.struct_literal.inits.ids,
                node -> as.struct_literal.inits.count,
                node -> as.struct_literal.inits.capacity
            );

            for (u32 i = 0; i < node -> as.struct_literal.inits.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.struct_literal.inits.ids[i]);
            }
            break;

        case AST_INDEX:
            fprintf(out, "│ payload: AstIndex\n");

            fprintf(
                out,
                "│   object:     AstNodeId=%u\n" 
                "│   index_expr: AstNodeId=%u\n",
                (u32)node -> as.index.object,
                (u32)node -> as.index.index_expr
            );
            break;


        /* --------------------------------------------------------
         * Types
         * -------------------------------------------------------- */

        case AST_TYPE_BASE:
            fprintf(out, "│ payload: AstTypeBase\n");
            fprintf(out, "│   expr: AstNodeId=%u\n", (u32)node -> as.type_base.expr);
            break;

        case AST_TYPE_ARRAY:
            fprintf(out, "│ payload: AstTypeArray\n");

            fprintf(
                out,
                "│   element:   AstNodeId=%u\n" 
                "│   size_expr: AstNodeId=%u\n",
                (u32)node -> as.type_array.element,
                (u32)node -> as.type_array.size_expr
            );
            break;

        case AST_TYPE_POINTER:
            fprintf(out, "│ payload: AstTypePointer\n");
            fprintf(out, "│   base_type: AstNodeId=%u\n", (u32)node -> as.type_pointer.base_type);
            break;

        case AST_TYPE_FUNCTION:
            fprintf(out, "│ payload: AstTypeFunction\n");

            fprintf(
                out,
                "│   parameters:\n"
                "│     ids:      %p\n"
                "│     count:    %u\n"
                "│     capacity: %u\n",
                (void*) node -> as.type_function.parameters.ids,
                node -> as.type_function.parameters.count,
                node -> as.type_function.parameters.capacity
            );

            for (u32 i = 0; i < node -> as.type_function.parameters.count; i++) {
                fprintf(out, "│       [%u] AstNodeId=%u\n", i, (u32)node -> as.type_function.parameters.ids[i]);
            }

            fprintf(out, "│   return_type: AstNodeId=%u\n", (u32)node -> as.type_function.return_type);
            break;

        case AST_TYPE_VARIADIC:
            fprintf(out, "│ payload: AstTypeVariadic\n");
            fprintf(out, "│   element_type: AstNodeId=%u\n", (u32)node -> as.type_variadic.element_type);
            break;


        /* --------------------------------------------------------
         * Error / unknown
         * -------------------------------------------------------- */

        case AST_ERROR:
            fprintf(out, "│ payload: <AST_ERROR>\n");
            break;

        default:
            fprintf(out, "│ payload: <unhandled AstNodeKind=%u>\n", (u32)node -> kind);
            break;
    }

    fprintf(out, "└──────────────────────────────────────────────────────────────\n");
}


void ast_print(char *path, FileId file_id) {
    FILE *out = stderr;
    bool close_output = false;

    File* file = file_lookup_id(file_id);
    Ast* ast = &file -> ast;

    if (path != NULL) {
        FILE *file = fopen(path, "w");

        if (file != NULL) {
            out = file;
            close_output = true;

            fprintf(stderr, "Dumped AST to %s\n", path);
        }
    }

    fprintf(
        out,
        "\n"
        "######################################################################\n"
        "#                              AST DUMP                             #\n"
        "######################################################################\n"
        "\n"
    );

    fprintf(
        out,
        "File: %.*s\n\n"
        "Ast\n"
        "├── address:         %p\n"
        "├── nodes:           %p\n"
        "├── count:           %u\n"
        "├── capacity:        %u\n"
        "├── node_size:       %zu bytes\n"
        "├── allocated_size:  %zu bytes\n"
        "├── gpa:             %p\n"
        "└── nodes_arena:     %p\n",
        file -> path.len, 
        file -> path.ptr,
        (void*) ast,
        (void*) ast -> nodes,
        ast -> count,
        ast -> capacity,
        sizeof(AstNode),
        sizeof(AstNode) * (usize)ast -> capacity,
        (void*) &ast -> gpa,
        (void*) &ast -> nodes_arena
    );

    fprintf(
        out,
        "\n"
        "AST memory summary:\n"
        "  nodes pointer:     %p\n"
        "  nodes array size:  %zu bytes\n"
        "  nodes used size:   %zu bytes\n"
        "  nodes free slots:  %u\n",
        (void*) ast -> nodes,
        sizeof(AstNode) * (usize)ast -> capacity,
        sizeof(AstNode) * (usize)ast -> count,
        ast -> capacity - ast -> count
    );

    fprintf(
        out,
        "\n"
        "----------------------------------------------------------------------\n"
        "ARENA: gpa\n"
        "address: %p\n"
        "----------------------------------------------------------------------\n",
        (void*) &ast -> gpa
    );

    arena_print_stats(out, &ast -> gpa, "gpa");

    fprintf(
        out,
        "\n"
        "----------------------------------------------------------------------\n"
        "ARENA: nodes\n"
        "address: %p\n"
        "----------------------------------------------------------------------\n",
        (void*) &ast -> nodes_arena
    );

    arena_print_stats(out, &ast -> nodes_arena, "nodes");


    ast_print_separator(out);

    fprintf(
        out,
        "AST NODE ARRAY\n" "count=%u capacity=%u nodes=%p\n",
        ast -> count,
        ast -> capacity,
        (void*) ast -> nodes
    );

    ast_print_separator(out);


    for (u32 i = 0; i < ast -> count; i++) {
        AstNode *node = &ast -> nodes[i];

        ast_print_node_header(out, node, i);
        ast_print_payload(out, node);

        fprintf(out, "\n");
    }


    ast_print_separator(out);

    fprintf(
        out,
        "AST SUMMARY\n"
        "----------------------------------------------------------------------\n"
        "Ast address:        %p\n"
        "nodes address:      %p\n"
        "gpa address:        %p\n"
        "nodes_arena:        %p\n"
        "node count:         %u\n"
        "node capacity:      %u\n"
        "node size:          %zu bytes\n"
        "allocated node mem: %zu bytes\n"
        "used node mem:      %zu bytes\n"
        "unused node slots:  %u\n",
        (void*) ast,
        (void*) ast -> nodes,
        (void*) &ast -> gpa,
        (void*) &ast -> nodes_arena,
        ast -> count,
        ast -> capacity,
        sizeof(AstNode),
        sizeof(AstNode) * (usize)ast -> capacity,
        sizeof(AstNode) * (usize)ast -> count,
        ast -> capacity - ast -> count
    );

    fprintf(
        out,
        "----------------------------------------------------------------------\n"
        "END AST DUMP\n"
        "######################################################################\n"
        "\n"
    );

    if (close_output) {
        fclose(out);
    }
}
