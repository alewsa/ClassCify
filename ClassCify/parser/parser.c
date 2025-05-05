#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Global tokenizer and current token
Tokenizer tokenizer;
static Token current;

// AST helpers
ASTNode *new_node(const char *label) {
    ASTNode *n = malloc(sizeof(ASTNode));
    n->label     = strdup(label);
    n->kid_count = 0;
    n->kids      = NULL;
    return n;
}
void add_child(ASTNode *parent, ASTNode *child) {
    parent->kids = realloc(parent->kids, sizeof(ASTNode*) * (parent->kid_count + 1));
    parent->kids[parent->kid_count++] = child;
}
void free_ast(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->kid_count; i++) {
        free_ast(node->kids[i]);
    }
    free(node->kids);
    free(node->label);
    free(node);
}
void print_ast(ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) putchar(' ');
    printf("%s\n", node->label);
    for (int i = 0; i < node->kid_count; i++) {
        print_ast(node->kids[i], indent + 2);
    }
}

// Token handling
static void next_token_safe() {
    free_token(&current);
    if (has_more_tokens(&tokenizer)) {
        current = next_token(&tokenizer);
    } else {
        current.kind  = TOKEN_UNKNOWN;
        current.value = NULL;
    }
}
static void parse_error(const char *msg) {
    fprintf(stderr, "Parse error at token '%s': %s\n",
            current.value ? current.value : "(null)", msg);
    exit(EXIT_FAILURE);
}
static void expect(TokenKind kind, const char *what) {
    if (current.kind != kind) parse_error(what);
    next_token_safe();
}

// Forward declarations
ASTNode *parse_program();
ASTNode *parse_classdef();
ASTNode *parse_constructor();
ASTNode *parse_methoddef();
ASTNode *parse_stmt_list();
ASTNode *parse_stmt();
ASTNode *parse_vardec_stmt();
ASTNode *parse_exp();
ASTNode *parse_type();

// program ::= classdef* stmt+
ASTNode *parse_program() {
    next_token_safe();
    ASTNode *root = new_node("Program");

    // zero or more classdefs
    while (current.kind == TOKEN_LPAREN) {
        int save = tokenizer.position;
        Token tok = current;
        next_token_safe();
        if (current.kind == TOKEN_CLASS) {
            tokenizer.position = save;
            current            = tok;
            add_child(root, parse_classdef());
        } else {
            tokenizer.position = save;
            current            = tok;
            break;
        }
    }

    // at least one statement
    if (!(current.kind == TOKEN_LPAREN ||
          current.kind == TOKEN_VARDEC ||
          current.kind == TOKEN_BREAK))
        parse_error("Expected at least one statement");
    add_child(root, parse_stmt_list());

    // make sure we really are at EOF
    if (current.kind != TOKEN_UNKNOWN)
        parse_error("Extra tokens after program end");

    return root;
}

// classdef ::= ( class classname [superclass] (vardec*) constructor methoddef* )
ASTNode *parse_classdef() {
    //“( class”
    expect(TOKEN_LPAREN, "Expected '(' for classdef");
    expect(TOKEN_CLASS,  "Expected 'class'");
    ASTNode *n = new_node("ClassDef");

    //classname
    if (current.kind != TOKEN_IDENTIFIER)
        parse_error("Expected class name");
    add_child(n, new_node(current.value));
    next_token_safe();

    //optional superclass
    if (current.kind == TOKEN_IDENTIFIER) {
        add_child(n, new_node(current.value));
        next_token_safe();
    }

    //parse ONE group of fields: an outer “( … )” wrapping multiple vardecs
    expect(TOKEN_LPAREN, "Expected '(' before field declarations");
    //inside, each field begins with its own LPAREN
    while (current.kind == TOKEN_LPAREN) {
        add_child(n, parse_vardec_stmt());
    }
    expect(TOKEN_RPAREN, "Expected ')' after field declarations");

    //exactly one constructor
    add_child(n, parse_constructor());

    //zero or more methods
    while (current.kind == TOKEN_LPAREN) {
        int save_pos = tokenizer.position;
        Token save_tok = current;
        next_token_safe();
        if (current.kind == TOKEN_METHOD) {
            tokenizer.position = save_pos;
            current = save_tok;
            add_child(n, parse_methoddef());
        } else {
            tokenizer.position = save_pos;
            current = save_tok;
            break;
        }
    }

    //closing “)” of the class
    expect(TOKEN_RPAREN, "Expected ')' after classdef");
    return n;
}


// constructor ::= ( init (vardec*) [ (super exp*) ] stmt* )
ASTNode *parse_constructor() {
    expect(TOKEN_LPAREN, "Expected '(' for init");
    expect(TOKEN_INIT,   "Expected 'init'");
    ASTNode *n = new_node("Constructor");

    expect(TOKEN_LPAREN, "Expected '(' before init params");
    while (current.kind == TOKEN_VARDEC) {
        add_child(n, parse_vardec_stmt());
    }
    expect(TOKEN_RPAREN, "Expected ')' after init params");

    // optional super call
    if (current.kind == TOKEN_LPAREN) {
        int save_pos = tokenizer.position;
        Token save_tok = current;
        next_token_safe();
        if (current.kind == TOKEN_SUPER) {
            ASTNode *sup = new_node("SuperCall");
            next_token_safe();
            while (current.kind == TOKEN_LPAREN ||
                   current.kind == TOKEN_IDENTIFIER ||
                   current.kind == TOKEN_INT_LITERAL) {
                add_child(sup, parse_exp());
            }
            expect(TOKEN_RPAREN, "Expected ')' after super");
            add_child(n, sup);
        } else {
            tokenizer.position = save_pos;
            current = save_tok;
        }
    }

    // body stmts
    while (current.kind == TOKEN_LPAREN ||
           current.kind == TOKEN_VARDEC ||
           current.kind == TOKEN_BREAK) {
        add_child(n, parse_stmt());
    }
    expect(TOKEN_RPAREN, "Expected ')' after constructor");
    return n;
}

// methoddef ::= ( method methodname (vardec*) type stmt* )
ASTNode *parse_methoddef() {
    expect(TOKEN_LPAREN, "Expected '(' for method");
    expect(TOKEN_METHOD,"Expected 'method'");
    if (current.kind != TOKEN_IDENTIFIER) parse_error("Expected method name");
    ASTNode *n = new_node(current.value);
    next_token_safe();

    expect(TOKEN_LPAREN, "Expected '(' before method params");
    while (current.kind == TOKEN_VARDEC) {
        add_child(n, parse_vardec_stmt());
    }
    expect(TOKEN_RPAREN, "Expected ')' after method params");

    add_child(n, parse_type());

    while (current.kind == TOKEN_LPAREN ||
           current.kind == TOKEN_VARDEC ||
           current.kind == TOKEN_BREAK) {
        add_child(n, parse_stmt());
    }
    expect(TOKEN_RPAREN, "Expected ')' after method");
    return n;
}

// vardec ::= ( vardec type var )
ASTNode *parse_vardec_stmt() {
    expect(TOKEN_LPAREN, "Expected '(' for vardec");
    expect(TOKEN_VARDEC, "Expected 'vardec'");
    ASTNode *n = new_node("VarDec");
    add_child(n, parse_type());
    if (current.kind != TOKEN_IDENTIFIER) parse_error("Expected var name");
    add_child(n, new_node(current.value));
    next_token_safe();
    expect(TOKEN_RPAREN, "Expected ')' after vardec");
    return n;
}

// stmt_list ::= stmt+
ASTNode *parse_stmt_list() {
    ASTNode *n = new_node("StmtList");
    do {
        add_child(n, parse_stmt());
    } while (current.kind == TOKEN_LPAREN ||
             current.kind == TOKEN_VARDEC ||
             current.kind == TOKEN_BREAK);
    return n;
}

// stmt ::= (vardec Type var) | break
//        | (= var exp) | (while …) | (if …) | (return …)
//        | (call …) | (println …)
ASTNode *parse_stmt() {
    // QUICK LOOKAHEAD FOR A VARDENC STATEMENT
    if (current.kind == TOKEN_LPAREN) {
        int save_pos = tokenizer.position;
        Token save_tok = current;
        next_token_safe();               // consume '('
        if (current.kind == TOKEN_VARDEC) {
            // rollback, then parse it properly
            tokenizer.position = save_pos;
            current = save_tok;
            return parse_vardec_stmt();
        }
        // not a vardec; rewind for the normal LPAREN logic
        tokenizer.position = save_pos;
        current = save_tok;
    }

    // plain break
    if (current.kind == TOKEN_BREAK) {
        ASTNode *n = new_node("Break");
        next_token_safe();
        return n;
    }

    // everything else must start with '('
    if (current.kind != TOKEN_LPAREN) {
        parse_error("Unrecognized statement");
    }

    // enter parenthesized statement
    expect(TOKEN_LPAREN, "Expected '(' for statement");
    TokenKind k = current.kind;
    ASTNode *n = NULL;

    if (k == TOKEN_SINGLE_EQUALS) {
        next_token_safe();
        n = new_node("Assign");
        if (current.kind != TOKEN_IDENTIFIER)
            parse_error("Expected variable name after '='");
        add_child(n, new_node(current.value));
        next_token_safe();
        add_child(n, parse_exp());

    } else if (k == TOKEN_WHILE) {
        next_token_safe();
        n = new_node("While");
        add_child(n, parse_exp());
        // loop body
        while (current.kind == TOKEN_LPAREN ||
               current.kind == TOKEN_BREAK) {
            add_child(n, parse_stmt());
        }

    } else if (k == TOKEN_IF) {
        next_token_safe();
        n = new_node("If");
        add_child(n, parse_exp());
        add_child(n, parse_stmt());
        // optional else
        if (current.kind == TOKEN_LPAREN ||
            current.kind == TOKEN_BREAK) {
            add_child(n, parse_stmt());
        }

    } else if (k == TOKEN_RETURN) {
        next_token_safe();
        n = new_node("Return");
        if (current.kind != TOKEN_RPAREN) {
            add_child(n, parse_exp());
        }

    } else if (k == TOKEN_CALL) {
        next_token_safe();
        n = new_node("Call");
        add_child(n, parse_exp());  // receiver
        if (current.kind != TOKEN_IDENTIFIER)
            parse_error("Expected method name in call");
        add_child(n, new_node(current.value));
        next_token_safe();
        while (current.kind != TOKEN_RPAREN) {
            add_child(n, parse_exp());
        }

    } else if (k == TOKEN_PRINT) {
        next_token_safe();
        n = new_node("Println");
        add_child(n, parse_exp());

    } else {
        parse_error("Unknown statement form");
    }

    expect(TOKEN_RPAREN, "Expected ')' after statement");
    return n;
}


// exp ::= var | this | true | false | int | (println exp) | (op exp exp) | (call exp method exp*) | (new classname exp*)
ASTNode *parse_exp() {
    if (current.kind == TOKEN_IDENTIFIER) {
        ASTNode *n = new_node(current.value);
        next_token_safe();
        return n;
    } else if (current.kind == TOKEN_THIS) {
        ASTNode *n = new_node("this");
        next_token_safe();
        return n;
    } else if (current.kind == TOKEN_TRUE) {
        ASTNode *n = new_node("true");
        next_token_safe();
        return n;
    } else if (current.kind == TOKEN_FALSE) {
        ASTNode *n = new_node("false");
        next_token_safe();
        return n;
    } else if (current.kind == TOKEN_INT_LITERAL) {
        ASTNode *n = new_node(current.value);
        next_token_safe();
        return n;
    } else if (current.kind == TOKEN_LPAREN) {
        expect(TOKEN_LPAREN, "Expected '(' for expression");
        TokenKind k = current.kind;
        ASTNode *n = NULL;
        if (k == TOKEN_PRINT) {
            next_token_safe();
            n = new_node("Println");
            add_child(n, parse_exp());
        } else if (k == TOKEN_PLUS || k == TOKEN_MINUS ||
                   k == TOKEN_MULT || k == TOKEN_DIV ||
                   k == TOKEN_LESSTHAN || k == TOKEN_EQUALS) {
            char lbl[3] = {0};
            switch (k) {
                case TOKEN_PLUS:     lbl[0] = '+'; break;
                case TOKEN_MINUS:    lbl[0] = '-'; break;
                case TOKEN_MULT:     lbl[0] = '*'; break;
                case TOKEN_DIV:      lbl[0] = '/'; break;
                case TOKEN_LESSTHAN: lbl[0] = '<'; break;
                case TOKEN_EQUALS:   lbl[0] = '='; lbl[1] = '='; break;
                default: break;
            }
            next_token_safe();
            n = new_node(lbl);
            add_child(n, parse_exp());
            add_child(n, parse_exp());
        } else if (k == TOKEN_CALL) {
            next_token_safe();
            n = new_node("Call");
            add_child(n, parse_exp());
            if (current.kind != TOKEN_IDENTIFIER) parse_error("Expected method name in call expr");
            add_child(n, new_node(current.value));
            next_token_safe();
            while (current.kind != TOKEN_RPAREN) {
                add_child(n, parse_exp());
            }
        } else if (k == TOKEN_NEW) {
            next_token_safe();
            if (current.kind != TOKEN_IDENTIFIER) parse_error("Expected class name in new expr");
            n = new_node("New");
            add_child(n, new_node(current.value));
            next_token_safe();
            while (current.kind != TOKEN_RPAREN) {
                add_child(n, parse_exp());
            }
        } else {
            parse_error("Unknown expression form");
        }
        expect(TOKEN_RPAREN, "Expected ')' after expression");
        return n;
    } else {
        parse_error("Unrecognized expression");
    }
    return NULL;
}

// type ::= Int | Boolean | Void | classname
ASTNode *parse_type() {
    ASTNode *n = NULL;
    if (current.kind == TOKEN_INT) {
        n = new_node("Int");
    } else if (current.kind == TOKEN_BOOL) {
        n = new_node("Boolean");
    } else if (current.kind == TOKEN_VOID) {
        n = new_node("Void");
    } else if (current.kind == TOKEN_IDENTIFIER) {
        n = new_node(current.value);
    } else {
        parse_error("Expected type");
    }
    next_token_safe();
    return n;
}
