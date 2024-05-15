#pragma once

#include "arena.h"
#include "ast.h"
#include "lexer.h"

typedef struct {
    Arena *arena;
    const char *buffer;
    Lexer lexer;
} Parser;

Parser parser_new(Arena *arena, const char *buffer);
ASTRoot parser_parse_root(Parser *parser);
