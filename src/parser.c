#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "ast.h"
#include "diagnostics.h"
#include "dynamic_string.h"
#include "lexer.h"
#include "parser.h"
#include "precedence.h"
#include "process.h"
#include "token.h"
#include "type.h"

Parser parser_new(Arena *arena, const char *buffer) {
    return (Parser){
        .arena = arena,
        .buffer = buffer,
        .lexer = lexer_new(buffer),
    };
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
        parser_eat_token(parser, TOK_KEYWORD_INT);

        type.kind = TY_SHORT;
        break;

    case TOK_KEYWORD_INT:
        type.kind = TY_INT;
        break;

    case TOK_KEYWORD_LONG:
        if (parser_peek_token(parser).kind == TOK_KEYWORD_LONG) {
            parser_next_token(parser);
            parser_eat_token(parser, TOK_KEYWORD_INT);

            type.kind = TY_LONG_LONG;
        } else if (parser_peek_token(parser).kind == TOK_KEYWORD_DOUBLE) {
            parser_next_token(parser);
            type.kind = TY_LONG_DOUBLE;
        } else {
            parser_eat_token(parser, TOK_KEYWORD_INT);

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

        process_exit(1);
    }

    return type;
}

Name parser_parse_name(Parser *parser) {
    if (parser_peek_token(parser).kind != TOK_IDENTIFIER) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected an identifier");

        process_exit(1);
    }

    Token identifier_token = parser_next_token(parser);

    DynamicString name_string = {0};

    for (size_t i = identifier_token.loc.start; i < identifier_token.loc.end;
         i++) {
        arena_da_append(parser->arena, &name_string, parser->buffer[i]);
    }

    arena_da_append(parser->arena, &name_string, '\0');

    return (Name){
        .buffer = name_string.items,
        .loc = buffer_loc_to_source_loc(parser->buffer, identifier_token.loc)};
}

ASTExpr parser_parse_expr(Parser *parser, Precedence precedence);

ASTUnaryOperation
parser_parse_unary_operation(Parser *parser, ASTUnaryOperator unary_operator) {
    Token unary_operator_token = parser_next_token(parser);

    ASTExpr rhs = parser_parse_expr(
        parser, precedence_from_token(unary_operator_token.kind));

    ASTExpr *rhs_on_heap = arena_memdup(parser->arena, &rhs, sizeof(ASTExpr));

    return (ASTUnaryOperation){.unary_operator = unary_operator,
                               .rhs = rhs_on_heap};
}

ASTExpr parser_parse_int_expression(Parser *parser) {
    Token int_token = parser_next_token(parser);
    SourceLoc loc = buffer_loc_to_source_loc(parser->buffer, int_token.loc);

    DynamicString int_string = {0};

    for (size_t i = int_token.loc.start; i < int_token.loc.end; i++) {
        arena_da_append(parser->arena, &int_string, parser->buffer[i]);
    }

    arena_da_append(parser->arena, &int_string, '\0');

    unsigned long long intval = atoll(int_string.items);

    if (errno == ERANGE) {
        errorf(loc, intval == LLONG_MAX
                        ? "integer constant is too big to represent in any "
                          "integer type"
                        : "integer constant is too small to represent in "
                          "any integer type");

        process_exit(1);
    }

    return (ASTExpr){.value = {.intval = intval}, .kind = EK_INT, .loc = loc};
}

ASTExpr parser_parse_float_expression(Parser *parser) {
    Token float_token = parser_next_token(parser);
    SourceLoc loc = buffer_loc_to_source_loc(parser->buffer, float_token.loc);

    DynamicString float_string = {0};

    for (size_t i = float_token.loc.start; i < float_token.loc.end; i++) {
        arena_da_append(parser->arena, &float_string, parser->buffer[i]);
    }

    arena_da_append(parser->arena, &float_string, '\0');

    long double floatval = strtold(float_string.items, NULL);

    if (errno == ERANGE) {
        errorf(loc, floatval == LDBL_MAX
                        ? "float constant is too big to represent in any "
                          "float type"
                        : "float constant is too small to represent in "
                          "any float type");

        process_exit(1);
    }

    return (ASTExpr){
        .value = {.floatval = floatval}, .kind = EK_FLOAT, .loc = loc};
}

ASTExpr parser_parse_identifier_expression(Parser *parser) {
    Name name = parser_parse_name(parser);

    return (ASTExpr){.value = {.identifier = {.name = name}},
                     .kind = EK_IDENTIFIER,
                     name.loc};
}

ASTExpr parser_parse_unary_expression(Parser *parser) {
    SourceLoc loc =
        buffer_loc_to_source_loc(parser->buffer, parser_peek_token(parser).loc);

    ASTExpr expr = {.kind = EK_UNARY_OPERATION, .loc = loc};

    switch (parser_peek_token(parser).kind) {
    case TOK_MINUS:
        expr.value.unary = parser_parse_unary_operation(parser, UO_MINUS);
        break;

    case TOK_BANG:
        expr.value.unary = parser_parse_unary_operation(parser, UO_BANG);
        break;

    case TOK_INT:
        expr = parser_parse_int_expression(parser);
        break;

    case TOK_FLOAT:
        expr = parser_parse_float_expression(parser);
        break;

    case TOK_IDENTIFIER:
        expr = parser_parse_identifier_expression(parser);
        break;

    default:
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected an expression");

        process_exit(1);
    }

    return expr;
}

ASTBinaryOperation
parser_parse_binary_operation(Parser *parser, ASTExpr lhs,
                              ASTBinaryOperator binary_operator) {
    Token binary_operator_token = parser_next_token(parser);

    ASTExpr rhs = parser_parse_expr(
        parser, precedence_from_token(binary_operator_token.kind));

    ASTExpr *lhs_on_heap = arena_memdup(parser->arena, &lhs, sizeof(ASTExpr));
    ASTExpr *rhs_on_heap = arena_memdup(parser->arena, &rhs, sizeof(ASTExpr));

    return (ASTBinaryOperation){.lhs = lhs_on_heap,
                                .binary_operator = binary_operator,
                                .rhs = rhs_on_heap};
}

ASTExpr parser_parse_binary_expression(Parser *parser, ASTExpr lhs) {
    SourceLoc loc =
        buffer_loc_to_source_loc(parser->buffer, parser_peek_token(parser).loc);

    ASTExpr expr = {.kind = EK_BINARY_OPERATION, .loc = loc};

    switch (parser_peek_token(parser).kind) {
    case TOK_PLUS:
        expr.value.binary = parser_parse_binary_operation(parser, lhs, BO_PLUS);
        break;

    case TOK_MINUS:
        expr.value.binary =
            parser_parse_binary_operation(parser, lhs, BO_MINUS);
        break;

    case TOK_STAR:
        expr.value.binary =
            parser_parse_binary_operation(parser, lhs, BO_STAR);
        break;

    case TOK_FORWARD_SLASH:
        expr.value.binary =
            parser_parse_binary_operation(parser, lhs, BO_FORWARD_SLASH);
        break;

    default:
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected an expression");

        process_exit(1);
    }

    return expr;
}

ASTExpr parser_parse_expr(Parser *parser, Precedence precedence) {
    ASTExpr lhs = parser_parse_unary_expression(parser);

    while (parser_peek_token(parser).kind != TOK_SEMICOLON &&
           precedence < precedence_from_token(parser_peek_token(parser).kind)) {
        lhs = parser_parse_binary_expression(parser, lhs);
    }

    return lhs;
}

ASTDeclaration parser_parse_variable_declaration(Parser *parser, Type type,
                                                 Name name) {
    ASTExpr value = {0};
    bool default_initialized = true;

    if (!parser_eat_token(parser, TOK_SEMICOLON)) {
        if (!parser_eat_token(parser, TOK_ASSIGN)) {
            errorf(buffer_loc_to_source_loc(parser->buffer,
                                            parser_peek_token(parser).loc),
                   "expected a ';' at the end of declaration");

            process_exit(1);
        }

        value = parser_parse_expr(parser, PR_LOWEST);
        default_initialized = false;

        if (!parser_eat_token(parser, TOK_SEMICOLON)) {
            errorf(buffer_loc_to_source_loc(parser->buffer,
                                            parser_peek_token(parser).loc),
                   "expected a ';' at the end of declaration");

            process_exit(1);
        }
    }

    ASTVariable variable = {.type = type,
                            .name = name,
                            .value = value,
                            .default_initialized = default_initialized};

    return (ASTDeclaration){
        .value = {.variable = variable}, .kind = DK_VARIABLE, .loc = name.loc};
}

ASTStmt parser_parse_return_stmt(Parser *parser) {
    SourceLoc loc =
        buffer_loc_to_source_loc(parser->buffer, parser_next_token(parser).loc);

    ASTExpr value = {0};
    bool none = true;

    if (parser_peek_token(parser).kind != TOK_SEMICOLON) {
        value = parser_parse_expr(parser, PR_LOWEST);
        none = false;
    }

    if (!parser_eat_token(parser, TOK_SEMICOLON)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a ';' at the end of statement");

        process_exit(1);
    }

    ASTReturn ret = {.value = value, .none = none};

    return (ASTStmt){.value = {.ret = ret}, .kind = SK_RETURN, .loc = loc};
}

ASTStmt parser_parse_stmt(Parser *parser) {
    switch (parser_peek_token(parser).kind) {
    case TOK_SEMICOLON:
        parser_next_token(parser);
        return parser_parse_stmt(parser);

    case TOK_KEYWORD_VOID:
    case TOK_KEYWORD_CHAR:
    case TOK_KEYWORD_SHORT:
    case TOK_KEYWORD_INT:
    case TOK_KEYWORD_LONG:
    case TOK_KEYWORD_FLOAT:
    case TOK_KEYWORD_DOUBLE: {
        Type type = parser_parse_type(parser);

        Name name = parser_parse_name(parser);

        ASTDeclaration declaration =
            parser_parse_variable_declaration(parser, type, name);

        return (ASTStmt){
            .value = {.variable_declaration = declaration.value.variable},
            .kind = SK_VARIABLE_DECLARATION,
            .loc = declaration.loc};
    }

    case TOK_KEYWORD_RETURN:
        return parser_parse_return_stmt(parser);

    default:
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a statement");

        process_exit(1);
    }
}

ASTFunctionParameter parser_parse_function_parameter(Parser *parser) {
    Type expected_type = parser_parse_type(parser);

    Name name = {0};

    if (expected_type.kind == TY_VOID &&
        parser_peek_token(parser).kind == TOK_IDENTIFIER) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "function parameter with incomplete type");

        process_exit(1);
    } else if (expected_type.kind != TY_VOID) {
        name = parser_parse_name(parser);
    }

    return (ASTFunctionParameter){.expected_type = expected_type, .name = name};
}

ASTFunctionParameters parser_parse_function_parameters(Parser *parser) {
    if (!parser_eat_token(parser, TOK_OPEN_PAREN)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a '('");

        process_exit(1);
    }

    ASTFunctionParameters parameters = {.variable = true};

    while (parser_peek_token(parser).kind != TOK_EOF &&
           parser_peek_token(parser).kind != TOK_CLOSE_PAREN) {
        SourceLoc parameter_type_loc = buffer_loc_to_source_loc(
            parser->buffer, parser_peek_token(parser).loc);

        ASTFunctionParameter parameter =
            parser_parse_function_parameter(parser);

        if (parameter.expected_type.kind == TY_VOID) {
            if (!parameters.variable) {
                errorf(parameter_type_loc,
                       "'void' must be the first and only parameter");

                process_exit(1);
            }
        } else {
            arena_da_append(parser->arena, &parameters, parameter);
        }

        parameters.variable = false;

        if (!parser_eat_token(parser, TOK_COMMA) &&
            parser_peek_token(parser).kind != TOK_CLOSE_PAREN) {
            errorf(buffer_loc_to_source_loc(parser->buffer,
                                            parser_peek_token(parser).loc),
                   "expected a ','");

            process_exit(1);
        }
    }

    if (!parser_eat_token(parser, TOK_CLOSE_PAREN)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a ')'");

        process_exit(1);
    }

    return parameters;
}

ASTStmts parser_parse_function_body(Parser *parser) {
    if (!parser_eat_token(parser, TOK_OPEN_BRACE)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a '{'");

        process_exit(1);
    }

    ASTStmts body = {0};

    while (parser_peek_token(parser).kind != TOK_EOF &&
           parser_peek_token(parser).kind != TOK_CLOSE_BRACE) {
        ASTStmt stmt = parser_parse_stmt(parser);
        arena_da_append(parser->arena, &body, stmt);
    }

    if (!parser_eat_token(parser, TOK_CLOSE_BRACE)) {
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a '}'");

        process_exit(1);
    }

    return body;
}

ASTDeclaration parser_parse_function_declaration(Parser *parser,
                                                 Type return_type, Name name) {
    ASTFunctionParameters parameters = parser_parse_function_parameters(parser);

    ASTFunctionPrototype prototype = {
        .return_type = return_type,
        .name = name,
        .parameters = parameters,
    };

    ASTStmts body = {0};

    if (!parser_eat_token(parser, TOK_SEMICOLON)) {
        prototype.definition = true;
        body = parser_parse_function_body(parser);
    }

    ASTFunction function = {.prototype = prototype, .body = body};

    return (ASTDeclaration){
        .value = {.function = function}, .kind = DK_FUNCTION, .loc = name.loc};
}

ASTDeclaration parser_parse_declaration(Parser *parser) {
    switch (parser_peek_token(parser).kind) {
    case TOK_KEYWORD_VOID:
    case TOK_KEYWORD_CHAR:
    case TOK_KEYWORD_SHORT:
    case TOK_KEYWORD_INT:
    case TOK_KEYWORD_LONG:
    case TOK_KEYWORD_FLOAT:
    case TOK_KEYWORD_DOUBLE: {
        Type type = parser_parse_type(parser);

        Name name = parser_parse_name(parser);

        if (parser_peek_token(parser).kind == TOK_SEMICOLON ||
            parser_peek_token(parser).kind == TOK_ASSIGN) {
            return parser_parse_variable_declaration(parser, type, name);
        } else if (parser_peek_token(parser).kind == TOK_OPEN_PAREN) {
            return parser_parse_function_declaration(parser, type, name);
        } else {
            errorf(buffer_loc_to_source_loc(parser->buffer,
                                            parser_peek_token(parser).loc),
                   "expected a ';' after top level declarator");

            process_exit(1);
        }
    }

    default:
        errorf(buffer_loc_to_source_loc(parser->buffer,
                                        parser_peek_token(parser).loc),
               "expected a top level declaration");

        process_exit(1);
    }
}

ASTRoot parser_parse_root(Parser *parser) {
    ASTRoot root = {0};

    while (parser_peek_token(parser).kind != TOK_EOF) {
        arena_da_append(parser->arena, &root.declarations,
                        parser_parse_declaration(parser));
    }

    return root;
}
