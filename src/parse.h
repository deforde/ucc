#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Token Token;

bool consume(char* op);
void expect(char* op);
int expectNumber(void);
Token* tokenise(const char* p);

#endif //PARSE_H
