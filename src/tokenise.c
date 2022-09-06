#include "tokenise.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "comp_err.h"
#include "defs.h"

Token *token = NULL;

static Token *newIdent(Token *cur, const char **p);
static Token *newToken(TokenKind kind, Token *cur, const char *str, size_t len);
static bool consumeTokKind(TokenKind kind);
static bool isIdentChar(char c);
static bool startsWith(const char *p, const char *q);

bool startsWith(const char *p, const char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

bool consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len) != 0) {
    return false;
  }
  token = token->next;
  return true;
}

bool consumeTokKind(TokenKind kind) {
  if (token->kind != kind) {
    return false;
  }
  token = token->next;
  return true;
}

bool consumeReturn(void) { return consumeTokKind(TK_RET); }

bool consumeIf(void) { return consumeTokKind(TK_IF); }

bool consumeElse(void) { return consumeTokKind(TK_ELSE); }

bool consumeWhile(void) { return consumeTokKind(TK_WHILE); }

bool consumeFor(void) { return consumeTokKind(TK_FOR); }

bool consumeSizeof(void) { return consumeTokKind(TK_SIZEOF); }

Token *consumeStrLit(void) {
  if (token->kind != TK_STR) {
    return NULL;
  }
  Token *cur = token;
  token = token->next;
  return cur;
}

Token *consumeIdent(void) {
  if (token->kind != TK_IDENT) {
    return NULL;
  }
  Token *cur = token;
  token = token->next;
  return cur;
}

Token *expectIdent(void) {
  Token *tok = consumeIdent();
  if (tok == NULL) {
    compError("expected identifier");
  }
  return tok;
}

void expect(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len) != 0) {
    compError("expected: '%c'", *op);
  }
  token = token->next;
}

int expectNumber(void) {
  if (token->kind != TK_NUM) {
    compError("expected number");
  }
  const int val = token->val;
  token = token->next;
  return val;
}

bool isEOF(void) { return token->kind == TK_EOF; }

Token *newToken(TokenKind kind, Token *cur, const char *str, size_t len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
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
    if (strchr("+-*/()<>=;{}&,[]", *p)) {
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
    if (strncmp(p, "sizeof", 6) == 0 && !isIdentChar(p[6])) {
      cur = newToken(TK_SIZEOF, cur, p, 6);
      p += 6;
      continue;
    }
    if (isIdentChar(*p) && !(*p >= '0' && *p <= '9')) {
      cur = newIdent(cur, &p);
      continue;
    }
    if (*p == '"') {
      const char *start = ++p;
      for (; *p != '"'; ++p) {
        if (*p == '\n' || *p == '\0') {
          compErrorToken(start, "unclosed string literal");
        }
      }
      const size_t len = p - start + 1;
      cur = newToken(TK_STR, cur, start, len);
      cur->str = calloc(1, len);
      memcpy((char *)cur->str, start, len - 1);
      p++;
      continue;
    }
    compErrorToken(p, "invalid token");
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

bool isFunc(void) {
  if (token->kind == TK_IDENT) {
    if (token->next->len == 1 && token->next->str[0] == '(') {
      return true;
    }
  }
  return false;
}
