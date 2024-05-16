#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "lexer.h"
#include "token.h"

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

void lexer_skip_identifer(Lexer *lexer) {
    while (!lexer_is_eof(lexer) && (isalnum(lexer->buffer[lexer->position]) ||
                                    lexer->buffer[lexer->position] == '_')) {
        lexer->position++;
    }
}

bool lexer_skip_number(Lexer *lexer) {
    bool is_float = false;

    while (!lexer_is_eof(lexer) && (isdigit(lexer->buffer[lexer->position]) ||
                                    lexer->buffer[lexer->position] == '.')) {
        lexer->position++;

        if (lexer->buffer[lexer->position] == '.') {
            is_float = true;
        }
    }

    return is_float;
}

#define SET_TOKEN_KIND(k)                                                      \
    token.kind = k;                                                            \
    token.loc.end = lexer->position;

#define COMPARE_AND_SET_TOKEN_KIND(s, k)                                       \
    if ((token.loc.end - token.loc.start == strlen(s)) &&                      \
        strncmp(&lexer->buffer[token.loc.start], s, strlen(s)) == 0) {         \
        SET_TOKEN_KIND(k);                                                     \
    }

#define TOKENIZE_SINGLE_CHARACTER(c, k)                                        \
    case c:                                                                    \
        SET_TOKEN_KIND(k)                                                      \
        break;

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
        TOKENIZE_SINGLE_CHARACTER(';', TOK_SEMICOLON)
        TOKENIZE_SINGLE_CHARACTER(':', TOK_COLON)
        TOKENIZE_SINGLE_CHARACTER(',', TOK_COMMA)
        TOKENIZE_SINGLE_CHARACTER('=', TOK_ASSIGN)

    default:
        if (isalpha(ch) || ch == '_') {
            lexer_skip_identifer(lexer);
            SET_TOKEN_KIND(TOK_IDENTIFIER)
            COMPARE_AND_SET_TOKEN_KIND("void", TOK_KEYWORD_VOID)
            COMPARE_AND_SET_TOKEN_KIND("char", TOK_KEYWORD_CHAR)
            COMPARE_AND_SET_TOKEN_KIND("short", TOK_KEYWORD_SHORT)
            COMPARE_AND_SET_TOKEN_KIND("int", TOK_KEYWORD_INT)
            COMPARE_AND_SET_TOKEN_KIND("long", TOK_KEYWORD_LONG)
            COMPARE_AND_SET_TOKEN_KIND("float", TOK_KEYWORD_FLOAT)
            COMPARE_AND_SET_TOKEN_KIND("double", TOK_KEYWORD_DOUBLE)
            COMPARE_AND_SET_TOKEN_KIND("return", TOK_KEYWORD_RETURN)
        } else if (isdigit(ch)) {
            SET_TOKEN_KIND(lexer_skip_number(lexer) ? TOK_FLOAT : TOK_INT)
        } else {
            SET_TOKEN_KIND(TOK_INVALID)
        }

        break;
    }

    return token;
}
