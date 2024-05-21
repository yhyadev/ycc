#include "token.h"

typedef enum {
    PR_LOWEST,
    PR_SUM,
    PR_PRODUCT,
    PR_PREFIX,
} Precedence;

Precedence precedence_from_token(TokenKind kind);
