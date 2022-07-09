#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    if(argc != 2) {
        fprintf(stderr, "Error! Exactly 1 command line argument should be provided");
        return EXIT_FAILURE;
    }

    char const* inp = argv[1];

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");
    printf("  mov rax, %ld\n", strtol(inp, (char**)&inp, 10));

    while(*inp) {
        if(*inp == '+') {
            ++inp;
            printf("  add rax, %ld\n", strtol(inp, (char**)&inp, 10));
            continue;
        }
        if(*inp == '-') {
            ++inp;
            printf("  sub rax, %ld\n", strtol(inp, (char**)&inp, 10));
            continue;
        }
        fprintf(stderr, "Error! Unrecognised operator: '%c'", *inp);
        return EXIT_FAILURE;
    }

    puts("  ret");

    return EXIT_SUCCESS;
}

