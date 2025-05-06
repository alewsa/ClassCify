#include <stdio.h>
#include <stdlib.h>
#include "tokenizer.h"

const char *get_token_name(TokenKind kind)
{
    switch (kind)
    {
    case TOKEN_INT:
        return "TOKEN_INT";
    case TOKEN_BOOL:
        return "TOKEN_BOOL";
    case TOKEN_VOID:
        return "TOKEN_VOID";
    case TOKEN_THIS:
        return "TOKEN_THIS";
    case TOKEN_TRUE:
        return "TOKEN_TRUE";
    case TOKEN_FALSE:
        return "TOKEN_FALSE";
    case TOKEN_NEW:
        return "TOKEN_NEW";
    case TOKEN_VARDEC:
        return "TOKEN_VARDEC";
    case TOKEN_WHILE:
        return "TOKEN_WHILE";
    case TOKEN_BREAK:
        return "TOKEN_BREAK";
    case TOKEN_PRINT:
        return "TOKEN_PRINT";
    case TOKEN_IF:
        return "TOKEN_IF";
    case TOKEN_RETURN:
        return "TOKEN_RETURN";
    case TOKEN_INIT:
        return "TOKEN_INIT";
    case TOKEN_SUPER:
        return "TOKEN_SUPER";
    case TOKEN_CLASS:
        return "TOKEN_CLASS";
    case TOKEN_METHOD:
        return "TOKEN_METHOD";
    case TOKEN_CALL:
        return "TOKEN_CALL";
    case TOKEN_LPAREN:
        return "TOKEN_LPAREN";
    case TOKEN_RPAREN:
        return "TOKEN_RPAREN";
    case TOKEN_LBRACE:
        return "TOKEN_LBRACE";
    case TOKEN_RBRACE:
        return "TOKEN_RBRACE";
    case TOKEN_DOT:
        return "TOKEN_DOT";
    case TOKEN_PLUS:
        return "TOKEN_PLUS";
    case TOKEN_MINUS:
        return "TOKEN_MINUS";
    case TOKEN_MULT:
        return "TOKEN_MULT";
    case TOKEN_DIV:
        return "TOKEN_DIV";
    case TOKEN_LESSTHAN:
        return "TOKEN_LESSTHAN";
    case TOKEN_EQUALS:
        return "TOKEN_EQUALS";
    case TOKEN_SINGLE_EQUALS:
        return "TOKEN_SINGLE_EQUALS";
    case TOKEN_IDENTIFIER:
        return "TOKEN_IDENTIFIER";
    case TOKEN_INT_LITERAL:
        return "TOKEN_INT_LITERAL";
    case TOKEN_STRING_LITERAL:
        return "TOKEN_STRING_LITERAL";
    case TOKEN_SEMICOLON:
        return "TOKEN_SEMICOLON";
    case TOKEN_UNKNOWN:
        return "TOKEN_UNKNOWN";
    default:
        return "UNKNOWN_TOKEN_KIND";
    }
}

int main()
{
    FILE *file = fopen("test_input.txt", "r");
    if (!file)
    {
        perror("Failed to open test_input.txt");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    Tokenizer tokenizer;
    init_tokenizer(&tokenizer, buffer);

    while (has_more_tokens(&tokenizer))
    {
        Token token = next_token(&tokenizer);
        printf("%s: %s\n", get_token_name(token.kind), token.value);
        free_token(&token);
    }

    free(buffer);
    return 0;
}
