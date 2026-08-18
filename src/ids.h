#ifndef LILY_IDS_H
#define LILY_IDS_H

#include "utils/types.h"

typedef u32 AstNodeId;
typedef u32 FileId;
typedef u32 NamespaceId;
typedef u32 StringId;

static constexpr u32 AST_NODE_ID_NONE = U32_MAX;
static constexpr u32 FILE_ID_NONE = U32_MAX;
static constexpr u32 NAMESPACE_ID_NONE = U32_MAX;
static constexpr u32 STRING_ID_NONE = U32_MAX;

#endif // !LILY_IDS_H
