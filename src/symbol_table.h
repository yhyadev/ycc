#pragma once

#include <llvm-c/Types.h>

#include <stddef.h>

#include "arena.h"
#include "ast.h"
#include "type.h"

typedef enum {
    SL_GLOBAL,
    SL_LOCAL,
} SymbolLinkage;

typedef struct {
    Type type;
    Name name;
    SymbolLinkage linkage;
    LLVMValueRef llvm_value;
} Symbol;

typedef struct {
    Symbol *items;
    size_t count;
    size_t capacity;
} Symbols;

typedef struct {
    Arena *arena;
    Symbols symbols;
} SymbolTable;

SymbolTable symbol_table_new(Arena *arena);
void symbol_table_set(SymbolTable *symbol_table, Symbol symbol);
void symbol_table_reset(SymbolTable *symbol_table);
Symbol symbol_table_lookup(SymbolTable *symbol_table, Name name);
