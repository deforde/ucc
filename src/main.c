#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "codegen.h"
#include "comp_err.h"
#include "parse.h"
#include "tokenise.h"

static char output_file_path[PATH_MAX] = {0};
static bool do_cc1 = false;
static bool do_argprint = false;
static bool do_assemble = true;
char *input_file_path = NULL;
FILE *output = NULL;

static char *createTmpfile(void);
static void assemble(char *input_path, char *output_path);
static void cc1(void);
static void cleanUp(void);
static void openOutput(void);
static void parseArgs(int argc, char *argv[]);
static void preprocess(char *input_path, char *output_path);
static void replaceExt(char (*path)[PATH_MAX], char *ext);
static void runSubprocess(char **argv);
static void runcc1(char *arg0, char *input, char *output);
static void usage(void);

void usage(void) {
  puts("Usage: ucc [options] file\n"
       "Options:\n"
       "\t-S\n"
       "\t-o <file>");
}

void parseArgs(int argc, char *argv[]) {
  int opt = 0;
  struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                              {"output", required_argument, NULL, 'o'},
                              {"cc1", no_argument, NULL, 0},
                              {"###", no_argument, NULL, 1},
                              {"", no_argument, NULL, 'S'},
                              {0, 0, 0, 0}};
  while ((opt = getopt_long(argc, argv, "ho:S", longopts, NULL)) != -1) {
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
    case 1:
      do_argprint = true;
      break;
    case 'S':
      do_assemble = false;
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
  input_file_path = argv[optind];
  if (output_file_path[0] == 0) {
    strncpy(output_file_path, input_file_path, PATH_MAX);
    if (do_assemble) {
      replaceExt(&output_file_path, "o");
    } else {
      replaceExt(&output_file_path, "s");
    }
  }
}

void cleanUp(void) {
  if (output && output != stdout) {
    fclose(output);
  }
}

void cc1(void) {
  tokenise(input_file_path);
  parse();
  gen();
}

void runcc1(char *arg0, char *input, char *output) {
  char **args = calloc(6, sizeof(char *));
  args[0] = arg0;
  int argc = 1;
  args[argc++] = "--cc1";
  if (input) {
    args[argc++] = input;
  }
  if (output) {
    args[argc++] = "-o";
    args[argc++] = output;
  }
  runSubprocess(args);
}

void replaceExt(char (*path)[PATH_MAX], char *ext) {
  char *p = strrchr(*path, '.');
  if (p != NULL) {
    *(++p) = 0;
  } else {
    p = &(*path)[strlen(*path)];
  }
  strncat(*path, ext, PATH_MAX - (p - *path));
}

char *createTmpfile(void) {
  char *path = strdup("/tmp/ucc-XXXXXX");
  int fd = mkstemp(path);
  if (fd == -1) {
    compError("mkstemp failed: %s", strerror(errno));
  }
  close(fd);
  return path;
}

void runSubprocess(char **argv) {
  if (do_argprint) {
    size_t idx = 0;
    printf("args = [ ");
    for (const char *arg = argv[idx]; arg; arg = argv[++idx]) {
      printf("%s%s", arg, argv[idx + 1] ? ", " : "");
    }
    puts(" ]");
    exit(EXIT_SUCCESS);
  }
  if (fork() == 0) {
    execvp(argv[0], argv);
    compError("exec failed: %s: %s", argv[0], strerror(errno));
    exit(EXIT_FAILURE);
  }
  int status = 0;
  // clang-format off
  while (wait(&status) > 0);
  // clang-format on
  if (status != 0) {
    exit(EXIT_FAILURE);
  }
}

void openOutput(void) {
  if (output_file_path[0] == '-' && output_file_path[1] == 0) {
    output = stdout;
  } else {
    output = fopen(output_file_path, "w");
    if (!output) {
      fprintf(stderr, "failed to open output file: '%s'\n", output_file_path);
      exit(EXIT_FAILURE);
    }
  }
}

void assemble(char *input_path, char *output_path) {
  char *cmd[] = {"as", "-c", input_path, "-o", output_path, NULL};
  runSubprocess(cmd);
}

void preprocess(char *input_path, char *output_path) {
  char *cmd[] = {"cpp", "-E", "-P", "-C", input_path, output_path, NULL};
  runSubprocess(cmd);
}

int main(int argc, char *argv[]) {
  parseArgs(argc, argv);

  if (do_cc1) {
    openOutput();
    cc1();
    return EXIT_SUCCESS;
  }

  char *compiler_input = createTmpfile();
  preprocess(input_file_path, compiler_input);

  if (do_assemble) {
    char *compiler_output = createTmpfile();
    runcc1(argv[0], compiler_input, compiler_output);
    assemble(compiler_output, output_file_path);
  } else {
    runcc1(argv[0], compiler_input, output_file_path);
  }

  cleanUp();

  return EXIT_SUCCESS;
}
