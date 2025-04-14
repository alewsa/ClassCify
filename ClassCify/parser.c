#include "parser.h"
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AST Node Utility Functions
ASTNode *new_ast_node(ASTNodeType type, const char *value) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->children = NULL;
    node->child_count = 0;
    return node;
}

void add_child(ASTNode *parent, ASTNode *child) {
    parent->child_count++;
    parent->children = realloc(parent->children, sizeof(ASTNode*) * parent->child_count);
    parent->children[parent->child_count - 1] = child;
}

void free_ast(ASTNode *node) {
    if (!node)
        return;
    for (int i = 0; i < node->child_count; i++)
        free_ast(node->children[i]);
    free(node->children);
    free(node->value);
    free(node);
}

void print_ast(ASTNode *node, int indent) {
    if (!node)
        return;
    for (int i = 0; i < indent; i++)
        printf("  ");
    switch (node->type) {
        case AST_PROGRAM:    printf("Program"); break;
        case AST_CLASSDEF:   printf("ClassDef"); break;
        case AST_CONSTRUCTOR:printf("Constructor"); break;
        case AST_METHODDEF:  printf("MethodDef"); break;
        case AST_STMT:       printf("Stmt"); break;
        case AST_VARDEC:     printf("VarDec"); break;
        case AST_EXP:        printf("Exp"); break;
        default:             printf("Unknown"); break;
    }
    if (node->value)
        printf(" (%s)", node->value);
    printf("\n");
    for (int i = 0; i < node->child_count; i++)
        print_ast(node->children[i], indent + 1);
}

// Parser Globals and Helper Functions (using token text only)
// Global variables.
static Token current;
static Tokenizer *parser_tokenizer = NULL;

// advance() updates the global 'current' token.
static void advance() {
    current = next_token(parser_tokenizer);
}

// lookahead() returns the next token without changing the current position.
static Token lookahead() {
    int savedPos = parser_tokenizer->position;
    Token la = next_token(parser_tokenizer);
    parser_tokenizer->position = savedPos;
    return la;
}

// token_equals() returns true if token t's text equals string s.
static int token_equals(Token t, const char *s) {
    return strcmp(t.value, s) == 0;
}

// expect_value() verifies that the current token's text equals the given string; then advances.
static void expect_value(const char *s) {
    if (!token_equals(current, s)) {
        fprintf(stderr, "Error: expected token '%s' but got '%s'\n", s, current.value);
        exit(1);
    }
    advance();
}

// Forward Declarations
static ASTNode *parse_stmt(void);
static ASTNode *parse_exp(void);
static ASTNode *parse_vardec(void);

// Parsing Constructors and Method Definitions
// parse_constructor()
// Parses: ( init ( vardec* ) [ ( super exp* ) ] stmt* )
static ASTNode *parse_constructor(void) {
    expect_value("(");
    expect_value("init");
    // Parse constructor parameters.
    expect_value("(");
    ASTNode *params = new_ast_node(AST_EXP, "params");
    while (!token_equals(current, ")"))
        add_child(params, parse_vardec());
    expect_value(")"); // End of parameters.
    // Optionally, parse a super-call.
    ASTNode *superCall = NULL;
    if (token_equals(current, "(")) {
        Token la = lookahead();
        if (token_equals(la, "super")) {
            expect_value("(");
            expect_value("super");
            ASTNode *supArgs = new_ast_node(AST_EXP, "supArgs");
            while (!token_equals(current, ")"))
                add_child(supArgs, parse_exp());
            expect_value(")");
            superCall = supArgs;
        }
    }
    // Parse constructor body (one or more statements).
    ASTNode *body = new_ast_node(AST_EXP, "constructor_body");
    while (!token_equals(current, ")"))
        add_child(body, parse_stmt());
    expect_value(")"); 
    
    ASTNode *ctor = new_ast_node(AST_CONSTRUCTOR, "init");
    add_child(ctor, params);
    if (superCall)
        add_child(ctor, superCall);
    add_child(ctor, body);
    return ctor;
}

// parse_methoddef()
// Parses: ( method methodname ( vardec* ) type stmt* )
// The opening "(" is assumed to have been consumed by the class parser.
static ASTNode *parse_methoddef(void) {
    expect_value("method");
    // Next token is the method name.
    ASTNode *method = new_ast_node(AST_METHODDEF, current.value);
    advance();
    // Parse parameter list.
    expect_value("(");
    while (!token_equals(current, ")"))
        add_child(method, parse_vardec());
    expect_value(")"); // End of parameters.
    // Parse return type.
    ASTNode *retType = new_ast_node(AST_EXP, current.value);
    advance();
    add_child(method, retType);
    // Parse method body (one or more statements).
    while (!token_equals(current, ")"))
        add_child(method, parse_stmt());
    expect_value(")"); 
    return method;
}

// Parsing Expressions, Vardec, and Statements
// parse_exp()
// If the current token is not "(" then it’s an atomic expression; otherwise, a compound one.
static ASTNode *parse_exp(void) {
    if (!token_equals(current, "(")) {
        ASTNode *node = new_ast_node(AST_EXP, current.value);
        advance();
        return node;
    }
    expect_value("(");
    // Special case: (println exp)
    if (token_equals(current, "println")) {
        ASTNode *node = new_ast_node(AST_EXP, "println");
        advance(); // consume "println"
        ASTNode *arg = parse_exp();
        add_child(node, arg);
        expect_value(")");
        return node;
    }
    // Generic s-expression.
    ASTNode *node = new_ast_node(AST_EXP, "sexpr");
    while (!token_equals(current, ")"))
        add_child(node, parse_exp());
    expect_value(")");
    return node;
}

// parse_vardec()
// Parses: ( vardec type var )
static ASTNode *parse_vardec(void) {
    expect_value("(");
    expect_value("vardec");
    ASTNode *typeNode = new_ast_node(AST_EXP, current.value);
    advance();
    ASTNode *varNode = new_ast_node(AST_EXP, current.value);
    advance();
    expect_value(")");
    ASTNode *node = new_ast_node(AST_VARDEC, varNode->value);
    add_child(node, typeNode);
    free_ast(varNode);
    return node;
}

// parse_assignment()
// Parses: ( = var exp )
static ASTNode *parse_assignment(void) {
    expect_value("(");
    expect_value("=");
    ASTNode *varNode = new_ast_node(AST_EXP, current.value);
    advance();
    ASTNode *expNode = parse_exp();
    expect_value(")");
    ASTNode *node = new_ast_node(AST_STMT, "=");
    add_child(node, varNode);
    add_child(node, expNode);
    return node;
}

// parse_while()
// Parses: ( while exp stmt* )
static ASTNode *parse_while(void) {
    expect_value("(");
    expect_value("while");
    ASTNode *node = new_ast_node(AST_STMT, "while");
    ASTNode *cond = parse_exp();
    add_child(node, cond);
    while (!token_equals(current, ")"))
        add_child(node, parse_stmt());
    expect_value(")");
    return node;
}

// parse_if()
// Parses: ( if exp stmt [stmt] )
static ASTNode *parse_if(void) {
    expect_value("(");
    expect_value("if");
    ASTNode *node = new_ast_node(AST_STMT, "if");
    ASTNode *cond = parse_exp();
    add_child(node, cond);
    ASTNode *thenStmt = parse_stmt();
    add_child(node, thenStmt);
    // Optional else clause.
    if (!token_equals(current, ")"))
         add_child(node, parse_stmt());
    expect_value(")");
    return node;
}

// parse_return()
// Parses: ( return [exp] )
static ASTNode *parse_return(void) {
    expect_value("(");
    expect_value("return");
    ASTNode *node = new_ast_node(AST_STMT, "return");
    if (!token_equals(current, ")"))
         add_child(node, parse_exp());
    expect_value(")");
    return node;
}

// parse_stmt()
// A statement can be a vardec, assignment, while, if, return, or an expression.
static ASTNode *parse_stmt(void) {
    if (token_equals(current, "(")) {
        // Peek at the next token to decide the statement type.
        Token la = lookahead();
        if (token_equals(la, "vardec"))
            return parse_vardec();
        else if (token_equals(la, "="))
            return parse_assignment();
        else if (token_equals(la, "while"))
            return parse_while();
        else if (token_equals(la, "if"))
            return parse_if();
        else if (token_equals(la, "return"))
            return parse_return();
        else
            return parse_exp(); 
    } else {
         fprintf(stderr, "Error: unexpected token '%s' in statement\n", current.value);
         exit(1);
         return NULL;
    }
}

// parse_classdef()
// Parses: ( class classname [superclass] ( vardec* ) constructor methoddef* )
// The field declarations must be grouped in a single pair of parentheses.
static ASTNode *parse_classdef(void) {
    expect_value("(");
    expect_value("class");
    ASTNode *cls = new_ast_node(AST_CLASSDEF, current.value);
    advance();
    // Optional superclass.
    if (!token_equals(current, "(")) {
         add_child(cls, new_ast_node(AST_EXP, current.value));
         advance();
    }
    expect_value("(");
    while (!token_equals(current, ")"))
         add_child(cls, parse_vardec());
    expect_value(")"); 
    // Constructor.
    ASTNode *ctor = parse_constructor();
    add_child(cls, ctor);
    // Method definitions.
    while (token_equals(current, "(")) {
         Token la = lookahead();
         if (token_equals(la, "method")) {
              expect_value("("); 
              ASTNode *m = parse_methoddef();
              add_child(cls, m);
         } else {
              break;
         }
    }
    expect_value(")"); 
    return cls;
}

// parse_program()
// A program consists of zero or more class definitions followed by one or more statements.
ASTNode *parse_program(Tokenizer *tokenizer) {
    parser_tokenizer = tokenizer;
    advance(); 
    ASTNode *program = new_ast_node(AST_PROGRAM, "program");
    // Parse class definitions.
    while (token_equals(current, "(")) {
         Token la = lookahead();
         if (token_equals(la, "class"))
              add_child(program, parse_classdef());
         else
              break;
    }
    while (strlen(current.value) > 0 && has_more_tokens(parser_tokenizer))
         add_child(program, parse_stmt());
    return program;
}
