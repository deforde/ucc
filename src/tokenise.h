#ifndef TOKENISE_H
#define TOKENISE_H

#include <stdbool.h>

typedef struct Token Token;

bool consume(char *op);
Token *consumeIdent(void);
bool consumeReturn(void);
bool consumeIf(void);
bool consumeElse(void);
bool consumeWhile(void);
bool consumeFor(void);
bool consumeSizeof(void);
void expect(char *op);
int expectNumber(void);
Token *expectIdent(void);
void tokenise(const char *p);
bool isEOF(void);
bool isFunc(void);

#endif // TOKENISE_H
