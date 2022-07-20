#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "parse.h"
#include "tokenise.h"

const char *input = NULL;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr,
            "Error! Exactly 1 command line argument should be provided");
    return EXIT_FAILURE;
  }

  input = argv[1];
  tokenise(input);
  parse();

  puts(".intel_syntax noprefix");
  puts(".globl main");
  puts("main:");
  puts("  push rbp");
  puts("  mov rbp, rsp");
  puts("  sub rsp, 208");

  gen();

  puts("  mov rsp, rbp");
  puts("  pop rbp");
  puts("  ret");
  return EXIT_SUCCESS;
}
