#include "ast/nodes/nodes.h"
#include "ast/parser/expr/expr.h"
#include "ast/parser/stmts/stmts.h"
#include "diagnostics/diagnostics.h"
#include "namespacing/namespacing.h"
#include "string_interner/interner.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <stdlib.h>

// null denotation
static AstNodeId nud(Parser* p, Token* tok);
static AstNodeId led(Parser* p, Token* tok, AstNodeId left);
static u8        lbp_of(TokenKind kind);

AstNodeId parse_expression(Parser* p, i32 min_bp) {
    Token* tok = parser_advance(p);

    AstNodeId left = nud(p, tok);

    while (lbp_of(parser_peek(p) -> kind) > min_bp) {
        Token* op = parser_advance(p);
        left = led(p, op, left);
    }

    return left;
}

static AstNodeId nud(Parser* p, Token* tok) {
    switch (tok -> kind) {
        case TOK_FALSE: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.literal.kind = LITERAL_BOOL;
            node -> as.literal.as.boolean = false;

            return id;
        }

        case TOK_TRUE: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.literal.kind = LITERAL_BOOL;
            node -> as.literal.as.boolean = true;

            return id;
        }

        case TOK_NULL: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.literal.kind = LITERAL_NULL;

            return id;
        }

        case TOK_INTEGER_LIT: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.literal.kind = LITERAL_INTEGER;

            char c = *(tok -> lexeme.pointer + tok -> lexeme.length);
            *(tok -> lexeme.pointer + tok -> lexeme.length) = 0;
            node -> as.literal.as.integer = strtoll((char*) tok -> lexeme.pointer, null, 10);
            *(tok -> lexeme.pointer + tok -> lexeme.length) = c;
            
            return id;
        }

        case TOK_FLOAT_LIT: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.literal.kind = LITERAL_FLOATING;

            char c = *(tok -> lexeme.pointer + tok -> lexeme.length);
            *(tok -> lexeme.pointer + tok -> lexeme.length) = 0;
            node -> as.literal.as.integer = strtod((char*) tok -> lexeme.pointer, null);
            *(tok -> lexeme.pointer + tok -> lexeme.length) = c;

            return id;
        }

        case TOK_STRING_LIT: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.literal.kind = LITERAL_STRING;
            node -> as.literal.as.string = STRING_INTERNER_LOOKUP_TOKEN(tok);

            return id;
        }

        case TOK_CHAR_LIT: {
            AstNodeId id  = parser_create_node(p, AST_LITERAL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.literal.kind = LITERAL_CHAR;
            node -> as.literal.as.character = *(tok -> lexeme.pointer + 1);
            return id;
        }

        case TOK_IDENT: {
            StringId segments[NAMESPACE_MAX_DEPTH];
            u32 count = 0;

            segments[count++] = STRING_INTERNER_LOOKUP_TOKEN(tok);

            while (parser_check(p, TOK_COLON_COLON)) {
                parser_advance(p);

                Token* segment = parser_advance(p);

                if (segment->kind != TOK_IDENT) {
                    diagnostic_add_token(
                        &driver_ctx.diagnostics,
                        p -> id,
                        DIAG_ERROR,
                        segment,
                        DIAG_LOC_WHOLE_TOK,
                        "expected identifier",
                        "add a valid identifier here"
                    );

                    AstNode* node = arena_alloc(&p->module->ast.arena, sizeof(*node));
                    return parser_error_stmt(p, node);
                }

                if (count >= NAMESPACE_MAX_DEPTH) {
                    diagnostic_add_token(
                        &driver_ctx.diagnostics,
                        p -> id,
                        DIAG_ERROR,
                        segment,
                        DIAG_LOC_WHOLE_TOK,
                        "max namespacing depth achieved (8)",
                        "shorten the namespacing, max of 8 segments is supported"
                    );

                    AstNode* node = arena_alloc(&p->module->ast.arena, sizeof(*node));
                    return parser_error_stmt(p, node);
                }

                segments[count++] = STRING_INTERNER_LOOKUP_TOKEN(segment);
            }

            AstNodeId id  = parser_create_node(p, AST_IDENT);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.ident.name_id = segments[count - 1];

            if (count == 1) {
                node -> as.ident.namespace_id = NAMESPACE_ID_NONE;
            } else {
                node -> as.ident.namespace_id = namespace_intern(segments, count - 1);
            }

            return id;
        }

        case TOK_STAR:
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
        case TOK_AMP: {
            AstNodeId id  = parser_create_node(p, AST_UNARY);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.unary_op.op = tok -> kind;
            node -> as.unary_op.operand = parse_expression(p, 120);

            return id;
        }

        case TOK_LPAREN: {
            AstNodeId id = parse_expression(p, 0);

            if (!parser_check(p, TOK_RPAREN)) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    parser_peek_previous(p),
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here" 
                );

                return parser_error_stmt(p, ast_node_get(&p -> module -> ast, id)); 
            }

            parser_advance(p);

            return id;
        }

        default: {
            AstNodeId id  = parser_create_node(p, AST_ERROR);
            return parser_error_stmt(p, ast_node_get(&p -> module -> ast, id));
        }
    }
}

static AstNodeId led(Parser* p, Token* tok, AstNodeId left) {
    switch (tok -> kind) {
        case TOK_LPAREN: {
            AstNodeId id  = parser_create_node(p, AST_FUNC_CALL);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.func_call.ident = left;

            node -> as.func_call.args = arena_alloc(&p -> module -> ast.arena, sizeof(AstNodeId) * 4);
            node -> as.func_call.arg_capacity = 4;
            node -> as.func_call.arg_count = 0;

            if (!parser_check(p, TOK_RPAREN)) {
                do {
                    AstNodeId arg = parse_expression(p, 0);
                    
                    if (UNLIKELY(node -> as.func_call.arg_count >= node -> as.func_call.arg_capacity)) {
                        u64 size = sizeof(AstNodeId) * node -> as.func_call.arg_capacity;
                        
                        node -> as.func_call.args = arena_realloc(
                            &p -> module -> ast.arena,
                            node -> as.func_call.args,
                            size,
                            size * 2
                        );
                        node -> as.func_call.arg_capacity *= 2;

                        debug_printf("Function call args realloc from %ld -> %ld bytes\n", size, size * 2);
                    }

                    node -> as.func_call.args[node -> as.func_call.arg_count++] = arg;

                } while (parser_check(p, TOK_COMMA) && parser_advance(p));
            }

            if (!parser_check(p, TOK_RPAREN)) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    parser_peek_previous(p),
                    DIAG_LOC_END_OF_TOK,
                    "expected ')'",
                    "add a ')' here" 
                );

                return parser_error_stmt(p, node); 
            }

            parser_advance(p);

            return id;
        }

        case TOK_LBRACKET: {
            AstNodeId id  = parser_create_node(p, AST_INDEX);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.index.ident = left;
            node -> as.index.index = parse_expression(p, 0);

            if (!parser_check(p, TOK_RBRACKET)) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    parser_peek_previous(p),
                    DIAG_LOC_END_OF_TOK,
                    "expected ']'",
                    "add a ']' here" 
                );

                return parser_error_stmt(p, node); 
            }

            return id;
        }

        case TOK_ARROW:
        case TOK_DOT: {
            AstNodeId id  = parser_create_node(p, AST_MEMBER_ACCESS);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            Token* field = parser_peek(p); 

            if (field -> kind != TOK_IDENT) {
                diagnostic_add_token(
                    &driver_ctx.diagnostics,
                    p -> id,
                    DIAG_ERROR,
                    parser_peek_previous(p),
                    DIAG_LOC_END_OF_TOK,
                    "expected field name",
                    "add a valid field name here" 
                );

                return parser_error_stmt(p, node); 
            }

            node -> as.member_access.ident = left;
            node -> as.member_access.field_id = STRING_INTERNER_LOOKUP_TOKEN(field);
            node -> as.member_access.pointer_access = false;

            if (tok -> kind == TOK_ARROW) {
                node -> as.member_access.pointer_access = true;
            }

            parser_advance(p);

            return id;
        }

        default: {
            AstNodeId id  = parser_create_node(p, AST_BINOP);
            AstNode* node = ast_node_get(&p -> module -> ast, id);

            node -> as.binary_op.op = tok -> kind;
            node -> as.binary_op.left  = left;
            node -> as.binary_op.right = parse_expression(p, op_table[tok -> kind].rbp);

            return id;
        }
    }
}

static u8 lbp_of(TokenKind kind) {
    if ((u32)kind >= OP_TABLE_LEN) return 0;
    return op_table[kind].lbp;
}
