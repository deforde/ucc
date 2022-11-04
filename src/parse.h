#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Type Type;

size_t alignTo(size_t n, size_t align);
void parse(void);
bool isInteger(Type *ty);
bool isFloat(Type *ty);
bool isNumeric(Type *ty);

#endif // PARSE_H
