#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "err.h"

extern const char* input;

void error(const char* loc, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int pos = (int)(loc - input);
    fprintf(stderr, "%s\n", input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}
