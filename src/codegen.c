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

    case EK_CALL: {
        Type callable_type = codegen_infer_type(gen, *expr.value.call.callable);

        if (callable_type.kind != TY_FUNCTION) {
            errorf(expr.loc, "expected a callable");

            process_exit(1);
        }

        type = *callable_type.data.prototype.return_type;

        break;
    }

    default:
        assert(false && "unreachable");
    }

    return type;
}

typedef struct {
    LLVMTypeRef *items;
    size_t count;
    size_t capacity;
} LLVMTypes;

typedef struct {
    LLVMTypeRef return_type;
    LLVMTypes parameters;
    bool variadic;
} LLVMFunctionPrototype;

LLVMTypeRef codegen_get_llvm_type(CodeGen *gen, Type type) {
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

    case TY_FUNCTION: {
        LLVMFunctionPrototype llvm_function_prototype = {
            .return_type =
                codegen_get_llvm_type(gen, *type.data.prototype.return_type),
            .variadic = type.data.prototype.variadic};

        for (size_t i = 0; i < type.data.prototype.parameters.count; i++) {
            arena_da_append(gen->arena, &llvm_function_prototype.parameters,
                            codegen_get_llvm_type(
                                gen, type.data.prototype.parameters.items[i]));
        }

        return LLVMFunctionType(llvm_function_prototype.return_type,
                                llvm_function_prototype.parameters.items,
                                llvm_function_prototype.parameters.count,
                                llvm_function_prototype.variadic);
    }

    default:
        assert(false && "unreachable");
    }
}

LLVMValueRef codegen_get_default_value(CodeGen *gen, Type type) {
    switch (type.kind) {
    case TY_CHAR:
    case TY_SHORT:
    case TY_INT:
    case TY_LONG:
    case TY_LONG_LONG:
        return LLVMConstInt(codegen_get_llvm_type(gen, type), 0, false);

    case TY_FLOAT:
    case TY_DOUBLE:
    case TY_LONG_DOUBLE:
        return LLVMConstReal(codegen_get_llvm_type(gen, type), 0.0);

    default:
        assert(false && "unreachable");
    }
}

LLVMValueRef codegen_cast_llvm_value(CodeGen *gen,
                                     LLVMTypeRef expected_llvm_type,
                                     Type original_type,
                                     LLVMValueRef llvm_value) {
    LLVMTypeKind expected_llvm_type_kind = LLVMGetTypeKind(expected_llvm_type);
    LLVMTypeKind original_llvm_type_kind =
        LLVMGetTypeKind(codegen_get_llvm_type(gen, original_type));

    if (original_llvm_type_kind != expected_llvm_type_kind) {
        if (original_type.kind >= TY_FLOAT &&
            expected_llvm_type_kind == LLVMIntegerTypeKind) {
            llvm_value = LLVMBuildFPToSI(gen->builder, llvm_value,
                                         expected_llvm_type, "");
        } else if (original_type.kind < TY_FLOAT &&
                   (expected_llvm_type_kind == LLVMDoubleTypeKind ||
                    expected_llvm_type_kind == LLVMFloatTypeKind)) {
            llvm_value = LLVMBuildSIToFP(gen->builder, llvm_value,
                                         expected_llvm_type, "");
        } else if (original_type.kind < TY_FLOAT &&
                   expected_llvm_type_kind == LLVMIntegerTypeKind) {
            llvm_value = LLVMBuildIntCast2(gen->builder, llvm_value,
                                           expected_llvm_type, true, "");
        } else if (original_type.kind >= TY_FLOAT &&
                   (expected_llvm_type_kind == LLVMDoubleTypeKind ||
                    expected_llvm_type_kind == LLVMFloatTypeKind)) {
            llvm_value = LLVMBuildFPCast(gen->builder, llvm_value,
                                         expected_llvm_type, "");
        }
    }

    return llvm_value;
}

LLVMValueRef codegen_compile_and_cast_expr(CodeGen *gen, Type expected_type,
                                           Type original_type, ASTExpr expr,
                                           bool constant_only);

typedef struct {
    LLVMValueRef *items;
    size_t count;
    size_t capacity;
} LLVMValues;

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

        LLVMValueRef symbol_value = {0};

        if (symbol.type.kind == TY_FUNCTION) {
            symbol_value = symbol.llvm_value;
        } else {
            symbol_value = LLVMBuildLoad2(
                gen->builder, codegen_get_llvm_type(gen, symbol.type),
                symbol.llvm_value, "");
        }

        return codegen_cast_llvm_value(gen, llvm_type, symbol.type,
                                       symbol_value);
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
            if (codegen_infer_type(gen, expr).kind < TY_FLOAT) {
                return LLVMBuildUDiv(gen->builder, lhs_value, rhs_value, "");
            } else {
                return LLVMBuildFDiv(gen->builder, lhs_value, rhs_value, "");
            }
        }

        return LLVMConstNull(llvm_type);
    }

    case EK_CALL: {
        if (constant_only) {
            errorf(expr.loc, "expected a constant expression only");

            process_exit(1);
        }

        Type callable_type = codegen_infer_type(gen, *expr.value.call.callable);

        if (callable_type.kind != TY_FUNCTION) {
            errorf(expr.loc, "expected a callable");

            process_exit(1);
        }

        if (expr.value.call.arguments.count !=
                callable_type.data.prototype.parameters.count &&
            !callable_type.data.prototype.variadic) {
            errorf(expr.loc, "expected %d %s got %d",
                   callable_type.data.prototype.parameters.count,
                   expr.value.call.arguments.count != 1 ? "arguments"
                                                        : "argument",
                   expr.value.call.arguments.count);

            process_exit(1);
        }

        LLVMTypeRef llvm_callable_type =
            codegen_get_llvm_type(gen, callable_type);

        LLVMValueRef llvm_callable_value = codegen_compile_expr(
            gen, llvm_callable_type, *expr.value.call.callable, constant_only);

        LLVMValues llvm_arguments = {0};

        for (size_t i = 0; i < expr.value.call.arguments.count; i++) {
            arena_da_append(
                gen->arena, &llvm_arguments,
                codegen_compile_and_cast_expr(
                    gen, callable_type.data.prototype.parameters.items[i],
                    codegen_infer_type(gen, expr.value.call.arguments.items[i]),
                    expr.value.call.arguments.items[i], constant_only));
        }

        return codegen_cast_llvm_value(
            gen, llvm_type, *callable_type.data.prototype.return_type,
            LLVMBuildCall2(gen->builder, llvm_callable_type,
                           llvm_callable_value, llvm_arguments.items,
                           llvm_arguments.count, ""));
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
        return codegen_compile_expr(gen,
                                    codegen_get_llvm_type(gen, expected_type),
                                    expr, constant_only);
    } else {
        return codegen_compile_expr(
            gen, codegen_get_llvm_type(gen, expected_type),
            codegen_cast_expr(expected_type, expr), constant_only);
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
        LLVMValueRef llvm_global_variable = LLVMAddGlobal(
            gen->module, codegen_get_llvm_type(gen, ast_variable.type),
            ast_variable.name.buffer);

        if (ast_variable.default_initialized) {
            LLVMSetInitializer(
                llvm_global_variable,
                codegen_get_default_value(gen, ast_variable.type));
        } else {
            LLVMSetInitializer(llvm_global_variable,
                               codegen_compile_and_cast_expr(
                                   gen, ast_variable.type,
                                   codegen_infer_type(gen, ast_variable.value),
                                   ast_variable.value, true));
        }

        Symbol symbol = {.type = ast_variable.type,
                         .name = ast_variable.name,
                         .linkage = symbol_linkage,
                         .llvm_value = llvm_global_variable};

        symbol_table_set(&gen->symbol_table, symbol);
    } else {
        LLVMValueRef llvm_alloca = LLVMBuildAlloca(
            gen->builder, codegen_get_llvm_type(gen, ast_variable.type),
            ast_variable.name.buffer);

        if (ast_variable.default_initialized) {
            LLVMBuildStore(gen->builder,
                           codegen_get_default_value(gen, ast_variable.type),
                           llvm_alloca);
        } else {
            LLVMBuildStore(gen->builder,
                           codegen_compile_and_cast_expr(
                               gen, ast_variable.type,
                               codegen_infer_type(gen, ast_variable.value),
                               ast_variable.value, false),
                           llvm_alloca);
        }

        Symbol symbol = {.type = ast_variable.type,
                         .name = ast_variable.name,
                         .linkage = symbol_linkage,
                         .llvm_value = llvm_alloca};

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

    case SK_EXPR:
        if (stmt.value.expr.kind == EK_CALL) {
            codegen_compile_expr(
                gen,
                codegen_get_llvm_type(gen,
                                      codegen_infer_type(gen, stmt.value.expr)),
                stmt.value.expr, false);
        } else {
            warnf(stmt.value.expr.loc,
                  "expression is not used, thus it will not be compiled");
        }

        break;

    default:
        assert(false && "unreachable");
    }
}

void codegen_compile_function(CodeGen *gen, ASTFunction ast_function) {
    if (strcmp(ast_function.prototype.name.buffer, "main") == 0 &&
        ast_function.prototype.return_type.kind != TY_INT) {
        warnf(ast_function.prototype.name.loc,
              "return type of 'main' is not 'int'");
    }

    Type *function_return_type_on_heap = arena_memdup(
        gen->arena, &ast_function.prototype.return_type, sizeof(Type));

    FunctionPrototype function_prototype = {
        .return_type = function_return_type_on_heap,
        .variadic = ast_function.prototype.parameters.variadic,
    };

    for (size_t i = 0; i < ast_function.prototype.parameters.count; i++) {
        arena_da_append(
            gen->arena, &function_prototype.parameters,
            ast_function.prototype.parameters.items[i].expected_type);
    }

    Type function_type = {.kind = TY_FUNCTION,
                          .data = {
                              .prototype = function_prototype,
                          }};

    LLVMValueRef llvm_function_value =
        LLVMAddFunction(gen->module, ast_function.prototype.name.buffer,
                        codegen_get_llvm_type(gen, function_type));

    Symbol function_symbol = {.type = function_type,
                              .name = ast_function.prototype.name,
                              .linkage = SL_GLOBAL,
                              .llvm_value = llvm_function_value};

    symbol_table_set(&gen->symbol_table, function_symbol);

    if (!ast_function.prototype.definition) {
        return;
    }

    LLVMBasicBlockRef entry_block =
        LLVMAppendBasicBlock(llvm_function_value, "entry");

    LLVMPositionBuilderAtEnd(gen->builder, entry_block);

    gen->context.function = ast_function;
    gen->context.function_returned = false;

    for (size_t i = 0; i < ast_function.prototype.parameters.count; i++) {
        LLVMValueRef llvm_alloca = LLVMBuildAlloca(
            gen->builder,
            codegen_get_llvm_type(
                gen, ast_function.prototype.parameters.items[i].expected_type),
            ast_function.prototype.parameters.items[i].name.buffer);

        LLVMBuildStore(gen->builder, LLVMGetParam(llvm_function_value, i),
                       llvm_alloca);

        Symbol argument_symbol = {
            .type = ast_function.prototype.parameters.items[i].expected_type,
            .name = ast_function.prototype.parameters.items[i].name,
            .linkage = SL_LOCAL,
            .llvm_value = llvm_alloca};

        symbol_table_set(&gen->symbol_table, argument_symbol);
    }

    for (size_t i = 0; i < ast_function.body.count; i++) {
        codegen_compile_stmt(gen, ast_function.body.items[i]);
    }

    if (!gen->context.function_returned) {
        if (gen->context.function.prototype.return_type.kind == TY_VOID) {
            LLVMBuildRetVoid(gen->builder);
        } else {
            LLVMBuildRet(gen->builder,
                         codegen_get_default_value(
                             gen, gen->context.function.prototype.return_type));
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
