#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#include "defs.h"

Type *getType(const char *kwd, size_t len);
void parse(void);

#endif // PARSE_H
