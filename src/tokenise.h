#ifndef TOKENISE_H
#define TOKENISE_H

#include <stdbool.h>

typedef struct Token Token;

void error(const char *fmt, ...);
bool consume(char *op);
Token *consumeIdent(void);
bool consumeReturn(void);
bool consumeIf(void);
bool consumeElse(void);
bool consumeWhile(void);
bool consumeFor(void);
void expect(char *op);
int expectNumber(void);
void tokenise(const char *p);
bool isEOF(void);

#endif // TOKENISE_H
