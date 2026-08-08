#ifndef LILY_AST_NODES_TYPES_H
#define LILY_AST_NODES_TYPES_H

#include "ids.h"
#include "symbols/types.h"
#include "token/types.h"
#include "utils/types.h"

#define AST_FLAGS_NONE          (0 << 0)
#define AST_FLAGS_IS_TOP_LEVEL  (1 << 0)
#define AST_FLAGS_IS_CONST      (1 << 1)
#define AST_FLAGS_IS_EXTERNAL   (1 << 2)
#define AST_FLAGS_IS_VARIADIC   (1 << 3)

#define AST_NODES(X)        \
    X(AST_ERROR)            \
                            \
    X(AST_MODULE)           \
    X(AST_IMPORT)           \
                            \
    X(AST_PARAM)            \
    X(AST_FIELD)            \
    X(AST_VARIANT)          \
                            \
    X(AST_MACRO)            \
    X(AST_MACRO_CALL)       \
                            \
    X(AST_FUNCTION)         \
    X(AST_STRUCT)           \
    X(AST_UNION)            \
    X(AST_ENUM)             \
                            \
    X(AST_BLOCK)            \
    X(AST_DEFER)            \
    X(AST_RETURN)           \
                            \
    X(AST_LET)              \
    X(AST_CONST)            \
                            \
    X(AST_BRANCH)           \
    X(AST_IF)               \
    X(AST_FOR)              \
    X(AST_WHILE)            \
                            \
    X(AST_BINOP)            \
    X(AST_UNARY)            \
    X(AST_FUNC_CALL)        \
    X(AST_IDENT)            \
    X(AST_LITERAL)          \
    X(AST_INDEX)            \
    X(AST_MEMBER_ACCESS)    \
    X(AST_STRUCT_INIT)      \
                            \
    X(AST_TYPE_BASE)        \
    X(AST_TYPE_ARRAY)       \
    X(AST_TYPE_POINTER)     \
    X(AST_TYPE_FUNCTION)    \
    X(AST_TYPE_VARIADIC)    \
                            \
    X(AST_KINDS_COUNT)      \

typedef enum {
    AST_NODES(GENERATE_ENUM)
} __attribute__((packed)) AstKind;

static const char* AST_KIND_STRS[] = {
    AST_NODES(GENERATE_STRING)
};

#undef AST_NODES

#define AST_LITERALS(X) \
    X(LITERAL_INTEGER)  \
    X(LITERAL_FLOATING) \
    X(LITERAL_STRING)   \
    X(LITERAL_CHAR)     \
    X(LITERAL_BOOL)     \
    X(LITERAL_NULL)     \

typedef enum {
    AST_LITERALS(GENERATE_ENUM)
} AstLiteralKind;

static const char* AST_LITERAL_KIND_STRS[] = {
    AST_LITERALS(GENERATE_STRING)
};

#undef AST_LITERALS

typedef struct AstNode AstNode;

//
// MODULE
//
typedef struct {
    NamespaceId namespace_id;
} AstModule;

//
// IMPORT
//
typedef struct {
    NamespaceId namespace_id;
} AstImport;

//
// FUNCTIONS
//
typedef struct {
    StringId name_id;
    AstNodeId type_expr;
} AstParam;

typedef struct {
    StringId name_id;

    AstNodeId* params;
    u32 param_count;
    u32 param_capacity;

    AstNodeId block;

    AstNodeId return_type_expr;
} AstFunctionDecl, AstMacroDecl;

//
// STRUCTS
//
typedef struct {
    StringId name_id;
    AstNodeId type_expr;
} AstField;

typedef struct {
    StringId name_id;

    AstNodeId* fields;
    u32 field_count;
    u32 field_capacity;
} AstStructDecl, AstUnionDecl;

//
// ENUMS
//
typedef struct {
    StringId  name_id;
    AstNodeId value_expr;
} AstVariant;

typedef struct {
    StringId name_id;

    // only integers are allowed
    AstNodeId type_expr;

    AstNodeId* variants;
    u32 variant_count;
    u32 variant_capacity;
} AstEnumDecl;

//
// BLOCK
//
typedef struct {
    AstNodeId* stmts;
    u32 stmt_count;
    u32 stmt_capacity;
} AstBlock;

//
// DEFER
//
typedef struct {
    AstNodeId stmt;
} AstDefer;

//
// RETURN
//
typedef struct {
    AstNodeId stmt;
} AstReturn;

//
// VARIABLE (AST_LET, AST_CONST)
//
typedef struct {
    StringId  name_id;
    AstNodeId value_expr;
    AstNodeId type_expr;
} AstVarDecl;

//
// IF
//
typedef struct {
    AstNodeId condition;
    AstNodeId block;
} AstBranch;

typedef struct {
    // only conditionals
    AstNodeId* branches;
    u32 branch_count;
    u32 branch_capacity;

    // null if no else block
    AstNodeId else_block;
} AstIf;

//
// FOR
//
typedef struct {
    AstNodeId init;
    AstNodeId cond;
    AstNodeId step;

    AstNodeId block;
} AstFor;

//
// WHILE
//
typedef struct {
    AstNodeId condition;
    AstNodeId block;
} AstWhile;

//
// BINARY
//
typedef struct {
    AstNodeId left;
    AstNodeId right;
    TokenKind op;
} AstBinary;

//
// UNARY
//
typedef struct {
    AstNodeId operand;
    TokenKind op;
} AstUnary;

//
// FUNCTION CALL
//
typedef struct {
    AstNodeId ident;

    AstNodeId* args;
    u32 arg_count;
    u32 arg_capacity;
} AstFnCall, AstMacroCall;

// 
// LITERAL
//
typedef struct {
    AstLiteralKind kind;

    union {
        StringId string;
        i64      integer;
        f64      floating;
        bool     boolean;
        char     character;
    } as;
} AstLiteral;

//
// INDEX
//
typedef struct {
    AstNodeId ident;
    AstNodeId index;
} AstIndex;

//
// MEMBER ACCESS
//
typedef struct {
    AstNodeId ident;
    StringId field_id;
    bool pointer_access;
} AstMemberAccess;

//
// STRUCT INITIALIZATION
//
// typedef struct {
//     AstNodeId ident;
//
//     AstNodeId* exprs;
//     u32 count;
//     u32 capacity;
// } AstStructInit;

//
// CONTINUE AND BREAK
//
typedef struct {
} AstContinue, AstBreak;

//
// IDENTIFIER
//
typedef struct {
    NamespaceId namespace_id;
    StringId name_id;

    SymbolRef symbol_ref;
} AstIdent;

typedef struct {
    AstNodeId ident;
} AstTypeBase;

typedef struct {
    AstNodeId element;
    AstNodeId size_expr;
} AstTypeArray;

typedef struct {
    AstNodeId base_type;
} AstTypePointer;

typedef struct {
    AstNodeId* params;
    u32 count;
    u32 capacity;

    AstNodeId return_type;
} AstTypeFunction;

typedef struct {
    AstNodeId element_type;
} AstTypeVariadic;

//
// NODE
//
typedef struct AstNode {
    AstNodeId id;
    AstKind kind;
    u16 flags;

    u8 __padding[4];
    TypeId resolved_type;

    Token* source_token;
    Span token_span;

    union {
        AstModule module_decl;
        AstImport import_decl;

        AstParam   param_decl;
        AstField   field_decl;
        AstVariant variant_decl;

        AstMacroDecl macro_decl;
        AstMacroCall macro_call;

        AstFunctionDecl func_decl;
        AstStructDecl   struct_decl;
        AstUnionDecl    union_decl;
        AstEnumDecl     enum_decl;

        AstVarDecl var_decl;
        AstVarDecl const_decl;

        AstBranch   branch;
        AstIf       if_stmt;
        AstFor      for_loop;
        AstWhile    while_loop;
        AstBreak    break_stmt;
        AstContinue continue_stmt;
        AstDefer    defer_stmt;
        AstReturn   return_stmt;

        AstBinary       binary_op;
        AstUnary        unary_op;
        AstFnCall       func_call;
        AstIdent        ident;
        AstLiteral      literal; 
        AstIndex        index;
        // AstStructInit   struct_init;
        AstMemberAccess member_access;

        AstBlock block;

        AstTypeBase     type_base_expr;
        AstTypeArray    type_array_expr;
        AstTypePointer  type_pointer_expr;
        AstTypeFunction type_function_expr;
        AstTypeVariadic type_variadic_expr;
    } as;
} AstNode;

#endif // !LILY_AST_NODES_TYPES_H
