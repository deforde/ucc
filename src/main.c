#include <stdio.h>
#include <stdlib.h>

#include "parse.h"
#include "tokenise.h"

Token *token = NULL;
const char *input = NULL;
extern Node *code[];

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr,
            "Error! Exactly 1 command line argument should be provided");
    return EXIT_FAILURE;
  }

  input = argv[1];
  token = tokenise(input);
  program();

  puts(".intel_syntax noprefix");
  puts(".globl main");
  puts("main:");
  puts("  push rbp");
  puts("  mov rbp, rsp");
  puts("  sub rsp, 208");

  for (size_t i = 0; code[i]; ++i) {
    gen(code[i]);
    puts("  pop rax");
  }

  puts("  mov rsp, rbp");
  puts("  pop rbp");
  puts("  ret");
  return EXIT_SUCCESS;
}
