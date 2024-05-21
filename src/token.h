#pragma once

#include <stddef.h>

typedef enum {
    TOK_EOF,
    TOK_INVALID,

    TOK_OPEN_PAREN,
    TOK_CLOSE_PAREN,
    TOK_OPEN_BRACE,
    TOK_CLOSE_BRACE,

    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_FORWARD_SLASH,
    TOK_BANG,
    TOK_ASSIGN,

    TOK_SEMICOLON,
    TOK_COLON,
    TOK_COMMA,

    TOK_IDENTIFIER,
    TOK_INT,
    TOK_FLOAT,

    TOK_KEYWORD_VOID,
    TOK_KEYWORD_CHAR,
    TOK_KEYWORD_SHORT,
    TOK_KEYWORD_INT,
    TOK_KEYWORD_LONG,
    TOK_KEYWORD_FLOAT,
    TOK_KEYWORD_DOUBLE,
    TOK_KEYWORD_RETURN,
} TokenKind;

typedef struct {
    size_t start;
    size_t end;
} BufferLoc;

typedef struct {
    TokenKind kind;
    BufferLoc loc;
} Token;
