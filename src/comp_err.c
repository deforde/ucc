#include "comp_err.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "defs.h"

extern Token *token;
extern const char *file_content;
extern const char *file_path;

#define COMP_ERR_BODY(LOC)                                                     \
  const char *line_start = (LOC);                                              \
  while (file_content < line_start && line_start[-1] != '\n') {                \
    line_start--;                                                              \
  }                                                                            \
  const char *line_end = (LOC);                                                \
  while (*line_end != '\n') {                                                  \
    line_end++;                                                                \
  }                                                                            \
  size_t line_num = 1;                                                         \
  for (const char *p = file_content; p < line_start; p++) {                    \
    if (*p == '\n') {                                                          \
      line_num++;                                                              \
    }                                                                          \
  }                                                                            \
  size_t offset = fprintf(stderr, "%s:%zu: ", file_path, line_num);            \
  fprintf(stderr, "%.*s\n", (int)(line_end - line_start), line_start);         \
  const int pos = (int)((LOC)-line_start + offset);                            \
  fprintf(stderr, "%*s", pos, " ");                                            \
  fprintf(stderr, "^ ");                                                       \
  va_list args;                                                                \
  va_start(args, fmt);                                                         \
  vfprintf(stderr, fmt, args);                                                 \
  va_end(args);                                                                \
  fprintf(stderr, "\n");                                                       \
  exit(EXIT_FAILURE);

void compError(const char *fmt, ...) { COMP_ERR_BODY(token->str); }

void compErrorToken(const char *loc, const char *fmt, ...) {
  COMP_ERR_BODY(loc);
}
