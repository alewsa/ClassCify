#ifndef PARSER_H
#define PARSER_H

#include "../tokenizer/tokenizer.h"

// AST node
typedef struct ASTNode {
    char *label;               // e.g. "ClassDef", "If", "Identifier:foo"
    struct ASTNode **kids;     // child nodes
    int kid_count;
} ASTNode;

// AST construction & traversal helpers
ASTNode *new_node(const char *label);
void     add_child(ASTNode *parent, ASTNode *child);
void     free_ast(ASTNode *node);
void     print_ast(ASTNode *node, int indent);

// Parsing entry points (each returns an AST subtree)
ASTNode *parse_program();
ASTNode *parse_classdef();
ASTNode *parse_constructor();
ASTNode *parse_methoddef();
ASTNode *parse_stmt_list();
ASTNode *parse_stmt();
ASTNode *parse_vardec_stmt();
ASTNode *parse_exp();
ASTNode *parse_type();

#endif // PARSER_H
