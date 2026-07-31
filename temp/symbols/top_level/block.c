#include "symbols/symbols.h"
#include "ast/nodes/types.h"
#include "modules/modules.h"
#include "symbols/symbols.h"

void resolve_block(Resolver* r, AstNodeId block_id) {
    Module* module = MODULE_ID_LOOKUP_REF(r -> current_module_id);
    Ast* ast = &module -> ast;

    AstNode* block = &ast -> nodes[block_id];

    scope_enter(r);

    for (u32 i = 0; i < block -> as.block.stmt_count; i++) {
        resolve_stmt(r, block -> as.block.stmts[i]);
    }

    scope_exit(r);
}
