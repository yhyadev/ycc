#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct Type Type;

typedef struct {
    Type *items;
    size_t count;
    size_t capacity;
} Types;

typedef struct {
    Type *return_type;
    Types parameters;
    bool variadic;
} FunctionPrototype;

typedef union {
    FunctionPrototype prototype;
} TypeData;

typedef enum {
    TY_VOID,
    TY_CHAR,
    TY_SHORT,
    TY_INT,
    TY_LONG,
    TY_LONG_LONG,
    TY_FLOAT,
    TY_DOUBLE,
    TY_LONG_DOUBLE,
    TY_FUNCTION,
} TypeKind;

struct Type {
    TypeKind kind;
    TypeData data;
};
