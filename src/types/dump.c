#include "driver/types.h"
#include "string_interner/interner.h"
#include "string_interner/types.h"
#include "types/ty.h"

#include <stdio.h>

extern LilyCtx driver_ctx;

static void print_type_name(TypeTable* table, TypeId id)
{
    TypeEntry* type = &table -> entries[id];

    switch (type -> kind) {
    case TYPE_BASE:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
        StringEntry s = STRING_ID_LOOKUP(type -> name);
        printf("%.*s", s.str.length, s.str.pointer);
        break;

    case TYPE_POINTER:
        print_type_name(table, type -> as.pointer.base);
        printf("*");
        break;

    case TYPE_ARRAY:
        print_type_name(table, type -> as.array.element);
        printf("[%lu]", type -> as.array.length); 
        break;

    default:
        printf("<unknown>");
        break;
    }
}

void print_type_table(TypeTable* table)
{
    printf("========== TYPE TABLE ==========\n");
    printf("count = %u\n\n", table -> count);

    for (u32 i = 0; i < table -> count; i++) {
        TypeEntry* type = &table -> entries[i];
        StringEntry name = STRING_ID_LOOKUP(type -> name);

        printf("[%3u] ", i);
        print_type_name(table, (TypeId)i);
        printf("\n");

        printf("    kind          : %s\n", TYPE_KIND_STRINGS[type -> kind]);
        printf("    name          : %.*s\n", name.str.length, name.str.pointer);
        printf("    hash          : 0x%x\n", type -> hash);

        printf("    size          : %u\n", type -> size);
        printf("    align         : %u\n", type -> align);

        printf("    resolve_state : %u\n", type -> resolve_state);

        printf("    type_expr     : %u\n", type -> type_expr);

        printf("    declaration   : %u:%u\n", type -> declaration.module_id, type -> declaration.symbol_id);

        switch (type -> kind) {
            case TYPE_POINTER:
                printf("    base          : %u (", type -> as.pointer.base);
                print_type_name(table, type -> as.pointer.base);
                printf(")\n");
                break;

            case TYPE_ARRAY:
                printf("    element       : %u\n", type -> as.array.element);
                printf("    length        : %lu\n", type -> as.array.length);
                break;

            default:
                break;
        }

        printf("\n");
    }

    printf("================================\n");
}
