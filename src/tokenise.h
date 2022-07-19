#ifndef TOKENISE_H
#define TOKENISE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  TK_RESERVED,
  TK_IDENT,
  TK_NUM,
  TK_RET,
  TK_IF,
  TK_WHILE,
  TK_FOR,
  TK_EOF,
} TokenType;

typedef struct Token Token;
struct Token {
  TokenType type;
  Token *next;
  int val;
  const char *str;
  size_t len;
};

bool consume(char *op);
Token *consumeIdent(void);
bool consumeReturn(void);
bool consumeIf(void);
bool consumeWhile(void);
bool consumeFor(void);
void expect(char *op);
int expectNumber(void);
Token *tokenise(const char *p);
bool isEOF(void);

#endif // TOKENISE_H
