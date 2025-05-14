#include "typechecker.h"
#include "../parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Type definitions
typedef enum { TYPE_INT, TYPE_BOOLEAN, TYPE_VOID, TYPE_CLASS } TypeKind;
typedef struct {
    TypeKind kind;
    char    *class_name;   // for TYPE_CLASS
} Type;

// Method/constructor signature
typedef struct {
    int     param_count;
    Type   *param_types;   // array of length param_count
    Type    return_type;   // for methods; for ctors, use TYPE_VOID
} MethodSig;

// Class inheritance environment
typedef struct ClassEntry {
    char *name;
    char *superclass;      // NULL if none
    struct ClassEntry *next;
} ClassEntry;

// Method signature environment
typedef struct MethodEntry {
    char     *class_name;
    char     *method_name; // method name or "<ctor>"
    MethodSig sig;
    struct MethodEntry *next;
} MethodEntry;

static ClassEntry  *class_list  = NULL;
static MethodEntry *method_list = NULL;

// Add a class to the list
static void register_class(const char *name, const char *superclass) {
    ClassEntry *e = malloc(sizeof *e);
    e->name       = strdup(name);
    e->superclass = superclass ? strdup(superclass) : NULL;
    e->next       = class_list;
    class_list    = e;
}

// Find a class by name
static ClassEntry *find_class(const char *name) {
    for (ClassEntry *e = class_list; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e;
    return NULL;
}

// Check if 'sub' is a subclass of 'super'
static int is_subclass(const char *sub, const char *super) {
    if (strcmp(sub, super) == 0) return 1;
    ClassEntry *e = find_class(sub);
    while (e && e->superclass) {
        if (strcmp(e->superclass, super) == 0) return 1;
        e = find_class(e->superclass);
    }
    return 0;
}

// Check subtype compatibility
static int is_subtype(Type sub, Type sup) {
    if (sub.kind != sup.kind) return 0;
    if (sub.kind == TYPE_CLASS)
        return is_subclass(sub.class_name, sup.class_name);
    return 1;
}

// Register a method or constructor signature
static void add_method_sig(const char *cls, const char *mname, MethodSig sig) {
    MethodEntry *e = malloc(sizeof *e);
    e->class_name  = strdup(cls);
    e->method_name = strdup(mname);
    e->sig         = sig;
    e->next        = method_list;
    method_list    = e;
}

// Find a method signature
static int find_method(const char *cls, const char *mname, MethodSig *out) {
    for (MethodEntry *e = method_list; e; e = e->next) {
        if (strcmp(e->class_name, cls) == 0
         && strcmp(e->method_name, mname) == 0) {
            *out = e->sig;
            return 1;
        }
    }
    return 0;
}

// Find a constructor signature
static int find_constructor(const char *cls, MethodSig *out) {
    return find_method(cls, "<ctor>", out);
}

// Symbol table for variables
typedef struct VarEntry {
    char *name;
    Type  type;
    struct VarEntry *next;
} VarEntry;

typedef struct {
    VarEntry *vars;
} SymTable;

// Create a new symbol table
static SymTable *create_table() {
    SymTable *t = malloc(sizeof *t);
    t->vars = NULL;
    return t;
}

// Free a symbol table
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

// Add a variable entry
static void add_variable(SymTable *t, const char *name, Type ty) {
    VarEntry *e = malloc(sizeof *e);
    e->name = strdup(name);
    e->type = ty;
    e->next = t->vars;
    t->vars = e;
}

// Lookup a variable's type
static int lookup_variable(SymTable *t, const char *name, Type *out) {
    for (VarEntry *e = t->vars; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            *out = e->type;
            return 1;
        }
    }
    return 0;
}

// Report a type error and exit
static void error(const char *msg, ASTNode *n) {
    fprintf(stderr, "Type error at '%s': %s\n", n->label, msg);
    exit(EXIT_FAILURE);
}

// Build a Type value
static Type make_type(TypeKind k, const char *cls) {
    Type t;
    t.kind       = k;
    t.class_name = cls ? strdup(cls) : NULL;
    return t;
}

// Convert ASTNode type to Type
static Type astnode_to_type(ASTNode *n) {
    if      (strcmp(n->label, "Int")     == 0) return make_type(TYPE_INT,     NULL);
    else if (strcmp(n->label, "Boolean") == 0) return make_type(TYPE_BOOLEAN, NULL);
    else if (strcmp(n->label, "Void")    == 0) return make_type(TYPE_VOID,    NULL);
    else                                       return make_type(TYPE_CLASS,   n->label);
}

// Forward declarations
static Type  infer_exp(ASTNode *n, SymTable *tbl);
static void  typecheck_stmt(ASTNode *n, SymTable *tbl, Type ret_t);
static void  typecheck_constructor(ASTNode *n, Type class_t);
static void  typecheck_method(ASTNode *n, Type class_t);
static void  typecheck_classdef(ASTNode *c);

// Expression type inference
static Type infer_exp(ASTNode *n, SymTable *tbl) {
    // this
    if (strcmp(n->label, "this") == 0) {
        Type t;
        if (lookup_variable(tbl, "this", &t)) return t;
        error("Unexpected 'this'", n);
    }
    // Int literal
    if (isdigit((unsigned char)n->label[0])) {
        return make_type(TYPE_INT, NULL);
    }
    // Boolean literal
    if (!strcmp(n->label, "true") || !strcmp(n->label, "false")) {
        return make_type(TYPE_BOOLEAN, NULL);
    }
    // ! operator
    if (!strcmp(n->label, "!")) {
        Type A = infer_exp(n->kids[0], tbl);
        if (A.kind != TYPE_BOOLEAN)
            error("Logical '!' requires Boolean", n);
        return make_type(TYPE_BOOLEAN, NULL);
    }
    // && and ||
    if (!strcmp(n->label, "&&") || !strcmp(n->label, "||")) {
        Type A = infer_exp(n->kids[0], tbl);
        Type B = infer_exp(n->kids[1], tbl);
        if (A.kind!=TYPE_BOOLEAN || B.kind!=TYPE_BOOLEAN)
            error("Logical '&&' and '||' require Boolean", n);
        return make_type(TYPE_BOOLEAN, NULL);
    }
    // Print/Println function
    if (!strcmp(n->label, "Print") || !strcmp(n->label, "Println")) {
        Type A = infer_exp(n->kids[0], tbl);
        if (A.kind != TYPE_INT)
            error("print expects Int", n);
        return make_type(TYPE_VOID, NULL);
    }
    // Arithmetic operators
    if (!strcmp(n->label, "+") || !strcmp(n->label, "-") ||
        !strcmp(n->label, "*") || !strcmp(n->label, "/")) {
        Type A = infer_exp(n->kids[0], tbl);
        Type B = infer_exp(n->kids[1], tbl);
        if (A.kind!=TYPE_INT || B.kind!=TYPE_INT)
            error("Arithmetic requires Int", n);
        return make_type(TYPE_INT, NULL);
    }
    // Comparison operators
    if (!strcmp(n->label, "<") || !strcmp(n->label, "==")) {
        Type A = infer_exp(n->kids[0], tbl);
        Type B = infer_exp(n->kids[1], tbl);
        if (A.kind!=TYPE_INT || B.kind!=TYPE_INT)
            error("Comparison requires Int", n);
        return make_type(TYPE_BOOLEAN, NULL);
    }
    // Method call
    if (!strcmp(n->label, "Call")) {
        Type recv = infer_exp(n->kids[0], tbl);
        if (recv.kind!=TYPE_CLASS)
            error("Call receiver must be class type", n);
        const char *mname = n->kids[1]->label;
        MethodSig sig;
        if (!find_method(recv.class_name, mname, &sig))
            error("Unknown method", n);
        int argc = n->kid_count - 2;
        if (argc != sig.param_count)
            error("Incorrect number of arguments", n);
        for (int i=0; i<argc; i++) {
            Type at = infer_exp(n->kids[2+i], tbl);
            if (!is_subtype(at, sig.param_types[i]))
                error("Argument type mismatch", n);
        }
        return sig.return_type;
    }
    // Object creation
    if (!strcmp(n->label, "New")) {
        const char *cls = n->kids[0]->label;
        if (!find_class(cls)) error("Unknown class", n);
        MethodSig ctor;
        if (!find_constructor(cls, &ctor))
            error("No matching constructor", n);
        int argc = n->kid_count - 1;
        if (argc != ctor.param_count)
            error("Wrong number of constructor args", n);
        for (int i=0; i<argc; i++) {
            Type at = infer_exp(n->kids[1+i], tbl);
            if (!is_subtype(at, ctor.param_types[i]))
                error("Constructor argument type mismatch", n);
        }
        return make_type(TYPE_CLASS, cls);
    }

    // Variable reference (leaf identifiers only)
    if (n->kid_count == 0 && isalpha((unsigned char)n->label[0])) {
        Type t;
        if (lookup_variable(tbl, n->label, &t)) return t;
        error("Undefined variable", n);
    }

    // nothing else matched
    error("Unsupported expression", n);
    return make_type(TYPE_VOID, NULL);
}

// Statement type checking
static void typecheck_stmt(ASTNode *n, SymTable *tbl, Type ret_t) {
    static int loop_depth = 0;
    // Variable declaration
    if (!strcmp(n->label, "VarDec")) {
        Type ty = astnode_to_type(n->kids[0]);
        add_variable(tbl, n->kids[1]->label, ty);
        return;
    }
    // Assignment
    if (!strcmp(n->label, "Assign")) {
        Type L;
        if (!lookup_variable(tbl, n->kids[0]->label, &L))
            error("Assign to undeclared var", n);
        Type R = infer_exp(n->kids[1], tbl);
        if (!is_subtype(R, L))
            error("Type mismatch in assignment", n);
        return;
    }
    // If statement
    if (!strcmp(n->label, "If")) {
        Type C = infer_exp(n->kids[0], tbl);
        if (C.kind!=TYPE_BOOLEAN) error("If cond must be Boolean", n);
        typecheck_stmt(n->kids[1], tbl, ret_t);
        if (n->kid_count==3) typecheck_stmt(n->kids[2], tbl, ret_t);
        return;
    }
    // While loop
    if (!strcmp(n->label, "While")) {
        Type C = infer_exp(n->kids[0], tbl);
        if (C.kind!=TYPE_BOOLEAN) error("While cond must be Boolean", n);
        loop_depth++;
        for (int i=1; i<n->kid_count; i++)
            typecheck_stmt(n->kids[i], tbl, ret_t);
        loop_depth--;
        return;
    }
    // Return statement
    if (!strcmp(n->label, "Return")) {
        if (n->kid_count==1) {
            Type R = infer_exp(n->kids[0], tbl);
            if (!is_subtype(R, ret_t)) error("Return type mismatch", n);
        } else {
            if (ret_t.kind!=TYPE_VOID) error("Missing return value", n);
        }
        return;
    }
    // Break statement
    if (!strcmp(n->label, "Break")) {
        if (loop_depth == 0) error("Break outside loop", n);
        return;
    }
    // Statement list
    if (!strcmp(n->label, "StmtList")) {
        for (int i=0; i<n->kid_count; i++)
            typecheck_stmt(n->kids[i], tbl, ret_t);
        return;
    }
    // Expression statement
    infer_exp(n, tbl);
}

// Constructor type checking
static void typecheck_constructor(ASTNode *n, Type class_t) {
    SymTable *tbl = create_table();
    add_variable(tbl, "this", class_t);
    Type void_t = make_type(TYPE_VOID, NULL);
    for (int i=0; i<n->kid_count; i++) {
        ASTNode *kid = n->kids[i];
        if (!strcmp(kid->label, "VarDec")) {
            Type ty = astnode_to_type(kid->kids[0]);
            add_variable(tbl, kid->kids[1]->label, ty);
        } else if (!strcmp(kid->label, "SuperCall")) {
            ClassEntry *ce = find_class(class_t.class_name);
            if (!ce || !ce->superclass)
                error("Super call in class with no superclass", kid);
            MethodSig super_ctor;
            if (!find_constructor(ce->superclass, &super_ctor))
                error("No matching super constructor", kid);
            if (kid->kid_count != super_ctor.param_count)
                error("Wrong number of arguments for super", kid);
            for (int j=0; j<kid->kid_count; j++) {
                Type arg_t = infer_exp(kid->kids[j], tbl);
                if (!is_subtype(arg_t, super_ctor.param_types[j]))
                    error("Super call argument type mismatch", kid);
            }
        } else {
            typecheck_stmt(kid, tbl, void_t);
        }
    }
    free_table(tbl);
}

// Method type checking
static void typecheck_method(ASTNode *n, Type class_t) {
    SymTable *tbl = create_table();
    add_variable(tbl, "this", class_t);
    int idx = 0;
    while (idx<n->kid_count && !strcmp(n->kids[idx]->label,"VarDec")) {
        ASTNode *p = n->kids[idx++];
        Type ty = astnode_to_type(p->kids[0]);
        add_variable(tbl, p->kids[1]->label, ty);
    }
    if (idx>=n->kid_count) error("Missing return type", n);
    Type ret_t = astnode_to_type(n->kids[idx++]);
    for (; idx<n->kid_count; idx++)
        typecheck_stmt(n->kids[idx], tbl, ret_t);
    free_table(tbl);
}

// Class definition checking
static void typecheck_classdef(ASTNode *c) {
    const char *cls = c->kids[0]->label;
    for (int i=1; i<c->kid_count; i++) {
        ASTNode *m = c->kids[i];
        if (!strcmp(m->label, "Constructor")) {
            int pc = 0;
            while (pc < m->kid_count && !strcmp(m->kids[pc]->label, "VarDec"))
                pc++;
            MethodSig sig;
            sig.param_count = pc;
            sig.param_types = malloc(pc * sizeof(Type));
            for (int j=0; j<pc; j++)
                sig.param_types[j] = astnode_to_type(m->kids[j]->kids[0]);
            sig.return_type = make_type(TYPE_VOID, NULL);
            add_method_sig(cls, "<ctor>", sig);
            typecheck_constructor(m, make_type(TYPE_CLASS, cls));
        } else if (!strcmp(m->label, "VarDec") || m->kid_count == 0) {
            // skip fields or standalone superclass
            continue;
        } else {
            int pc = 0;
            while (pc < m->kid_count && !strcmp(m->kids[pc]->label, "VarDec")) pc++;
            MethodSig sig;
            sig.param_count = pc;
            sig.param_types = malloc(pc * sizeof(Type));
            for (int j=0; j<pc; j++)
                sig.param_types[j] = astnode_to_type(m->kids[j]->kids[0]);
            sig.return_type = astnode_to_type(m->kids[pc]);
            add_method_sig(cls, m->label, sig);
            typecheck_method(m, make_type(TYPE_CLASS, cls));
        }
    }
}

// Program entry point
void typecheck_program(ASTNode *root) {
    int i = 0;
    // Register classes
    while (i < root->kid_count && !strcmp(root->kids[i]->label, "ClassDef")) {
        ASTNode *c = root->kids[i++];
        const char *cls = c->kids[0]->label;
        ASTNode *supNode = c->kids[1];
        const char *sup = NULL;
        if (strcmp(supNode->label, "VarDec") != 0 &&
            strcmp(supNode->label, "Constructor") != 0)
            sup = supNode->label;
        register_class(cls, sup);
    }
    // Typecheck classes
    for (int j = 0; j < i; j++)
        typecheck_classdef(root->kids[j]);

    // Check main statements
    SymTable *main_tbl = create_table();
    Type void_t = make_type(TYPE_VOID, NULL);

    bool found_stmts = false;
    while (i < root->kid_count) {
        if (!strcmp(root->kids[i]->label, "StmtList")) {
            found_stmts = true;
            typecheck_stmt(root->kids[i], main_tbl, void_t);
            break;
        }
        i++;
    }

    free_table(main_tbl);

    if (!found_stmts) {
        // no statements to typecheck
        printf("No statements to typecheck.\n");
    }

    printf("Type checking passed.\n");
}
