#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

#include "ast.h"
#include "codegen.h"
#include "diagnostics.h"
#include "dynamic_array.h"
#include "type.h"

CodeGen codegen_new(const char *source_file_path) {
    LLVMModuleRef module = LLVMModuleCreateWithName(source_file_path);
    LLVMSetSourceFileName(module, source_file_path, strlen(source_file_path));

    LLVMBuilderRef builder = LLVMCreateBuilder();

    CodeGen gen = {.module = module, .builder = builder};

    return gen;
}

void codegen_free(CodeGen gen) {
    LLVMDisposeModule(gen.module);
    LLVMDisposeBuilder(gen.builder);
}

LLVMTypeRef get_llvm_type(Type type) {
    switch (type.kind) {
    case TY_VOID:
        return LLVMVoidType();
    case TY_CHAR:
        return LLVMInt8Type();
    case TY_SHORT:
        return LLVMInt16Type();
    case TY_INT:
        return LLVMInt32Type();
    case TY_LONG:
    case TY_LONG_LONG:
        return LLVMInt64Type();
    case TY_FLOAT:
        return LLVMFloatType();
    case TY_DOUBLE:
    case TY_LONG_DOUBLE:
        return LLVMDoubleType();
    default:
        assert(false && "unreachable");
    }
}

typedef struct {
    LLVMTypeRef *items;
    size_t count;
    size_t capacity;
} LLVMTypes;

Type infer_type(ASTExpr expr) {
    Type type = {0};

    switch (expr.kind) {
    case EK_INT:
        type.kind = TY_LONG_LONG;
        break;
    case EK_FLOAT:
        type.kind = TY_LONG_DOUBLE;
        break;
    default:
        assert(false && "unreachable");
    }

    return type;
}

LLVMValueRef codegen_compile_expr(LLVMTypeRef llvm_type, ASTExpr expr) {
    switch (expr.kind) {
    case EK_INT:
        return LLVMConstInt(llvm_type, expr.value.intval, false);
    case EK_FLOAT:
        return LLVMConstReal(llvm_type, expr.value.floatval);
    default:
        assert(false && "unreachable");
    }
}

#define COMPARE_AND_CAST_EXPRESSION_INT(dt, rt)                                \
    if (type.kind == dt && expr.kind == EK_INT) {                              \
        expr.value.intval = (rt)expr.value.intval;                             \
    }                                                                          \
                                                                               \
    if (type.kind == dt && expr.kind == EK_FLOAT) {                            \
        expr.kind = EK_INT;                                                    \
        expr.value.intval = (rt)expr.value.floatval;                           \
    }

#define COMPARE_AND_CAST_EXPRESSION_FLOAT(dt, rt)                              \
    if (type.kind == dt && expr.kind == EK_INT) {                              \
        expr.kind = EK_FLOAT;                                                  \
        expr.value.floatval = (rt)expr.value.intval;                           \
    }                                                                          \
                                                                               \
    if (type.kind == dt && expr.kind == EK_FLOAT) {                            \
        expr.value.floatval = (rt)expr.value.floatval;                         \
    }

ASTExpr codegen_cast_expr(Type type, ASTExpr expr) {
    COMPARE_AND_CAST_EXPRESSION_INT(TY_CHAR, char)
    COMPARE_AND_CAST_EXPRESSION_INT(TY_SHORT, short)
    COMPARE_AND_CAST_EXPRESSION_INT(TY_INT, int)
    COMPARE_AND_CAST_EXPRESSION_INT(TY_LONG, long)
    COMPARE_AND_CAST_EXPRESSION_INT(TY_LONG_LONG, long long)

    COMPARE_AND_CAST_EXPRESSION_FLOAT(TY_FLOAT, float)
    COMPARE_AND_CAST_EXPRESSION_FLOAT(TY_DOUBLE, double)
    COMPARE_AND_CAST_EXPRESSION_FLOAT(TY_LONG_DOUBLE, long double)

    return expr;
}

LLVMValueRef codegen_compile_and_cast_expr(Type expected_type,
                                           Type original_type, ASTExpr expr) {
    if (expected_type.kind == original_type.kind) {
        return codegen_compile_expr(get_llvm_type(expected_type), expr);
    } else {
        return codegen_compile_expr(get_llvm_type(expected_type),
                                    codegen_cast_expr(expected_type, expr));
    }
}

void codegen_compile_return_stmt(CodeGen *gen, ASTStmt stmt) {
    if (stmt.value.ret.none) {
        if (gen->current_function.prototype.return_type.kind != TY_VOID) {
            errorf(stmt.loc, "expected non void return type");
            exit(1);
        }

        LLVMBuildRetVoid(gen->builder);
    } else {
        LLVMBuildRet(gen->builder,
                     codegen_compile_and_cast_expr(
                         gen->current_function.prototype.return_type,
                         infer_type(stmt.value.ret.value),
                         stmt.value.ret.value));
    }
}

void codegen_compile_stmt(CodeGen *gen, ASTStmt stmt) {
    switch (stmt.kind) {
    case SK_RETURN:
        codegen_compile_return_stmt(gen, stmt);
        break;
    default:
        assert(false && "unreachable");
    }
}

void codegen_compile_function(CodeGen *gen, ASTFunction ast_function) {
    LLVMTypeRef return_type = get_llvm_type(ast_function.prototype.return_type);
    LLVMTypes parameter_types = {0};

    for (size_t i = 0; i < ast_function.prototype.parameters.count; i++) {
        da_append(
            &parameter_types,
            get_llvm_type(
                ast_function.prototype.parameters.items[i].expected_type));
    }

    LLVMTypeRef function_type = LLVMFunctionType(
        return_type, parameter_types.items, parameter_types.count,
        ast_function.prototype.parameters.variable);

    LLVMValueRef function = LLVMAddFunction(
        gen->module, ast_function.prototype.name.buffer, function_type);

    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(function, "entry");

    LLVMPositionBuilderAtEnd(gen->builder, entry_block);

    gen->current_function = ast_function;

    for (size_t i = 0; i < ast_function.body.count; i++) {
        codegen_compile_stmt(gen, ast_function.body.items[i]);
    }
}

void codegen_compile_declaration(CodeGen *gen, ASTDeclaration declaration) {
    switch (declaration.kind) {
    case ADK_FUNCTION:
        codegen_compile_function(gen, declaration.value.function);
        break;
    }
}

void codegen_compile_root(CodeGen *gen, ASTRoot root) {
    for (size_t i = 0; i < root.declarations.count; i++) {
        codegen_compile_declaration(gen, root.declarations.items[i]);
    }
}
