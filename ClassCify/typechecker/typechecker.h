#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "../parser/parser.h"

// Walks the AST rooted at ‘root’ and verifies all types.
// On a mismatch it will print an error and exit; otherwise,
// it prints “Type checking passed.”
void typecheck_program(ASTNode *root);

#endif // TYPECHECKER_H
