#ifndef LILY_IDS_H
#define LILY_IDS_H

#include "utils/types.h"

typedef u32 AstNodeId;
typedef u32 FileId;
typedef u32 ModuleId;
typedef u32 NamespaceId;
typedef u32 ScopeId;
typedef u32 StringId;
typedef u32 SymbolId;

#define AST_NODE_ID_NONE    U32_MAX
#define FILE_ID_NONE        U32_MAX
#define MODULE_ID_NONE      U32_MAX
#define NAMESPACE_ID_NONE   U32_MAX
#define SCOPE_ID_NONE       U32_MAX
#define STRING_ID_NONE      U32_MAX
#define SYMBOL_ID_NONE      U32_MAX

#endif // !LILY_IDS_H
