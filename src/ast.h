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

typedef struct {
    Name name;
} ASTIdentifier;

typedef struct {
    unsigned long long intval;
    long double floatval;
    ASTIdentifier identifier;
} ASTExprValue;

typedef enum {
    EK_INT,
    EK_FLOAT,
    EK_IDENTIFIER,
} ASTExprKind;

typedef struct {
    ASTExprValue value;
    ASTExprKind kind;
    SourceLoc loc;
} ASTExpr;

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
} ASTStmtKind;

typedef union {
    ASTReturn ret;
    ASTVariable variable_declaration;
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
    bool variable;

    ASTFunctionParameter *items;
    size_t count;
    size_t capacity;
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
