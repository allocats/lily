#include "driver/types.h"
#include "ids.h"
#include "resolver_stack/stack.h"
#include "resolver_stack/types.h"
#include "symbols/table/table.h"
#include "types/entries/entries.h"
#include "types/resolve/resolve.h"
#include "types/entries/types.h"
#include "types/table/table.h"

#include <assert.h>

extern DriverCtx driver;

static TypeId resolve_top_level_type_entry(TypeId id);

void resolve_top_level_types(void) {
    TypeTable* table = &driver.type_table;

    for (u32 i = 0; i < table -> entry_count; i++) {
        TypeEntry* entry = &table -> entries[i];

        if (type_family_lut[entry -> kind] != TYPE_FAMILY_NOMINAL) {
            continue;
        }

        resolve_top_level_type_entry(i);
    }
}

static TypeId resolve_top_level_type_entry(TypeId id) {
    assert(id != TYPE_ID_NONE);

    TypeEntry* entry = TYPE_ID_LOOKUP_REF(id);

    if (entry -> state == RESOLVE_RESOLVED) return id;
    if (entry -> state == RESOLVE_ERROR) return TYPE_ID_NONE;

    ResolveQuery query = {
        .kind = QUERY_TYPE,
        .as.type = id
    };

    if (entry -> state == RESOLVE_RESOLVING) {
        i32 stack_index = resolver_stack_find(query);

        // diagnostics
        if (stack_index == -1) {

        } else {

        }

        entry -> state = RESOLVE_ERROR;
        return TYPE_ID_NONE;
    }

    entry -> state = RESOLVE_RESOLVING;

    if (!resolver_stack_push(query)) {
        // error, stack is full

        entry -> state = RESOLVE_ERROR;

        return TYPE_ID_NONE;
    }

    resolver_stack_pop();

    return id;
}
