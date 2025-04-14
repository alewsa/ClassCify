#ifndef PARSER_H
#define PARSER_H

#include "tokenizer.h"

// AST node types.
typedef enum {
    AST_PROGRAM,
    AST_CLASSDEF,
    AST_CONSTRUCTOR,
    AST_METHODDEF,
    AST_STMT,
    AST_VARDEC,
    AST_EXP
} ASTNodeType;

// AST node structure.
typedef struct ASTNode {
    ASTNodeType type;            // Node kind.
    char *value;                 // For identifiers, literals, or keywords.
    struct ASTNode **children;   // Array of child node pointers.
    int child_count;             // Number of children.
} ASTNode;

// AST utility functions.
ASTNode *new_ast_node(ASTNodeType type, const char *value);
void add_child(ASTNode *parent, ASTNode *child);
void free_ast(ASTNode *node);
void print_ast(ASTNode *node, int indent);

ASTNode *parse_program(Tokenizer *tokenizer);

#endif // PARSER_H
