#include "comp_err.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "defs.h"

extern Token *token;
extern const char *input;

#define COMP_ERR_BODY(LOC)                                                     \
  va_list args;                                                                \
  va_start(args, fmt);                                                         \
  const int pos = (int)((LOC)-input);                                          \
  fprintf(stderr, "%s\n", input);                                              \
  fprintf(stderr, "%*s", pos, " ");                                            \
  fprintf(stderr, "^ ");                                                       \
  vfprintf(stderr, fmt, args);                                                 \
  va_end(args);                                                                \
  fprintf(stderr, "\n");                                                       \
  exit(EXIT_FAILURE);

void compError(const char *fmt, ...) { COMP_ERR_BODY(token->str); }

void compErrorToken(const char *loc, const char *fmt, ...) {
  COMP_ERR_BODY(loc);
}
