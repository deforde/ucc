#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Type Type;

void parse(void);
bool isInteger(Type *ty);
bool isFloat(Type *ty);
bool isNumeric(Type *ty);

#endif // PARSE_H
