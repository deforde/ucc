#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "parse.h"
#include "tokenise.h"

const char *file_path = NULL;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr,
            "error! exactly 1 command line argument should be provided\n");
    return EXIT_FAILURE;
  }

  file_path = argv[1];

  tokenise(file_path);
  parse();
  gen();

  return EXIT_SUCCESS;
}
