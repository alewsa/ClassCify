#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdbool.h>

// Define token types
typedef enum
{
    TOKEN_INT,
    TOKEN_BOOL,
    TOKEN_VOID,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NEW,
    TOKEN_VARDEC,
    TOKEN_WHILE,
    TOKEN_BREAK,
    TOKEN_PRINT,
    TOKEN_IF,
    TOKEN_RETURN,
    TOKEN_INIT,
    TOKEN_SUPER,
    TOKEN_CLASS,
    TOKEN_METHOD,
    TOKEN_CALL,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_DOT,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULT,
    TOKEN_DIV,
    TOKEN_LESSTHAN,
    TOKEN_EQUALS,
    TOKEN_SINGLE_EQUALS,
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_SEMICOLON,
    TOKEN_UNKNOWN
} TokenKind;

// Token structure
typedef struct
{
    TokenKind kind;
    char *value;
} Token;

// Tokenizer structure
typedef struct
{
    const char *input;
    int position;
} Tokenizer;

// Function declarations
void init_tokenizer(Tokenizer *tokenizer, const char *input);
Token next_token(Tokenizer *tokenizer);
bool has_more_tokens(Tokenizer *tokenizer);
void free_token(Token *token);

#endif // TOKENIZER_H