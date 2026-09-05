#include "driver/types.h"
#include "string_interner/interner.h"
#include "symbols/symbols/types.h"
#include "symbols/table/table.h"
#include "types/entries/types.h"
#include "types/table/types.h"

extern DriverCtx driver;

static void print_type_name(TypeTable* table, TypeId id) {
    TypeEntry* type = &table -> entries[id];

    switch (type -> kind) {
        case TYPE_BASE: {
            StringEntry s = STRING_ID_LOOKUP(type -> as.base_type.name);
            printf("%.*s", s.str.len, s.str.ptr);
            break;
        }

        case TYPE_STRUCT:
        case TYPE_UNION:
        case TYPE_ENUM:
        case TYPE_MODULE: {
            Symbol sym = SYMBOL_ID_LOOKUP(type -> symbol_id);
            StringEntry s = STRING_ID_LOOKUP(sym.name_id);
            printf("%.*s", s.str.len, s.str.ptr);
            break;
        }

        case TYPE_POINTER:
            print_type_name(table, type -> as.pointer_type.base);
            printf("*");
            break;

        case TYPE_ARRAY:
            print_type_name(table, type -> as.array_type.element);
            printf("[%u]", type -> as.array_type.size);
            break;

        case TYPE_SLICE:
            print_type_name(table, type -> as.slice_type.element);
            printf("[]");
            break;

        case TYPE_FUNCTION: {
            printf("fn(");
            for (u32 i = 0; i < type -> as.function_type.argument_count; i++) {
                if (i > 0) printf(", ");
                print_type_name(table, type -> as.function_type.arguments[i]);
            }
            printf(")  ->  ");
            print_type_name(table, type -> as.function_type.return_type);
            break;
        }

        default:
            printf("<unknown>");
            break;
    }
}

void print_type_table(TypeTable* table) {
    printf("========== TYPE TABLE ==========\n");
    printf("count = %u\n\n", table -> entry_count);

    for (u32 i = 0; i < table -> entry_count; i++) {
        TypeEntry* type = &table -> entries[i];

        printf("[%3u] ", i);
        print_type_name(table, (TypeId)i);
        printf("\n");

        printf("    kind          : %s\n", TYPE_KIND_STRINGS[type -> kind]);
        printf("    hash          : 0x%x\n", type -> hash);
        printf("    size          : %u\n", type -> size);
        printf("    alignment     : %u\n", type -> alignment);
        printf("    symbol_id     : %u\n", type -> symbol_id);

        switch (type -> kind) {
            case TYPE_POINTER:
                printf("    base          : %u (", type -> as.pointer_type.base);
                print_type_name(table, type -> as.pointer_type.base);
                printf(")\n");
                break;

            case TYPE_ARRAY:
                printf("    element       : %u\n", type -> as.array_type.element);
                printf("    size          : %u\n", type -> as.array_type.size);
                break;

            case TYPE_SLICE:
                printf("    element       : %u\n", type -> as.slice_type.element);
                break;

            case TYPE_STRUCT:
                printf("    field_count   : %u\n", type -> as.struct_type.field_count);
                printf("    symbol_id     : %u\n", type -> as.struct_type.symbol_id);
                for (u32 f = 0; f < type -> as.struct_type.field_count; f++) {
                    printf("      field[%u]    : %u\n", f, type -> as.struct_type.fields[f]);
                }
                break;

            case TYPE_UNION:
                printf("    field_count   : %u\n", type -> as.union_type.field_count);
                printf("    symbol_id     : %u\n", type -> as.union_type.symbol_id);
                for (u32 f = 0; f < type -> as.union_type.field_count; f++) {
                    printf("      field[%u]    : %u\n", f, type -> as.union_type.fields[f]);
                }
                break;

            case TYPE_ENUM:
                printf("    underlying    : %u\n", type -> as.enum_type.underlying_type);
                printf("    symbol_id     : %u\n", type -> as.enum_type.symbol_id);
                break;

            case TYPE_FUNCTION:
                printf("    argument_count: %u\n", type -> as.function_type.argument_count);
                for (u32 a = 0; a < type -> as.function_type.argument_count; a++) {
                    printf("      arg[%u]      : %u\n", a, type -> as.function_type.arguments[a]);
                }
                printf("    return_type   : %u\n", type -> as.function_type.return_type);
                break;

            case TYPE_MODULE:
                printf("    scope_id      : %u\n", type -> as.module_type.scope_id);
                printf("    symbol_id     : %u\n", type -> as.module_type.symbol_id);
                break;

            default:
                break;
        }

        printf("\n");
    }

    printf("================================\n");
}
