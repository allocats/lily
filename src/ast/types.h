#ifndef LILY_AST_TYPES_H
#define LILY_AST_TYPES_H

#include "ids.h"

#include <assert.h>

static constexpr u16 AST_FLAGS_NONE        = 0 << 0;
static constexpr u16 AST_FLAGS_IS_TOP_DECL = 1 << 0;
static constexpr u16 AST_FLAGS_IS_CONSTANT = 1 << 1;
static constexpr u16 AST_FLAGS_IS_EXTERNAL = 1 << 2;

static_assert(AST_FLAGS_NONE != AST_FLAGS_IS_TOP_DECL);
static_assert(AST_FLAGS_NONE != AST_FLAGS_IS_CONSTANT);
static_assert(AST_FLAGS_NONE != AST_FLAGS_IS_EXTERNAL);

static_assert(AST_FLAGS_IS_TOP_DECL != AST_FLAGS_IS_CONSTANT);
static_assert(AST_FLAGS_IS_TOP_DECL != AST_FLAGS_IS_EXTERNAL);

static_assert(AST_FLAGS_IS_CONSTANT != AST_FLAGS_IS_EXTERNAL);

#define AST_NODES(X)        \
    X(AST_ERROR)            \
                            \
    X(AST_MODULE_DECL)      \
    X(AST_IMPORT_DECL)      \
                            \
    X(AST_PARAMETER)        \
    X(AST_FUNCTION_DECL)    \
                            \
    X(AST_FIELD)            \
    X(AST_STRUCT_DECL)      \
    X(AST_UNION_DECL)       \
                            \
    X(AST_VARIANT)          \
    X(AST_ENUM_DECL)        \
                            \
    X(AST_BLOCK)            \
                            \
    X(AST_DEFER_STMT)       \
    X(AST_RETURN_STMT)      \
                            \
    X(AST_SWITCH_STMT)      \
    X(AST_SWITCH_CASE)      \
                            \
    X(AST_BRANCH)           \
    X(AST_IF_STMT)          \
    X(AST_FOR_LOOP)         \
    X(AST_WHILE_LOOP)       \
                            \
    X(AST_BINARY_OP)        \
    X(AST_UNARY_OP)         \
    X(AST_FUNCTION_CALL)    \
    X(AST_IDENTIFIER)       \
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
} __attribute__((packed)) AstNodeKind;

static const char* AST_NODE_KIND_STRINGS[] = {
    AST_NODES(GENERATE_STRING)
};

#undef AST_NODES


typedef struct {
    NamespaceId id;
} AstImport, AstModule;


typedef struct {
    StringId name;
    AstNodeId type_expr;
} AstParameter;

typedef struct {
    AstNodeId id;
    AstNodeKind kind;
    u16 flags;

    union {
    } as;
} AstNode;

#endif // !LILY_AST_TYPES_H
