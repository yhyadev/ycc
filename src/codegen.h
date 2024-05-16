#pragma once

#include <llvm-c/Types.h>

#include "arena.h"
#include "ast.h"
#include "symbol_table.h"

typedef struct {
    ASTFunction function;
    bool  function_returned;
} CodeGenContext;

typedef struct {
    Arena *arena;
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    SymbolTable symbol_table;

    CodeGenContext context;
} CodeGen;

CodeGen codegen_new(Arena *arena, const char *source_file_path);
void codegen_compile_root(CodeGen *gen, ASTRoot root);
