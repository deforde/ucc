#include "tokenise.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

Token *token = NULL;
extern const char *input;

static void error(const char *loc, const char *fmt, ...);
static bool startsWith(const char *p, const char *q);
static Token *newToken(TokenType type, Token *cur, const char *str, size_t len);
static Token *newIdent(Token *cur, const char **p);
static bool isIdentChar(char c);
static bool consumeTokType(TokenType type);

void error(const char *loc, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const int pos = (int)(loc - input);
  fprintf(stderr, "%s\n", input);
  fprintf(stderr, "%*s", pos, " ");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

bool startsWith(const char *p, const char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

bool consume(char *op) {
  if (token->type != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len) != 0) {
    return false;
  }
  token = token->next;
  return true;
}

bool consumeTokType(TokenType type) {
  if (token->type != type) {
    return false;
  }
  token = token->next;
  return true;
}

bool consumeReturn(void) { return consumeTokType(TK_RET); }

bool consumeIf(void) { return consumeTokType(TK_IF); }

bool consumeElse(void) { return consumeTokType(TK_ELSE); }

bool consumeWhile(void) { return consumeTokType(TK_WHILE); }

bool consumeFor(void) { return consumeTokType(TK_FOR); }

Token *consumeIdent(void) {
  if (token->type != TK_IDENT) {
    return NULL;
  }
  Token *cur = token;
  token = token->next;
  return cur;
}

void expect(char *op) {
  if (token->type != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len) != 0) {
    error(token->str, "Expected: '%c'", *op);
  }
  token = token->next;
}

int expectNumber(void) {
  if (token->type != TK_NUM) {
    error(token->str, "Expected number");
  }
  const int val = token->val;
  token = token->next;
  return val;
}

bool isEOF(void) { return token->type == TK_EOF; }

Token *newToken(TokenType type, Token *cur, const char *str, size_t len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->type = type;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

void tokenise(const char *p) {
  Token head = {0};
  Token *cur = &head;
  while (*p) {
    if (isspace(*p)) {
      ++p;
      continue;
    }
    if (startsWith(p, "==") || startsWith(p, "!=") || startsWith(p, "<=") ||
        startsWith(p, ">=")) {
      cur = newToken(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }
    if (strchr("+-*/()<>=;{}&", *p)) {
      cur = newToken(TK_RESERVED, cur, p++, 1);
      continue;
    }
    if (isdigit(*p)) {
      cur = newToken(TK_NUM, cur, p, 0);
      const char *q = p;
      cur->val = (int)strtol(p, (char **)&p, 10);
      cur->len = p - q;
      continue;
    }
    if (strncmp(p, "if", 2) == 0 && !isIdentChar(p[2])) {
      cur = newToken(TK_IF, cur, p, 2);
      p += 2;
      continue;
    }
    if (strncmp(p, "else", 4) == 0 && !isIdentChar(p[4])) {
      cur = newToken(TK_ELSE, cur, p, 4);
      p += 4;
      continue;
    }
    if (strncmp(p, "while", 5) == 0 && !isIdentChar(p[5])) {
      cur = newToken(TK_WHILE, cur, p, 5);
      p += 5;
      continue;
    }
    if (strncmp(p, "for", 3) == 0 && !isIdentChar(p[3])) {
      cur = newToken(TK_FOR, cur, p, 3);
      p += 3;
      continue;
    }
    if (strncmp(p, "return", 6) == 0 && !isIdentChar(p[6])) {
      cur = newToken(TK_RET, cur, p, 6);
      p += 6;
      continue;
    }
    if (isIdentChar(*p) && !(*p >= '0' && *p <= '9')) {
      cur = newIdent(cur, &p);
      continue;
    }
    error(p, "Invalid token");
  }
  newToken(TK_EOF, cur, p, 0);
  token = head.next;
}

Token *newIdent(Token *cur, const char **p) {
  const char *q = *p;
  while (*q && isIdentChar(*q)) {
    q++;
  }
  Token *tok = newToken(TK_IDENT, cur, *p, q - *p);
  *p = q;
  return tok;
}

bool isIdentChar(char c) { return isalnum(c) || c == '_'; }
