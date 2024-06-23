#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "diagnostics.h"
#include "dynamic_array.h"
#include "symbol_table.h"

SymbolTable symbol_table_new() { return (SymbolTable){}; }

void symbol_table_set(SymbolTable *symbol_table, Symbol symbol) {
    for (size_t i = 0; i < symbol_table->symbols.count; i++) {
        if (strcmp(symbol_table->symbols.items[i].name.buffer,
                   symbol.name.buffer) == 0) {
            errorf(symbol.name.loc, "redifinition of '%s'",
                   symbol_table->symbols.items[i].name.buffer);

            exit(1);
        }
    }

    da_append(&symbol_table->symbols, symbol);
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

    exit(1);
}
