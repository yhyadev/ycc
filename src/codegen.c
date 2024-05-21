#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

#include "arena.h"
#include "ast.h"
#include "codegen.h"
#include "diagnostics.h"
#include "process.h"
#include "symbol_table.h"
#include "type.h"

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

LLVMValueRef get_default_value(Type type) {
    switch (type.kind) {
    case TY_CHAR:
    case TY_SHORT:
    case TY_INT:
    case TY_LONG:
    case TY_LONG_LONG:
        return LLVMConstInt(get_llvm_type(type), 0, false);
    case TY_FLOAT:
    case TY_DOUBLE:
    case TY_LONG_DOUBLE:
        return LLVMConstReal(get_llvm_type(type), 0.0);
    default:
        assert(false && "unreachable");
    }
}

CodeGen codegen_new(Arena *arena, const char *source_file_path) {
    LLVMModuleRef module = LLVMModuleCreateWithName(source_file_path);
    LLVMSetSourceFileName(module, source_file_path, strlen(source_file_path));

    LLVMBuilderRef builder = LLVMCreateBuilder();

    return (CodeGen){.arena = arena,
                     .module = module,
                     .builder = builder,
                     .symbol_table = symbol_table_new(arena)};
}

Type codegen_infer_type(CodeGen *gen, ASTExpr expr) {
    Type type = {0};

    switch (expr.kind) {
    case EK_INT:
        type.kind = TY_LONG_LONG;
        break;

    case EK_FLOAT:
        type.kind = TY_LONG_DOUBLE;
        break;

    case EK_IDENTIFIER:
        type =
            symbol_table_lookup(&gen->symbol_table, expr.value.identifier.name)
                .type;
        break;

    case EK_UNARY_OPERATION:
        type = codegen_infer_type(gen, *expr.value.unary.rhs);
        break;

    case EK_BINARY_OPERATION: {
        Type lhs_type = codegen_infer_type(gen, *expr.value.binary.lhs);
        Type rhs_type = codegen_infer_type(gen, *expr.value.binary.rhs);

        return lhs_type.kind > rhs_type.kind ? lhs_type : rhs_type;
    }

    default:
        assert(false && "unreachable");
    }

    return type;
}

LLVMValueRef codegen_compile_expr(CodeGen *gen, LLVMTypeRef llvm_type,
                                  ASTExpr expr, bool constant_only) {
    switch (expr.kind) {
    case EK_INT:
        return LLVMConstInt(llvm_type, expr.value.intval, false);

    case EK_FLOAT:
        return LLVMConstReal(llvm_type, expr.value.floatval);

    case EK_IDENTIFIER: {
        if (constant_only) {
            errorf(expr.loc, "expected a constant expression only");

            process_exit(1);
        }

        Symbol symbol =
            symbol_table_lookup(&gen->symbol_table, expr.value.identifier.name);

        return LLVMBuildLoad2(gen->builder, get_llvm_type(symbol.type),
                              symbol.llvm_value_pointer, "");
    }

    case EK_UNARY_OPERATION: {
        LLVMValueRef rhs_value = codegen_compile_expr(
            gen, llvm_type, *expr.value.unary.rhs, constant_only);

        if (expr.value.unary.unary_operator == UO_MINUS) {
            return LLVMConstNeg(rhs_value);
        }

        if (expr.value.unary.unary_operator == UO_BANG) {
            return LLVMConstNot(rhs_value);
        }

        return LLVMConstNull(llvm_type);
    }

    case EK_BINARY_OPERATION: {
        LLVMValueRef lhs_value = codegen_compile_expr(
            gen, llvm_type, *expr.value.binary.lhs, constant_only);

        LLVMValueRef rhs_value = codegen_compile_expr(
            gen, llvm_type, *expr.value.binary.rhs, constant_only);

        if (expr.value.binary.binary_operator == BO_PLUS) {
            return LLVMConstAdd(lhs_value, rhs_value);
        }

        if (expr.value.binary.binary_operator == BO_MINUS) {
            return LLVMConstSub(lhs_value, rhs_value);
        }

        if (expr.value.binary.binary_operator == BO_STAR) {
            return LLVMConstMul(lhs_value, rhs_value);
        }

        if (expr.value.binary.binary_operator == BO_FORWARD_SLASH) {
            if (codegen_infer_type(gen, expr).kind < TY_LONG_DOUBLE) {
                return LLVMBuildUDiv(gen->builder, lhs_value, rhs_value, "");
            } else {
                return LLVMBuildFDiv(gen->builder, lhs_value, rhs_value, "");
            }
        }

        return LLVMConstNull(llvm_type);
    }

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

    if (expr.kind == EK_IDENTIFIER) {
        errorf(expr.loc,
               "casting non-constant expression is not implemented yet");

        process_exit(1);
    }

    if (expr.kind == EK_UNARY_OPERATION) {
        *expr.value.unary.rhs = codegen_cast_expr(type, *expr.value.unary.rhs);
    }

    if (expr.kind == EK_BINARY_OPERATION) {
        *expr.value.binary.lhs =
            codegen_cast_expr(type, *expr.value.binary.lhs);

        *expr.value.binary.rhs =
            codegen_cast_expr(type, *expr.value.binary.rhs);
    }

    return expr;
}

LLVMValueRef codegen_compile_and_cast_expr(CodeGen *gen, Type expected_type,
                                           Type original_type, ASTExpr expr,
                                           bool constant_only) {
    if (expected_type.kind == original_type.kind) {
        return codegen_compile_expr(gen, get_llvm_type(expected_type), expr,
                                    constant_only);
    } else {
        return codegen_compile_expr(gen, get_llvm_type(expected_type),
                                    codegen_cast_expr(expected_type, expr),
                                    constant_only);
    }
}

void codegen_compile_return_stmt(CodeGen *gen, ASTStmt stmt) {
    if (stmt.value.ret.none) {
        if (gen->context.function.prototype.return_type.kind != TY_VOID) {
            errorf(stmt.loc, "expected non-void return type");

            process_exit(1);
        }

        LLVMBuildRetVoid(gen->builder);
    } else {
        LLVMBuildRet(gen->builder,
                     codegen_compile_and_cast_expr(
                         gen, gen->context.function.prototype.return_type,
                         codegen_infer_type(gen, stmt.value.ret.value),
                         stmt.value.ret.value, false));
    }

    gen->context.function_returned = true;
}

void codegen_compile_variable(CodeGen *gen, ASTVariable ast_variable,
                              SymbolLinkage symbol_linkage) {
    if (ast_variable.type.kind == TY_VOID) {
        errorf(ast_variable.name.loc,
               "a variable cannot have incomplete type 'void'");

        process_exit(1);
    }

    if (symbol_linkage == SL_GLOBAL) {
        LLVMValueRef global_variable =
            LLVMAddGlobal(gen->module, get_llvm_type(ast_variable.type),
                          ast_variable.name.buffer);

        if (ast_variable.default_initialized) {
            LLVMSetInitializer(global_variable,
                               get_default_value(ast_variable.type));
        } else {
            LLVMSetInitializer(global_variable,
                               codegen_compile_and_cast_expr(
                                   gen, ast_variable.type,
                                   codegen_infer_type(gen, ast_variable.value),
                                   ast_variable.value, true));
        }

        Symbol symbol = {.type = ast_variable.type,
                         .name = ast_variable.name,
                         .linkage = symbol_linkage,
                         .llvm_value_pointer = global_variable};

        symbol_table_set(&gen->symbol_table, symbol);
    } else {
        LLVMValueRef alloca_pointer =
            LLVMBuildAlloca(gen->builder, get_llvm_type(ast_variable.type),
                            ast_variable.name.buffer);

        if (ast_variable.default_initialized) {
            LLVMBuildStore(gen->builder, get_default_value(ast_variable.type),
                           alloca_pointer);
        } else {
            LLVMBuildStore(gen->builder,
                           codegen_compile_and_cast_expr(
                               gen, ast_variable.type,
                               codegen_infer_type(gen, ast_variable.value),
                               ast_variable.value, false),
                           alloca_pointer);
        }

        Symbol symbol = {.type = ast_variable.type,
                         .name = ast_variable.name,
                         .linkage = symbol_linkage,
                         .llvm_value_pointer = alloca_pointer};

        symbol_table_set(&gen->symbol_table, symbol);
    }
}

void codegen_compile_stmt(CodeGen *gen, ASTStmt stmt) {
    switch (stmt.kind) {
    case SK_RETURN:
        codegen_compile_return_stmt(gen, stmt);
        break;

    case SK_VARIABLE_DECLARATION:
        codegen_compile_variable(gen, stmt.value.variable_declaration,
                                 SL_LOCAL);
        break;

    default:
        assert(false && "unreachable");
    }
}

typedef struct {
    LLVMTypeRef *items;
    size_t count;
    size_t capacity;
} LLVMTypes;

void codegen_compile_function(CodeGen *gen, ASTFunction ast_function) {
    LLVMTypeRef return_type = get_llvm_type(ast_function.prototype.return_type);
    LLVMTypes parameter_types = {0};

    for (size_t i = 0; i < ast_function.prototype.parameters.count; i++) {
        arena_da_append(
            gen->arena, &parameter_types,
            get_llvm_type(
                ast_function.prototype.parameters.items[i].expected_type));
    }

    LLVMTypeRef function_type = LLVMFunctionType(
        return_type, parameter_types.items, parameter_types.count,
        ast_function.prototype.parameters.variable);

    LLVMValueRef function = LLVMAddFunction(
        gen->module, ast_function.prototype.name.buffer, function_type);

    if (!ast_function.prototype.definition) {
        return;
    }

    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(function, "entry");

    LLVMPositionBuilderAtEnd(gen->builder, entry_block);

    gen->context.function = ast_function;
    gen->context.function_returned = false;

    for (size_t i = 0; i < ast_function.body.count; i++) {
        codegen_compile_stmt(gen, ast_function.body.items[i]);
    }

    if (!gen->context.function_returned) {
        if (gen->context.function.prototype.return_type.kind == TY_VOID) {
            LLVMBuildRetVoid(gen->builder);
        } else {
            LLVMBuildRet(
                gen->builder,
                get_default_value(gen->context.function.prototype.return_type));
        }
    }

    symbol_table_reset(&gen->symbol_table);
}

void codegen_compile_declaration(CodeGen *gen, ASTDeclaration declaration) {
    switch (declaration.kind) {
    case DK_FUNCTION:
        codegen_compile_function(gen, declaration.value.function);
        break;

    case DK_VARIABLE:
        codegen_compile_variable(gen, declaration.value.variable, SL_GLOBAL);
        break;

    default:
        assert(false && "unreachable");
    }
}

void codegen_compile_root(CodeGen *gen, ASTRoot root) {
    for (size_t i = 0; i < root.declarations.count; i++) {
        codegen_compile_declaration(gen, root.declarations.items[i]);
    }
}
