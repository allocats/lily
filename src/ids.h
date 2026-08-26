#ifndef LILY_IDS_H
#define LILY_IDS_H

#include "utils/types.h"

typedef u32 AstNodeId;
typedef u32 FileId;
typedef u32 ModuleId;
typedef u32 ScopeId;
typedef u32 StringId;
typedef u32 SymbolId;
typedef u32 TypeId;

static constexpr u32 AST_NODE_ID_NONE = U32_MAX;
static constexpr u32 FILE_ID_NONE = U32_MAX;
static constexpr u32 MODULE_ID_NONE = U32_MAX;
static constexpr u32 SCOPE_ID_NONE = U32_MAX;
static constexpr u32 STRING_ID_NONE = U32_MAX;
static constexpr u32 SYMBOL_ID_NONE = U32_MAX;
static constexpr u32 TYPE_ID_NONE = U32_MAX;

#endif // !LILY_IDS_H
