#include "ast/nodes/types.h"
#include "files/files.h"
#include "symbols/register/register.h"

void register_top_level_symbols_for_file(FileId id) {
    File* file = file_lookup_id(id); 
    Ast* ast = &file -> ast;

    for (u32 i = 0; i < ast -> count; i++) {
        AstNode* node = &ast -> nodes[i];

        if (!(node -> flags & AST_FLAGS_IS_TOP_DECL)) {
            continue; 
        }

        // register_symbol_from_node();
    }
}
