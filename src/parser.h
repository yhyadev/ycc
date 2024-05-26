#pragma once

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "token.h"

typedef enum {
    PR_LOWEST,
    PR_SUM,
    PR_PRODUCT,
    PR_PREFIX,
    PR_CALL,
} Precedence;

Precedence precedence_from_token(TokenKind kind);

typedef struct {
    Arena *arena;
    const char *buffer;
    Lexer lexer;
} Parser;

Parser parser_new(Arena *arena, const char *buffer);
ASTRoot parser_parse_root(Parser *parser);
