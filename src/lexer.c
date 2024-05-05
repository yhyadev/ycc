#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "lexer.h"
#include "token.h"

#define SET_TOKEN_KIND(k)                                                      \
    token.kind = k;                                                            \
    token.loc.end = lexer->position;

#define TOKENIZE_SINGLE_CHARACTER(c, k)                                        \
    case c:                                                                    \
        SET_TOKEN_KIND(k)                                                      \
        break;

Lexer lexer_new(const char *buffer) {
    Lexer lexer = {.buffer = buffer};
    return lexer;
}

bool lexer_is_eof(Lexer *lexer) {
    return lexer->position >= strlen(lexer->buffer);
}

void lexer_skip_whitespace(Lexer *lexer) {
    while (!lexer_is_eof(lexer) && isspace(lexer->buffer[lexer->position])) {
        lexer->position++;
    }
}

Token lexer_next_token(Lexer *lexer) {
    lexer_skip_whitespace(lexer);

    Token token = {.kind = TOK_EOF,
                   .loc = {
                       .start = lexer->position,
                       .end = lexer->position,
                   }};

    if (lexer_is_eof(lexer))
        return token;

    char ch = lexer->buffer[lexer->position++];

    switch (ch) {
        TOKENIZE_SINGLE_CHARACTER('(', TOK_OPEN_PAREN)
        TOKENIZE_SINGLE_CHARACTER(')', TOK_CLOSE_PAREN)
        TOKENIZE_SINGLE_CHARACTER('{', TOK_OPEN_BRACE)
        TOKENIZE_SINGLE_CHARACTER('}', TOK_CLOSE_BRACE)
    default:
        SET_TOKEN_KIND(TOK_INVALID)
        break;
    }

    return token;
}

void lexer_debug_token(Lexer *lexer) {
    Token token = lexer_next_token(lexer);

    printf("%zu-%zu: ", token.loc.start, token.loc.end);

    for (size_t i = token.loc.start; i < token.loc.end; i++) {
        printf("%c", lexer->buffer[i]);
    }

    printf(" (%d)\n", token.kind);
}

