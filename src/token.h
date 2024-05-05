#pragma once

#include <stddef.h>

typedef enum {
    TOK_EOF,
    TOK_INVALID,
    TOK_OPEN_PAREN,
    TOK_CLOSE_PAREN,
    TOK_OPEN_BRACE,
    TOK_CLOSE_BRACE,
    TOK_IDENTIFIER,
    TOK_NUMBER,
} TokenKind;

typedef struct {
    size_t start;
    size_t end;
} BufferLoc;

typedef struct {
    TokenKind kind;
    BufferLoc loc;
} Token;
