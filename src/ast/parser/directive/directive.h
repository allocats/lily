#ifndef LILY_AST_PARSER_DIRECTIVE_H
#define LILY_AST_PARSER_DIRECTIVE_H

#include "ast/parser/types.h"

// Important, this must be the first init function called that has string_intern_*() for now
// LUT relies on an assertion that the first strings in the interner are directives
void directive_ids_init();

AstNodeId parse_directive(Parser* p, StringId name_id); 

#endif // !LILY_AST_PARSER_DIRECTIVE_H
