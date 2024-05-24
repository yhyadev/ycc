#include "precedence.h"
#include "token.h"

Precedence precedence_from_token(TokenKind kind) {
    switch (kind) {
    case TOK_PLUS:
    case TOK_MINUS:
        return PR_SUM;

    case TOK_STAR:
    case TOK_FORWARD_SLASH:
        return PR_PRODUCT;

    case TOK_OPEN_PAREN:
        return PR_CALL;

    default:
        return PR_LOWEST;
    }
}
