#include "ast/nodes/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "resolver_stack/types.h"
#include "symbols/register/types.h"
#include "symbols/scope/scope.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "types/builtins/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "symbols/register/register.h"

extern DriverCtx driver;

static void register_symbol(Registrar* r, AstNode* node);

static void register_import(Registrar* r, AstNode* node);

static inline SymbolId register_builtin_type(TypeId id, TypeBuiltin* type) {
    SymbolId symbol_id = scope_intern(COMPILER_SCOPE_ID, type -> name_id, SYMBOL_TYPE);
    Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

    symbol -> state = RESOLVE_RESOLVED;
    symbol -> as.type_symbol.type_id = id;

    return symbol_id;
}

inline void symbols_register_builtin_types(void) {
    for (u32 i = 0; i < BUILTIN_NOMINAL_TYPES_COUNT; i++) {
        TypeBuiltin* type = &BUILTIN_NOMINAL_TYPES[i];
        register_builtin_type(type -> id, type);
    }
}

ScopeId register_top_level_symbols_for_file(FileId id) {
    assert(id != FILE_ID_NONE);

    File* file = file_lookup_id(id); 

    debug_printf("Registering symbols for %.*s", file -> path.len, file -> path.ptr);

    if (file -> scope_id != SCOPE_ID_NONE) {
        return file -> scope_id;
    }

    Ast* ast = &file -> ast;

    Registrar registrar = {
        .file = file,
        .scope_id = COMPILER_SCOPE_ID
    };

    ScopeId file_scope_id = scope_enter(&registrar);

    file -> scope_id = file_scope_id;

    for (u32 i = 0; i < ast -> count; i++) {
        AstNode* node = &ast -> nodes[i];

        if (!(node -> flags & AST_FLAGS_IS_TOP_DECL)) {
            continue; 
        }

        register_symbol(&registrar, node);
    }

    scope_exit(&registrar);

    return file_scope_id; 
}

static void register_symbol(Registrar* r, AstNode* node) {
    switch (node -> kind) {
        case AST_IMPORT_DIRECTIVE:
            register_import(r, node);
            break;

        case AST_INCLUDE_DIRECTIVE:
            ScopeId scope_id = register_top_level_symbols_for_file(node -> as.include_directive.file_id);
            scope_merge(r -> scope_id, scope_id);
            break;

        case AST_FUNCTION_DECL:
            // register_function();
            break;

        case AST_STRUCT_DECL:
            // register_struct();
            break;

        case AST_UNION_DECL:
            // register_union();
            break;

        case AST_ENUM_DECL:
            // register_enum();
            break;

        case AST_VARIABLE_DECL:
            // register_varaible();
            break;

        default:
            UNREACHABLE("Found a non top level declaration node in register_symbol()");
    }
}

static inline void register_import(Registrar* r, AstNode* node) {
    ScopeId scope_id = register_top_level_symbols_for_file(node -> as.import_directive.file_id);

    StringId binding = node -> as.import_directive.binding;

    if (binding == STRING_ID_NONE) {
        scope_merge(r -> scope_id, scope_id);
    } else {
        SymbolId symbol_id = symbol_table_lookup(r -> scope_id, binding);

        if (symbol_id != SYMBOL_ID_NONE) {
            UNREACHABLE("TODO: diagnostics");
        }

        symbol_id = scope_intern_from_node(r -> scope_id, r -> file -> id, binding, node -> id);
        Symbol* symbol = SYMBOL_ID_LOOKUP_REF(symbol_id);

        symbol -> as.import_symbol.scope_id = scope_id;
    }
}
