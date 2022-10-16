#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "parse.h"
#include "tokenise.h"

static char output_file_path[PATH_MAX] = {0};
static bool do_cc1 = false;
const char *input_file_path = NULL;
FILE *output = NULL;

static void usage(void);
static void parseArgs(int argc, char* argv[]);
static void cleanUp(void);

void usage(void) {
  puts(
      "Usage: ucc [options] file\n"
      "Options:\n"
      "\t-o <file>"
    );
}

void parseArgs(int argc, char* argv[]) {
  int opt = 0;
  struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                              {"output", required_argument, NULL, 'o'},
                              {"cc1", no_argument, NULL, 0},
                              {0, 0, 0, 0}};
  while ((opt = getopt_long(argc, argv, "ho:", longopts, NULL)) != -1) {
    switch (opt) {
    case 'h':
      usage();
      exit(EXIT_SUCCESS);
    case 'o':
      strncpy(output_file_path, optarg, sizeof(output_file_path));
      output_file_path[sizeof(output_file_path) - 1] = '\0';
      break;
    case 0:
      do_cc1 = true;
      break;
    case '?':
    case ':':
    default:
      usage();
      exit(EXIT_FAILURE);
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "expected argument after options\n");
    usage();
    exit(EXIT_FAILURE);
  }
  if (output_file_path[0] != 0) {
    output = fopen(output_file_path, "w");
    if (!output) {
      fprintf(stderr, "failed to open output file: '%s'\n", output_file_path);
      exit(EXIT_FAILURE);
    }
  } else {
    output = stdout;
  }
  input_file_path = argv[optind];
}

void cleanUp(void) {
  if (output != stdout) {
    fclose(output);
  }
}

int main(int argc, char *argv[]) {
  parseArgs(argc, argv);

  tokenise(input_file_path);
  parse();
  gen();

  cleanUp();

  return EXIT_SUCCESS;
}
