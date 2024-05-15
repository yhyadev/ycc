#pragma once

#include <llvm-c/Types.h>

#include "arena.h"
#include "ast.h"

typedef struct {
    Arena *arena;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    ASTFunction function;
    bool  function_returned;
} CodeGen;

CodeGen codegen_new(Arena *arena, const char *source_file_path);
void codegen_compile_root(CodeGen *gen, ASTRoot root);
