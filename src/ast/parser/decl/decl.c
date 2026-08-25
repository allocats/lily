#include "ast/parser/decl/decl.h"
#include "ast/nodes/types.h"
#include "ast/parser/directive/directive.h"
#include "ast/parser/parser.h"
#include "ast/parser/recovery/recovery.h"
#include "ast/parser/recovery/types.h"
#include "ast/parser/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "token/types.h"

AstNodeId parse_top_level_decl(Parser* p) {
    FileId file_id = p -> current_file -> id;

    Token name_token = parser_peek(p);
    StringId name_id = string_intern_token(file_id, name_token);

    parser_advance(p); // advance past identifier
    parser_advance(p); // advance past '::'

    Token token = parser_advance(p);

    switch (token.kind) {
        case TOK_HASHTAG:
            return parse_directive(p, name_id);

        case TOK_KW_EXTERNAL:
            return parse_external_decl(p, name_id);

        case TOK_KW_FN:
            return parse_function_decl(p, name_id);

        case TOK_KW_ENUM:
            return parse_enum_decl(p, name_id);

        case TOK_KW_STRUCT:
            return parse_struct_decl(p, name_id);

        case TOK_KW_UNION:
            return parse_union_decl(p, name_id);
            break;

        default:
            diagnostic_add_token(
                file_id,
                DIAG_ERROR,
                &token,
                DIAG_LOC_WHOLE_TOK,
                "invalid top level declaration",
                "expected (fn | struct | enum | union | external | #directive) after '::'"
            );

            AstNodeId id = parser_create_node(p, AST_ERROR, AST_FLAGS_IS_TOP_DECL, 0);
            return parser_error(p, id, RECOVERY_DECL);
    }
}
