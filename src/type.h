#pragma once

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
} TypeKind;

typedef struct {
    TypeKind kind;
} Type;
