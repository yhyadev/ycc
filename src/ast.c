#include <assert.h>
#include <malloc.h>

#include "ast.h"
#include "dynamic_array.h"

void ast_function_free(ASTFunction function) {
    da_free(function.body);

    free(function.prototype.name.buffer);

    for (size_t i = 0; i < function.prototype.parameters.count; i++) {
        free(function.prototype.parameters.items[i].name.buffer);
    }

    da_free(function.prototype.parameters);
}

void ast_declaration_free(ASTDeclaration declaration) {
    switch (declaration.kind) {
    case ADK_FUNCTION:
        ast_function_free(declaration.value.function);
        break;
    default:
        assert(false && "unreachable");
    }
}

void ast_root_free(ASTRoot root) {
    for (size_t i = 0; i < root.declarations.count; i++) {
        ast_declaration_free(root.declarations.items[i]);
    }

    da_free(root.declarations);
}
