#pragma once

#include <stddef.h>

#include "token.h"

typedef struct {
    const char *buffer;
    size_t position;
} Lexer;

Lexer lexer_new(const char *buffer);
Token lexer_next_token(Lexer *lexer);
