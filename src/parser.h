#pragma once

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
    const char *buffer;
    Lexer lexer;
} Parser;

Parser parser_new( const char *buffer);
ASTRoot parser_parse_root(Parser *parser);
