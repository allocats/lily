#ifndef LILY_AST_NODES_TYPES_H
#define LILY_AST_NODES_TYPES_H

#include "ids.h"
#include "token/types.h"

#include <assert.h>

static constexpr u32 AST_FLAGS_NONE        = 0 << 0;
static constexpr u32 AST_FLAGS_IS_TOP_DECL = 1 << 0;
static constexpr u32 AST_FLAGS_IS_CONSTANT = 1 << 1;
static constexpr u32 AST_FLAGS_IS_EXTERNAL = 1 << 2;
static constexpr u32 AST_FLAGS_IS_VARIADIC = 1 << 3;

static_assert(AST_FLAGS_NONE != AST_FLAGS_IS_TOP_DECL);
static_assert(AST_FLAGS_NONE != AST_FLAGS_IS_CONSTANT);
static_assert(AST_FLAGS_NONE != AST_FLAGS_IS_EXTERNAL);
static_assert(AST_FLAGS_NONE != AST_FLAGS_IS_VARIADIC);

static_assert(AST_FLAGS_IS_TOP_DECL != AST_FLAGS_IS_CONSTANT);
static_assert(AST_FLAGS_IS_TOP_DECL != AST_FLAGS_IS_EXTERNAL);
static_assert(AST_FLAGS_IS_TOP_DECL != AST_FLAGS_IS_VARIADIC);

static_assert(AST_FLAGS_IS_CONSTANT != AST_FLAGS_IS_EXTERNAL);
static_assert(AST_FLAGS_IS_CONSTANT != AST_FLAGS_IS_VARIADIC);

static_assert(AST_FLAGS_IS_EXTERNAL != AST_FLAGS_IS_VARIADIC);

#define AST_NODES(X)            \
    X(AST_ERROR)                \
                                \
    X(AST_EXECUTE_DIRECTIVE)    \
    X(AST_IMPORT_DIRECTIVE)     \
    X(AST_PASTE_DIRECTIVE)      \
                                \
    X(AST_PARAMETER)            \
    X(AST_FUNCTION_DECL)        \
    X(AST_MACRO_DECL)           \
                                \
    X(AST_FIELD)                \
    X(AST_STRUCT_DECL)          \
    X(AST_UNION_DECL)           \
                                \
    X(AST_VARIANT)              \
    X(AST_ENUM_DECL)            \
                                \
    X(AST_BLOCK)                \
                                \
    X(AST_VARIABLE_DECL)        \
                                \
    X(AST_DEFER_STMT)           \
    X(AST_RETURN_STMT)          \
                                \
    X(AST_SWITCH_STMT)          \
    X(AST_SWITCH_CASE)          \
                                \
    X(AST_BRANCH)               \
    X(AST_IF_STMT)              \
    X(AST_FOR_LOOP)             \
    X(AST_WHILE_LOOP)           \
                                \
    X(AST_CONTINUE_STMT)        \
    X(AST_BREAK_STMT)           \
                                \
    X(AST_BINARY_OP)            \
    X(AST_UNARY_OP)             \
    X(AST_FUNCTION_CALL)        \
    X(AST_MACRO_CALL)           \
    X(AST_IDENTIFIER)           \
    X(AST_LITERAL)              \
    X(AST_INDEX)                \
    X(AST_MEMBER_ACCESS)        \
    X(AST_FIELD_INIT)           \
    X(AST_STRUCT_LITERAL)       \
                                \
    X(AST_TYPE_BASE)            \
    X(AST_TYPE_ARRAY)           \
    X(AST_TYPE_POINTER)         \
    X(AST_TYPE_FUNCTION)        \
    X(AST_TYPE_VARIADIC)        \
                                \
    X(AST_KINDS_COUNT)          \

typedef enum {
    AST_NODES(GENERATE_ENUM)
} __attribute__((packed)) AstNodeKind;

static const char* AST_NODE_KIND_STRINGS[] = {
    AST_NODES(GENERATE_STRING)
};

#undef AST_NODES



typedef enum {
    LITERAL_STRING,
    LITERAL_CHAR,
    LITERAL_INTEGER,
    LITERAL_FLOAT,
    LITERAL_BOOL,
    LITERAL_NULL
} LiteralKind;



// Simple abstraction to reduce code noise/duplication
// can just use one function to resize
typedef struct {
    AstNodeId* ids;
    u32 count;
    u32 capacity;
} AstNodeIdList;



typedef struct {
    AstNodeId expr;
} AstExecuteDirective;



typedef struct {
    AstNodeId binding;

    StringId path;
    ModuleId resolved;
} AstImportDirective;



typedef struct {
    StringId path;
} AstPasteDirective;



typedef struct {
    StringId name;
    AstNodeId type_expr;
} AstParameterDecl;



typedef struct {
    AstNodeIdList parameters;

    StringId name;

    AstNodeId return_type_expr;

    AstNodeId block;
} AstFunctionDecl, AstMacroDecl;

typedef struct {
    AstNodeIdList arguments;

    AstNodeId identifier; 
} AstFunctionCall, AstMacroCall;



typedef struct {
    StringId  name;
    AstNodeId type_expr;
} AstField;

typedef struct {
    AstNodeIdList fields;

    StringId name;
} AstStructDecl, AstUnionDecl;



typedef struct {
    StringId  name;
    AstNodeId value_expr;
} AstVariant;

typedef struct {
    AstNodeIdList variants;

    AstNodeId type_expr;

    StringId name;
} AstEnumDecl;



typedef struct {
    AstNodeIdList statements;
} AstBlock; 



typedef struct {
    StringId  name;
    AstNodeId type_expr;
    AstNodeId value_expr;
} AstVariableDecl;



typedef struct {
    AstNodeId expr;
} AstReturnStmt;



typedef struct {
    AstNodeId stmt;
} AstDeferStmt;



typedef struct {
    AstNodeIdList patterns; // in order to support something like: case x > 1, x < 10:
    AstNodeId block;
} AstSwitchCase;

typedef struct {
    AstNodeId     value; // what is being switched on
    AstNodeId     default_case; // none if none lol 
    AstNodeIdList cases;
} AstSwitchStatement;



typedef struct {
    AstNodeId condition;
    AstNodeId block;
} AstBranch;

typedef struct {
    AstNodeIdList branches;

    AstNodeId else_block;
} AstIfStmt;



// TODO: Think about `for x in array {}`, perhaps 
// behind the scenes automatically extract it into
// this. Look into creating an iterator system that 
// can be implemented on user defined types as well
typedef struct {
    AstNodeId init;
    AstNodeId cond;
    AstNodeId step;

    AstNodeId block;
} AstForLoop;



typedef struct {
    AstNodeId cond;
    AstNodeId block;
} AstWhileLoop;



typedef struct {
    // TODO: Labels perhaps
} AstContinueStmt, AstBreakStmt;



typedef struct {
    TokenKind op;
    AstNodeId left;
    AstNodeId right;
} AstBinaryOp;



typedef struct {
    TokenKind op;
    AstNodeId operand;
} AstUnaryOp;



typedef struct {
    StringId name;
} AstIdentifier;



typedef struct {
    LiteralKind kind;
    
    union {
        StringId string;
        i64      integer;
        f64      floating;
        bool     boolean;
        i64      character;
    } as;
} AstLiteral;



typedef struct {
    bool used_pointer_access;

    AstNodeId object;
    AstNodeId member;
} AstMemberAccess;


typedef struct {
    AstNodeId field;
    AstNodeId value;
} AstFieldInit;

// MyStruct { .x = 2, .y = 1, .z = 0 };
typedef struct {
    AstNodeId struct_type;

    AstNodeIdList inits;
} AstStructLiteral;



typedef struct {
    AstNodeId object;
    AstNodeId index_expr;
} AstIndex;



typedef struct {
    AstNodeId expr;
} AstTypeBase;


typedef struct {
    AstNodeId element;
    AstNodeId size_expr;
} AstTypeArray;


typedef struct {
    AstNodeId base_type;
} AstTypePointer;


typedef struct {
    AstNodeIdList parameters;

    AstNodeId return_type;
} AstTypeFunction;


typedef struct {
    AstNodeId element_type;
} AstTypeVariadic;



typedef struct {
    AstNodeId id;
    AstNodeKind kind;
    u16 flags;

    TypeId resolved_type;

    SpanU32 tokens;

    union {
        // Directives ("#ident ...") 
        AstExecuteDirective execute_directive;
        AstImportDirective import_directive;
        AstPasteDirective  paste_directive;

        // Function & Macro
        AstParameterDecl parameter_decl;

        AstFunctionDecl function_decl;
        AstFunctionCall function_call;

        AstMacroDecl macro_decl;
        AstMacroCall macro_call;

        // Struct & Union
        AstField field;

        AstStructDecl struct_decl;
        AstUnionDecl  union_decl;

        // Enum
        AstVariant variant;
        AstEnumDecl enum_decl;

        // Block
        AstBlock block;

        // Variables (const is on the flags)
        AstVariableDecl variable_decl;

        // Return statment
        AstReturnStmt return_stmt;

        // Defer statment
        AstDeferStmt defer_stmt;

        // Switch statements
        AstSwitchCase switch_case;
        AstSwitchStatement switch_stmt;

        // If statements
        AstBranch branch;
        AstIfStmt if_stmt;

        // For loop
        AstForLoop for_loop;

        // While loop
        AstWhileLoop while_loop;

        // Break & Continue
        AstContinueStmt continue_stmt;
        AstBreakStmt    break_stmt;

        // Binary Operation
        AstBinaryOp binary_op;

        // Unary Operation
        AstUnaryOp unary_op;

        // Identifiers
        AstIdentifier identifier;

        // Literals
        AstLiteral literal;

        // Member access
        AstMemberAccess member_access;

        // Struct Literal
        AstFieldInit field_init;
        AstStructLiteral struct_literal;

        // Index
        AstIndex index;

        // Types
        AstTypeBase     type_base;
        AstTypeArray    type_array;
        AstTypePointer  type_pointer;
        AstTypeFunction type_function;
        AstTypeVariadic type_variadic;
    } as;
} __attribute__((aligned(64))) AstNode;

#endif // !LILY_AST_NODES_TYPES_H
