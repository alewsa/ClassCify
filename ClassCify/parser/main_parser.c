#include "parser.h"
#include "../tokenizer/tokenizer.h"
#include <stdio.h>
#include <stdlib.h>

extern Tokenizer tokenizer;

int main(void) {
    // Open the test file
    FILE *f = fopen("sample_text.txt", "r");
    if (!f) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    // Read entire contents into a buffer
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *src = malloc(size + 1);
    if (!src) {
        fprintf(stderr, "Out of memory!\n");
        fclose(f);
        return EXIT_FAILURE;
    }
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);

    // Tokenize and parse into an AST
    init_tokenizer(&tokenizer, src);
    ASTNode *ast = parse_program();

    // Print the AST
    printf("=== AST ===\n");
    print_ast(ast, 0);

    // Cleanup
    free_ast(ast);
    free(src);
    return EXIT_SUCCESS;
}
