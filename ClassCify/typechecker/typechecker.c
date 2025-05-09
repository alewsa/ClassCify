#include "typechecker.h"
#include "../parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Type definitions ---
typedef enum { TYPE_INT, TYPE_BOOLEAN, TYPE_VOID, TYPE_CLASS } TypeKind;

typedef struct {
    TypeKind kind;
    char *class_name;   // only for TYPE_CLASS
} Type;

// Simple linked list symbol‐table entries:
typedef struct VarEntry {
    char          *name;
    Type           type;
    struct VarEntry *next;
} VarEntry;

typedef struct {
    VarEntry *vars;
} SymTable;

// --- Symbol table helpers ---
static SymTable *create_table() {
    SymTable *t = malloc(sizeof *t);
    t->vars = NULL;
    return t;
}

static void free_table(SymTable *t) {
    VarEntry *e = t->vars;
    while (e) {
        VarEntry *nx = e->next;
        free(e->name);
        free(e);
        e = nx;
    }
    free(t);
}

static void add_variable(SymTable *t, const char *name, Type ty) {
    VarEntry *e = malloc(sizeof *e);
    e->name = strdup(name);
    e->type = ty;
    e->next = t->vars;
    t->vars = e;
}

static int lookup_variable(SymTable *t, const char *name, Type *out) {
    for (VarEntry *e = t->vars; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            *out = e->type;
            return 1;
        }
    }
    return 0;
}

// --- Error reporting ---
static void error(const char *msg, ASTNode *n) {
    fprintf(stderr, "Type error at node '%s': %s\n", n->label, msg);
    exit(EXIT_FAILURE);
}

// --- Type constructors ---
static Type make_type(TypeKind k, const char *cls) {
    Type t;
    t.kind = k;
    t.class_name = cls ? strdup(cls) : NULL;
    return t;
}

// Map a type‐name ASTNode to our Type
static Type astnode_to_type(ASTNode *n) {
    if      (strcmp(n->label, "Int")     == 0) return make_type(TYPE_INT,     NULL);
    else if (strcmp(n->label, "Boolean") == 0) return make_type(TYPE_BOOLEAN, NULL);
    else if (strcmp(n->label, "Void")    == 0) return make_type(TYPE_VOID,    NULL);
    else                                       return make_type(TYPE_CLASS,   n->label);
}

// Forward decls:
static Type  infer_exp (ASTNode *n, SymTable *tbl);
static void  typecheck_stmt(ASTNode *n, SymTable *tbl, Type ret_t);

// --- Expression inference ---
static Type infer_exp(ASTNode *n, SymTable *tbl) {
    // Integer literal?
    if (isdigit((unsigned char)n->label[0])) {
        return make_type(TYPE_INT, NULL);
    }
    // Variable reference?
    if (isalpha((unsigned char)n->label[0]) && !strstr(n->label, ":")) {
        Type t;
        if (lookup_variable(tbl, n->label, &t)) return t;
        error("Undefined variable", n);
    }
    // Arithmetic operators:
    if (!strcmp(n->label, "+") || !strcmp(n->label, "-") ||
        !strcmp(n->label, "*") || !strcmp(n->label, "/"))
    {
        Type A = infer_exp(n->kids[0], tbl);
        Type B = infer_exp(n->kids[1], tbl);
        if (A.kind!=TYPE_INT || B.kind!=TYPE_INT)
            error("Arithmetic requires Int", n);
        return make_type(TYPE_INT, NULL);
    }
    // Comparison:
    if (!strcmp(n->label, "<") || !strcmp(n->label, "==")) {
        Type A = infer_exp(n->kids[0], tbl);
        Type B = infer_exp(n->kids[1], tbl);
        if (A.kind!=TYPE_INT || B.kind!=TYPE_INT)
            error("Comparison requires Int", n);
        return make_type(TYPE_BOOLEAN, NULL);
    }
    // Println:
    if (!strcmp(n->label, "Println")) {
        Type A = infer_exp(n->kids[0], tbl);
        if (A.kind!=TYPE_INT)
            error("println expects Int", n);
        return make_type(TYPE_VOID, NULL);
    }

    error("Unsupported expression", n);
    return make_type(TYPE_VOID, NULL);
}

// --- Statement checking ---
static void typecheck_stmt(ASTNode *n, SymTable *tbl, Type ret_t) {
    if (!strcmp(n->label, "VarDec")) {
        Type ty = astnode_to_type(n->kids[0]);
        add_variable(tbl, n->kids[1]->label, ty);
        return;
    }
    if (!strcmp(n->label, "Assign")) {
        Type L;
        if (!lookup_variable(tbl, n->kids[0]->label, &L))
            error("Assign to undeclared var", n);
        Type R = infer_exp(n->kids[1], tbl);
        if (L.kind!=R.kind ||
           (L.kind==TYPE_CLASS && strcmp(L.class_name,R.class_name)))
            error("Type mismatch in assignment", n);
        return;
    }
    if (!strcmp(n->label, "If")) {
        Type C = infer_exp(n->kids[0], tbl);
        if (C.kind!=TYPE_BOOLEAN) error("If cond must be Boolean", n);
        typecheck_stmt(n->kids[1], tbl, ret_t);
        if (n->kid_count==3) typecheck_stmt(n->kids[2], tbl, ret_t);
        return;
    }
    if (!strcmp(n->label, "While")) {
        Type C = infer_exp(n->kids[0], tbl);
        if (C.kind!=TYPE_BOOLEAN) error("While cond must be Boolean", n);
        for (int i=1;i<n->kid_count;i++)
            typecheck_stmt(n->kids[i], tbl, ret_t);
        return;
    }
    if (!strcmp(n->label, "Return")) {
        if (n->kid_count==1) {
            Type R = infer_exp(n->kids[0], tbl);
            if (R.kind!=ret_t.kind) error("Return type mismatch", n);
        } else {
            if (ret_t.kind!=TYPE_VOID) error("Missing return value", n);
        }
        return;
    }
    if (!strcmp(n->label, "StmtList")) {
        for (int i=0;i<n->kid_count;i++)
            typecheck_stmt(n->kids[i], tbl, ret_t);
        return;
    }
    // other stmt‐types: skip or extend here
}

// --- Entry point ---
void typecheck_program(ASTNode *root) {
    for (int i=0;i<root->kid_count;i++) {
        ASTNode *c = root->kids[i];
        if (!strcmp(c->label, "StmtList")) {
            SymTable *tbl = create_table();
            Type void_t = make_type(TYPE_VOID, NULL);
            typecheck_stmt(c, tbl, void_t);
            free_table(tbl);
            printf("Type checking passed.\n");
            return;
        }
    }
    printf("No statements to typecheck.\n");
}
