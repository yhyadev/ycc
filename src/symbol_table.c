#include <stddef.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "diagnostics.h"
#include "process.h"
#include "symbol_table.h"

SymbolTable symbol_table_new(Arena *arena) {
    SymbolTable symbol_table = {.arena = arena};

    return symbol_table;
}

void symbol_table_set(SymbolTable *symbol_table, Symbol symbol) {
    for (size_t i = 0; i < symbol_table->symbols.count; i++) {
        if (strcmp(symbol_table->symbols.items[i].name.buffer,
                   symbol.name.buffer) == 0) {
            errorf(symbol.name.loc, "redifinition of '%s'",
                   symbol_table->symbols.items[i].name.buffer);

            process_exit(1);
        }
    }

    arena_da_append(symbol_table->arena, &symbol_table->symbols, symbol);
}

void symbol_table_reset(SymbolTable *symbol_table) {
    for (size_t i = 0; i < symbol_table->symbols.count; i++) {
        if (symbol_table->symbols.items[i].linkage != SL_GLOBAL) {
            symbol_table->symbols.items[i] =
                symbol_table->symbols.items[--symbol_table->symbols.count];
        }
    }
}

Symbol symbol_table_lookup(SymbolTable *symbol_table, Name name) {
    for (size_t i = 0; i < symbol_table->symbols.count; i++) {
        if (strcmp(symbol_table->symbols.items[i].name.buffer, name.buffer) ==
            0) {
            return symbol_table->symbols.items[i];
        }
    }

    errorf(name.loc, "undefined '%s'", name.buffer);

    process_exit(1);
}
