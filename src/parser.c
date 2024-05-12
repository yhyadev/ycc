#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "ast.h"
#include "diagnostics.h"
#include "dynamic_array.h"
#include "lexer.h"
#include "parser.h"
#include "token.h"
#include "type.h"

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} DynamicString;

Parser parser_new(const char *buffer) {
    Parser parser = {
        .buffer = buffer,
        .lexer = lexer_new(buffer),
    };

    return parser;
}

Token parser_next_token(Parser *parser) {
    return lexer_next_token(&parser->lexer);
}

Token parser_peek_token(Parser *parser) {
    Lexer lexer_copy = parser->lexer;
    return lexer_next_token(&lexer_copy);
}

bool parser_eat_token(Parser *parser, TokenKind kind) {
    if (parser_peek_token(parser).kind == kind) {
        parser_next_token(parser);

        return true;
    } else {
        return false;
    }
}

SourceLoc buffer_loc_to_source_loc(const char *buffer, BufferLoc buffer_loc) {
    SourceLoc source_loc = {1, 1};

    for (size_t i = 0; i < buffer_loc.start; i++) {
        if (buffer[i] == '\n') {
            source_loc.line++;
            source_loc.column = 0;
        }

        source_loc.column++;
    }

    return source_loc;
}

Type parser_parse_type(Parser *parser) {
    Token token = parser_next_token(parser);
    Type type = {0};

    switch (token.kind) {
    case TOK_KEYWORD_VOID:
        type.kind = TY_VOID;
        break;

    case TOK_KEYWORD_CHAR:
        type.kind = TY_CHAR;
        break;

    case TOK_KEYWORD_SHORT:
        type.kind = TY_SHORT;
        break;

    case TOK_KEYWORD_INT:
        type.kind = TY_INT;
        break;

    case TOK_KEYWORD_LONG:
        if (parser_peek_token(parser).kind == TOK_KEYWORD_LONG) {
            parser_next_token(parser);
            type.kind = TY_LONG_LONG;
        } else if (parser_peek_token(parser).kind == TOK_KEYWORD_DOUBLE) {
            parser_next_token(parser);
            type.kind = TY_LONG_DOUBLE;
        } else {
            type.kind = TY_LONG;
        }

        break;

    case TOK_KEYWORD_FLOAT:
        type.kind = TY_FLOAT;
        break;

    case TOK_KEYWORD_DOUBLE:
        type.kind = TY_DOUBLE;
        break;

    default:
        errorf(buffer_loc_to_source_loc(parser->buffer, token.loc),
               "unkown type");

        exit(1);
    }

    return type;
}

Name parser_parse_name(Parser *parser) {
    if (parser_peek_token(parser).kind != TOK_IDENTIFIER) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected an identifier");

        exit(1);
    }

    Token identifier_token = parser_next_token(parser);

    DynamicString name_string = {0};

    for (size_t i = identifier_token.loc.start; i < identifier_token.loc.end; i++) {
        da_append(&name_string, parser->buffer[i]);
    }

    da_append(&name_string, '\0');

    Name name = {
        .buffer = name_string.items,
        .loc = buffer_loc_to_source_loc(parser->buffer, identifier_token.loc)};

    return name;
}

ASTFunctionParameter parser_parse_function_parameter(Parser *parser) {
    Type expected_type = parser_parse_type(parser);
    Name name = parser_parse_name(parser);

    ASTFunctionParameter parameter = {.expected_type = expected_type,
                                      .name = name};

    return parameter;
}

ASTFunctionParameters parser_parse_function_parameters(Parser *parser) {
    if (!parser_eat_token(parser, TOK_OPEN_PAREN)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a '('");

        exit(1);
    }

    ASTFunctionParameters parameters = {0};

    while (parser_peek_token(parser).kind != TOK_EOF &&
           parser_peek_token(parser).kind != TOK_CLOSE_PAREN) {
        da_append(&parameters, parser_parse_function_parameter(parser));

        if (!parser_eat_token(parser, TOK_COMMA)) {
            errorf(buffer_loc_to_source_loc(parser->buffer,
                                            parser_peek_token(parser).loc),
                   "expected a ','");

            exit(1);
        }
    }

    if (!parser_eat_token(parser, TOK_CLOSE_PAREN)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a ')'");

        exit(1);
    }

    return parameters;
}

ASTExpr parser_parse_expr(Parser *parser) {
    switch (parser_peek_token(parser).kind) {
    case TOK_INT: {
        Token int_token = parser_next_token(parser);
        SourceLoc loc = buffer_loc_to_source_loc(parser->buffer, int_token.loc);

        DynamicString int_string = {0};

        for (size_t i = int_token.loc.start; i < int_token.loc.end; i++) {
            da_append(&int_string, parser->buffer[i]);
        }

        da_append(&int_string, '\0');

        unsigned long long intval = atoll(int_string.items);

        da_free(int_string);

        if (errno == ERANGE) {
            errorf(loc, intval == LLONG_MAX
                            ? "integer constant is too big to represent in any "
                              "integer type"
                            : "integer constant is too small to represent in "
                              "any integer type");

            exit(1);
        }

        ASTExpr expr = {
            .value = {.intval = intval}, .kind = EK_INT, .loc = loc};

        return expr;
    }

    case TOK_FLOAT: {
        Token float_token = parser_next_token(parser);
        SourceLoc loc =
            buffer_loc_to_source_loc(parser->buffer, float_token.loc);

        DynamicString float_string = {0};

        for (size_t i = float_token.loc.start; i < float_token.loc.end; i++) {
            da_append(&float_string, parser->buffer[i]);
        }

        da_append(&float_string, '\0');

        long double floatval = strtold(float_string.items, NULL);

        da_free(float_string);

        if (errno == ERANGE) {
            errorf(loc, floatval == LDBL_MAX
                            ? "float constant is too big to represent in any "
                              "float type"
                            : "float constant is too small to represent in "
                              "any float type");

            exit(1);
        }

        ASTExpr expr = {
            .value = {.floatval = floatval}, .kind = EK_FLOAT, .loc = loc};

        return expr;
    }
    default:
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected an expression");

        exit(1);
    }
}

ASTStmt parser_parse_return_stmt(Parser *parser) {
    SourceLoc loc =
        buffer_loc_to_source_loc(parser->buffer, parser_next_token(parser).loc);

    ASTExpr value = {0};
    bool none = true;

    if (parser_peek_token(parser).kind != TOK_SEMICOLON) {
        value = parser_parse_expr(parser);
        none = false;
    }

    if (!parser_eat_token(parser, TOK_SEMICOLON)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a semicolon after statement");

        exit(1);
    }

    ASTReturn ret = {.value = value, .none = none};

    ASTStmt stmt = {.value = {.ret = ret}, .kind = SK_RETURN, .loc = loc};

    return stmt;
}

ASTStmt parser_parse_stmt(Parser *parser) {
    switch (parser_peek_token(parser).kind) {
    case TOK_SEMICOLON:
        parser_next_token(parser);
        return parser_parse_stmt(parser);

    case TOK_KEYWORD_RETURN:
        return parser_parse_return_stmt(parser);

    default:
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a statement");

        exit(1);
    }
}

ASTStmts parser_parse_function_body(Parser *parser) {
    if (!parser_eat_token(parser, TOK_OPEN_BRACE)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a '{'");

        exit(1);
    }

    ASTStmts body = {0};

    while (parser_peek_token(parser).kind != TOK_EOF &&
           parser_peek_token(parser).kind != TOK_CLOSE_BRACE) {
        da_append(&body, parser_parse_stmt(parser));
    }

    if (!parser_eat_token(parser, TOK_CLOSE_BRACE)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a '}'");

        exit(1);
    }

    return body;
}

ASTDeclaration parser_parse_function_declaration(Parser *parser) {
    Token return_type_token = parser_peek_token(parser);
    Type return_type = parser_parse_type(parser);

    Name name = parser_parse_name(parser);

    ASTFunctionParameters parameters = parser_parse_function_parameters(parser);

    ASTStmts body = parser_parse_function_body(parser);

    ASTFunction function = {.prototype =
                                {
                                    .return_type = return_type,
                                    .name = name,
                                    .parameters = parameters,
                                },
                            .body = body};

    ASTDeclaration declaration = {
        .value = {.function = function},
        .kind = ADK_FUNCTION,
        .loc = buffer_loc_to_source_loc(parser->buffer, return_type_token.loc)};

    return declaration;
}

ASTDeclaration parser_parse_declaration(Parser *parser) {
    switch (parser_peek_token(parser).kind) {
    case TOK_KEYWORD_VOID:
    case TOK_KEYWORD_CHAR:
    case TOK_KEYWORD_SHORT:
    case TOK_KEYWORD_INT:
    case TOK_KEYWORD_LONG:
    case TOK_KEYWORD_FLOAT:
    case TOK_KEYWORD_DOUBLE:
        return parser_parse_function_declaration(parser);

    default:
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a top level declaration");

        exit(1);
    }
}

ASTRoot parser_parse_root(Parser *parser) {
    ASTRoot root = {0};

    while (parser_peek_token(parser).kind != TOK_EOF) {
        da_append(&root.declarations, parser_parse_declaration(parser));
    }

    return root;
}
