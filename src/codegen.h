#pragma once

#include <llvm-c/Types.h>

#include "ast.h"

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    ASTFunction current_function;
} CodeGen;

CodeGen codegen_new(const char *source_file_path);
void codegen_free(CodeGen gen);
void codegen_compile_root(CodeGen *gen, ASTRoot root);
