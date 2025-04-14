#include "tokenizer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

typedef struct
{
    const char *keyword;
    TokenKind kind;
} KeywordMap;

static KeywordMap reserved_keywords[] = {
    {"Int", TOKEN_INT}, {"Boolean", TOKEN_BOOL}, {"Void", TOKEN_VOID}, {"this", TOKEN_THIS}, {"true", TOKEN_TRUE}, {"false", TOKEN_FALSE}, {"new", TOKEN_NEW}, {"vardec", TOKEN_VARDEC}, {"while", TOKEN_WHILE}, {"break", TOKEN_BREAK}, {"println", TOKEN_PRINT}, {"if", TOKEN_IF}, {"return", TOKEN_RETURN}, {"init", TOKEN_INIT}, {"super", TOKEN_SUPER}, {"class", TOKEN_CLASS}, {"method", TOKEN_METHOD}, {"call", TOKEN_CALL}, {NULL, TOKEN_UNKNOWN}};

static Token make_token(TokenKind kind, const char *start, int length)
{
    Token token;
    token.kind = kind;
    token.value = (char *)malloc(length + 1);
    strncpy(token.value, start, length);
    token.value[length] = '\0';
    return token;
}

void free_token(Token *token)
{
    if (token->value)
        free(token->value);
    token->value = NULL;
}

void init_tokenizer(Tokenizer *tokenizer, const char *input)
{
    tokenizer->input = input;
    tokenizer->position = 0;
}

bool has_more_tokens(Tokenizer *tokenizer)
{
    return tokenizer->input[tokenizer->position] != '\0';
}

static void skip_whitespace(Tokenizer *tokenizer)
{
    while (isspace(tokenizer->input[tokenizer->position]))
    {
        tokenizer->position++;
    }
}

static TokenKind match_keyword(const char *start, int length)
{
    for (int i = 0; reserved_keywords[i].keyword != NULL; i++)
    {
        if (strncmp(start, reserved_keywords[i].keyword, length) == 0 &&
            strlen(reserved_keywords[i].keyword) == length)
        {
            return reserved_keywords[i].kind;
        }
    }
    return TOKEN_IDENTIFIER;
}

Token next_token(Tokenizer *tokenizer)
{
    skip_whitespace(tokenizer);
    const char *src = tokenizer->input + tokenizer->position;

    if (isalpha(*src))
    {
        const char *start = src;
        while (isalnum(*src))
            src++;
        int length = src - start;
        TokenKind kind = match_keyword(start, length);
        tokenizer->position += length;
        return make_token(kind, start, length);
    }
    else if (isdigit(*src))
    {
        const char *start = src;
        while (isdigit(*src))
            src++;
        int length = src - start;
        tokenizer->position += length;
        return make_token(TOKEN_INT_LITERAL, start, length);
    }
    else
    {
        switch (*src)
        {
        case '(':
            tokenizer->position++;
            return make_token(TOKEN_LPAREN, src, 1);
        case ')':
            tokenizer->position++;
            return make_token(TOKEN_RPAREN, src, 1);
        case '{':
            tokenizer->position++;
            return make_token(TOKEN_LBRACE, src, 1);
        case '}':
            tokenizer->position++;
            return make_token(TOKEN_RBRACE, src, 1);
        case '.':
            tokenizer->position++;
            return make_token(TOKEN_DOT, src, 1);
        case '+':
            tokenizer->position++;
            return make_token(TOKEN_PLUS, src, 1);
        case '-':
            tokenizer->position++;
            return make_token(TOKEN_MINUS, src, 1);
        case '*':
            tokenizer->position++;
            return make_token(TOKEN_MULT, src, 1);
        case '/':
            tokenizer->position++;
            return make_token(TOKEN_DIV, src, 1);
        case '<':
            tokenizer->position++;
            return make_token(TOKEN_LESSTHAN, src, 1);
        case ';':
            tokenizer->position++;
            return make_token(TOKEN_SEMICOLON, src, 1);
        case '=':
            if (*(src + 1) == '=')
            {
                tokenizer->position += 2;
                return make_token(TOKEN_EQUALS, src, 2);
            }
            else
            {
                tokenizer->position++;
                return make_token(TOKEN_SINGLE_EQUALS, src, 1);
            }
        default:
            tokenizer->position++;
            return make_token(TOKEN_UNKNOWN, src, 1);
        }
    }
}