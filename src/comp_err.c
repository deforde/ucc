#include "comp_err.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "token.h"

extern Token *token;
extern const char *input;

void compError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  compErrorToken(token->str, fmt, args);
}

void compErrorToken(const char *loc, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const int pos = (int)(loc - input);
  fprintf(stderr, "%s\n", input);
  fprintf(stderr, "%*s", pos, " ");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}
