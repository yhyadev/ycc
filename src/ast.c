#include "ast.h"
#include "dynamic_array.h"
#include <assert.h>

void ast_declaration_free(ASTDeclaration declaration) {
    switch (declaration.kind) {
    case ADK_FUNCTION:
        da_free(declaration.value.function.body);
        da_free(declaration.value.function.prototype.parameters);

        break;

    default:
        assert(0 && "unreachable");
        break;
    }
}

void ast_root_free(ASTRoot root) {
    for (size_t i = 0; i < root.declarations.count; i++) {
        ast_declaration_free(root.declarations.items[i]);
    }

    da_free(root.declarations);
}
