#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

typedef enum {
  TK_RESERVED,
  TK_IDENT,
  TK_NUM,
  TK_RET,
  TK_IF,
  TK_ELSE,
  TK_WHILE,
  TK_FOR,
  TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind;
  Token *next;
  int val;
  const char *str;
  size_t len;
};

#endif // TOKEN_H
