#include "token.h"
#include "precedence.h"

Precedence precedence_from_token(TokenKind kind) {
    switch (kind) {
    case TOK_PLUS:
    case TOK_MINUS:
        return PR_SUM;

    case TOK_STAR:
    case TOK_FORWARD_SLASH:
        return PR_PRODUCT;

    default:
        return PR_LOWEST;
    }
}
