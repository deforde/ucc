#ifndef TOKENISE_H
#define TOKENISE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Token Token;

bool consume(char *op);
Token *consumeIdent(void);
bool consumeReturn(void);
bool consumeIf(void);
bool consumeElse(void);
bool consumeWhile(void);
bool consumeFor(void);
bool consumeSizeof(void);
Token *consumeStrLit(void);
void expect(char *op);
int64_t expectNumber(void);
Token *expectIdent(void);
void tokenise(const char *file_path);
bool isEOF(void);
bool isFunc(void);

#endif // TOKENISE_H
