#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "parse.h"

Token* token = NULL;
const char* input = NULL;

int main(int argc, char* argv[])
{
    if(argc != 2) {
        fprintf(stderr, "Error! Exactly 1 command line argument should be provided");
    }

    input = argv[1];
    token = tokenise(input);
    Node* node = expr();

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    gen(node);

    puts("  pop rax");
    puts("  ret");
    return EXIT_SUCCESS;
}

