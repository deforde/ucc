#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#include "defs.h"

void parse(void);
bool isInteger(Type *ty);
bool isFloat(Type *ty);

#endif // PARSE_H
