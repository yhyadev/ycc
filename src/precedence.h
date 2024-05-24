#include "token.h"

typedef enum {
    PR_LOWEST,
    PR_SUM,
    PR_PRODUCT,
    PR_PREFIX,
    PR_CALL,
} Precedence;

Precedence precedence_from_token(TokenKind kind);
