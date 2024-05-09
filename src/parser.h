#pragma once

#include "ast.h"
#include "lexer.h"

typedef struct {
    const char *buffer;
    Lexer lexer;
} Parser;

Parser parser_new(const char *buffer);
ASTRoot parser_parse_root(Parser *parser);
