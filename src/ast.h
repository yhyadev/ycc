#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "type.h"

typedef struct {
    size_t line;
    size_t column;
} SourceLoc;

typedef struct {
    char *buffer;
    SourceLoc loc;
} Name;

typedef struct ASTExpr ASTExpr;

typedef struct {
    Name name;
} ASTIdentifier;

typedef enum {
    UO_MINUS,
    UO_BANG,
} ASTUnaryOperator;

typedef struct {
    ASTUnaryOperator unary_operator;
    ASTExpr *rhs;
} ASTUnaryOperation;

typedef enum {
    BO_PLUS,
    BO_MINUS,
    BO_STAR,
    BO_FORWARD_SLASH,
} ASTBinaryOperator;

typedef struct {
    ASTExpr *lhs;
    ASTBinaryOperator binary_operator;
    ASTExpr *rhs;
} ASTBinaryOperation;

typedef struct {
    ASTExpr *items;
    size_t count;
    size_t capacity;
} ASTExprs;

typedef struct {
    ASTExpr *callable;
    ASTExprs arguments;
} ASTCall;

typedef struct {
    unsigned long long intval;
    long double floatval;
    ASTIdentifier identifier;
    ASTUnaryOperation unary;
    ASTBinaryOperation binary;
    ASTCall call;
} ASTExprValue;

typedef enum {
    EK_INT,
    EK_FLOAT,
    EK_IDENTIFIER,
    EK_UNARY_OPERATION,
    EK_BINARY_OPERATION,
    EK_CALL,
} ASTExprKind;

struct ASTExpr {
    ASTExprValue value;
    ASTExprKind kind;
    SourceLoc loc;
};

typedef struct {
    ASTExpr value;
    bool none;
} ASTReturn;

typedef struct {
    Type type;
    Name name;
    ASTExpr value;
    bool default_initialized;
} ASTVariable;

typedef enum {
    SK_RETURN,
    SK_VARIABLE_DECLARATION,
    SK_EXPR,
} ASTStmtKind;

typedef union {
    ASTReturn ret;
    ASTVariable variable_declaration;
    ASTExpr expr;
} ASTStmtValue;

typedef struct {
    ASTStmtValue value;
    ASTStmtKind kind;
    SourceLoc loc;
} ASTStmt;

typedef struct {
    ASTStmt *items;
    size_t count;
    size_t capacity;
} ASTStmts;

typedef struct {
    Type expected_type;
    Name name;
} ASTFunctionParameter;

typedef struct {
    ASTFunctionParameter *items;
    size_t count;
    size_t capacity;

    bool variadic;
} ASTFunctionParameters;

typedef struct {
    Type return_type;
    Name name;
    ASTFunctionParameters parameters;
    bool definition;
} ASTFunctionPrototype;

typedef struct {
    ASTFunctionPrototype prototype;
    ASTStmts body;
} ASTFunction;

typedef union {
    ASTFunction function;
    ASTVariable variable;
} ASTDeclarationValue;

typedef enum {
    DK_FUNCTION,
    DK_VARIABLE,
} ASTDeclarationKind;

typedef struct {
    ASTDeclarationValue value;
    ASTDeclarationKind kind;
    SourceLoc loc;
} ASTDeclaration;

typedef struct {
    ASTDeclaration *items;
    size_t count;
    size_t capacity;
} ASTDeclarations;

typedef struct {
    ASTDeclarations declarations;
} ASTRoot;
