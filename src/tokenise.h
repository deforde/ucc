#ifndef TOKENISE_H
#define TOKENISE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Token Token;

Token *consumeIdent(void);
Token *consumeKeyword(void);
Token *consumeLabel(void);
Token *consumeStrLit(void);
Token *expectIdent(void);
Token *expectKeyword(void);
Token *expectNumber(void);
bool consume(char *op);
bool consumeAlignof(void);
bool consumeBreak(void);
bool consumeCase(void);
bool consumeCont(void);
bool consumeDefault(void);
bool consumeDo(void);
bool consumeElse(void);
bool consumeFor(void);
bool consumeGoto(void);
bool consumeIf(void);
bool consumeKwdMatch(const char *kwd);
bool consumeReturn(void);
bool consumeSizeof(void);
bool consumeSwitch(void);
bool consumeWhile(void);
bool isEOF(void);
void expect(char *op);
void expectWhile(void);
void tokenise(const char *file_path);

#endif // TOKENISE_H
