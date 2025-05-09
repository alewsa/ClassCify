#include "../parser/parser.h"
#include "../tokenizer/tokenizer.h"
#include "typechecker.h"
#include <stdio.h>
#include <stdlib.h>

Tokenizer tokenizer;

int main(void)
{
    FILE *f = fopen("sample_typecheck_input.txt", "r");
    if (!f)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *src = malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);

    init_tokenizer(&tokenizer, src);
    ASTNode *ast = parse_program();

    typecheck_program(ast);

    free_ast(ast);
    free(src);
    return EXIT_SUCCESS;
}
