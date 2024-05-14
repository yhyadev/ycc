#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#include "ast.h"
#include "codegen.h"
#include "driver.h"
#include "parser.h"

void driver_compile(const char *source_file_path, const char *input) {
    Parser parser = parser_new(input);

    ASTRoot root = parser_parse_root(&parser);

    CodeGen gen = codegen_new(source_file_path);

    codegen_compile_root(&gen, root);

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    const char *target_triple = LLVMGetDefaultTargetTriple();

    LLVMTargetRef target;
    LLVMGetTargetFromTriple(target_triple, &target, NULL);

    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
        target, target_triple, "generic", "", LLVMCodeGenLevelDefault,
        LLVMRelocDefault, LLVMCodeModelDefault);

    LLVMTargetMachineEmitToFile(target_machine, gen.module, "a.obj",
                                LLVMObjectFile, NULL);

    codegen_free(gen);
    ast_root_free(root);
}
