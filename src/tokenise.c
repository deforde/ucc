#include "tokenise.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "comp_err.h"
#include "defs.h"

Token *token = NULL;
const char *file_content = NULL;

static Token *newIdent(Token *cur, const char **p, size_t line_num);
static Token *newToken(TokenKind kind, Token *cur, const char *str, size_t len,
                       size_t line_num);
static bool consumeKwdMatch(const char *kwd);
static bool consumeTokKind(TokenKind kind);
static bool isIdentChar(char c);
static bool isKeyword(const char *str, size_t len);
static bool startsWith(const char *p, const char *q);
static char *readFile(const char *file_path);
static int fromHex(char c);
static int readEscapedChar(const char **p);
static long readIntLiteral(const char **start);

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

Token *consumeLabel(void) {
  if (token->kind != TK_IDENT || token->next->kind != TK_RESERVED ||
      token->next->len != 1 || token->next->str[0] != ':') {
    return NULL;
  }
  Token *label = token;
  token = token->next->next;
  return label;
}

Token *consumeStrLit(void) {
  if (token->kind != TK_STR) {
    return NULL;
  }
  Token *cur = token;
  token = token->next;
  return cur;
}

Token *consumeKeyword(void) {
  if (token->kind != TK_KWD) {
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

Token *expectKeyword(void) {
  Token *tok = consumeKeyword();
  if (tok == NULL) {
    compError("expected keyword");
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

int64_t expectNumber(void) {
  if (token->kind != TK_NUM) {
    compError("expected number");
  }
  const int64_t val = token->val;
  token = token->next;
  return val;
}

bool isEOF(void) { return token->kind == TK_EOF; }

Token *newToken(TokenKind kind, Token *cur, const char *str, size_t len,
                size_t line_num) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  tok->line_num = line_num;
  cur->next = tok;
  return tok;
}

void tokenise(const char *file_path) {
  const char *p = file_content = readFile(file_path);

  Token head = {0};
  Token *cur = &head;
  size_t line_num = 1;
  while (*p) {
    if (startsWith(p, "//")) {
      while (*p != '\n') {
        p++;
      }
      continue;
    }
    if (startsWith(p, "/*")) {
      char *q = strstr(p + 2, "*/");
      if (!q) {
        compErrorToken(p, "unclosed block comment");
      }
      p = q + 2;
      continue;
    }
    if (isspace(*p)) {
      if (*p == '\n') {
        line_num++;
      }
      ++p;
      continue;
    }
    if (startsWith(p, ">>=") || startsWith(p, "<<=")) {
      cur = newToken(TK_RESERVED, cur, p, 3, line_num);
      p += 3;
      continue;
    }
    if (startsWith(p, "==") || startsWith(p, "!=") || startsWith(p, "<=") ||
        startsWith(p, ">=") || startsWith(p, "->") || startsWith(p, "+=") ||
        startsWith(p, "-=") || startsWith(p, "*=") || startsWith(p, "/=") ||
        startsWith(p, "++") || startsWith(p, "--") || startsWith(p, "%=") ||
        startsWith(p, "&=") || startsWith(p, "|=") || startsWith(p, "^=") ||
        startsWith(p, "&&") || startsWith(p, "||") || startsWith(p, ">>") ||
        startsWith(p, "<<")) {
      cur = newToken(TK_RESERVED, cur, p, 2, line_num);
      p += 2;
      continue;
    }
    if (strchr("+-*/()<>=;{}&,[].!~%|^:?", *p)) {
      cur = newToken(TK_RESERVED, cur, p++, 1, line_num);
      continue;
    }
    if (isdigit(*p)) {
      const char *q = p;
      const long val = readIntLiteral(&p);
      cur = newToken(TK_NUM, cur, p, p - q, line_num);
      cur->val = val;
      continue;
    }
    if (strncmp(p, "if", 2) == 0 && !isIdentChar(p[2])) {
      cur = newToken(TK_IF, cur, p, 2, line_num);
      p += 2;
      continue;
    }
    if (strncmp(p, "else", 4) == 0 && !isIdentChar(p[4])) {
      cur = newToken(TK_ELSE, cur, p, 4, line_num);
      p += 4;
      continue;
    }
    if (strncmp(p, "while", 5) == 0 && !isIdentChar(p[5])) {
      cur = newToken(TK_WHILE, cur, p, 5, line_num);
      p += 5;
      continue;
    }
    if (strncmp(p, "for", 3) == 0 && !isIdentChar(p[3])) {
      cur = newToken(TK_FOR, cur, p, 3, line_num);
      p += 3;
      continue;
    }
    if (strncmp(p, "return", 6) == 0 && !isIdentChar(p[6])) {
      cur = newToken(TK_RET, cur, p, 6, line_num);
      p += 6;
      continue;
    }
    if (strncmp(p, "sizeof", 6) == 0 && !isIdentChar(p[6])) {
      cur = newToken(TK_SIZEOF, cur, p, 6, line_num);
      p += 6;
      continue;
    }
    if (isIdentChar(*p) && !(*p >= '0' && *p <= '9')) {
      cur = newIdent(cur, &p, line_num);
      if (isKeyword(cur->str, cur->len)) {
        cur->kind = TK_KWD;
      }
      continue;
    }
    if (*p == '"') {
      const char *start = ++p;
      for (; *p != '"'; ++p) {
        if (*p == '\n' || *p == '\0') {
          compErrorToken(start, "unclosed string literal");
        }
        if (*p == '\\') {
          p++;
        }
      }
      const size_t max_len = p - start + 1;
      cur = newToken(TK_STR, cur, start, max_len, line_num);
      cur->str = calloc(1, max_len);
      size_t len = 0;
      for (const char *c = start; c != p;) {
        if (*c == '\\') {
          c++;
          ((char *)cur->str)[len++] = (char)readEscapedChar(&c);
        } else {
          ((char *)cur->str)[len++] = *(c++);
        }
      }
      cur->len = len + 1;
      p++;
      continue;
    }
    if (*p == '\'') {
      const char *start = p++;
      if (*p == '\0') {
        compErrorToken(start, "unclosed char literal");
      }
      char c = 0;
      if (*p == '\\') {
        p++;
        c = (char)readEscapedChar(&p);
      } else {
        c = *p++;
      }
      if (*p != '\'') {
        compErrorToken(start, "unclosed char literal");
      }
      p++;
      cur = newToken(TK_NUM, cur, start, p - start, line_num);
      cur->val = (int64_t)c;
      continue;
    }
    compErrorToken(p, "invalid token");
  }
  newToken(TK_EOF, cur, p, 0, line_num);
  token = head.next;
}

Token *newIdent(Token *cur, const char **p, size_t line_num) {
  const char *q = *p;
  while (*q && isIdentChar(*q)) {
    q++;
  }
  Token *tok = newToken(TK_IDENT, cur, *p, q - *p, line_num);
  *p = q;
  return tok;
}

bool isIdentChar(char c) { return isalnum(c) || c == '_'; }

int fromHex(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  }
  if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }
  return c - 'A' + 10;
}

int readEscapedChar(const char **p) {
  const char *ch = *p;

  if ('0' <= *ch && *ch <= '7') {
    int c = *ch - '0';
    ch++;
    if ('0' <= *ch && *ch <= '7') {
      c = (c << 3) + (*ch - '0');
      ch++;
      if ('0' <= *ch && *ch <= '7') {
        c = (c << 3) + (*ch - '0');
        ch++;
      }
    }
    *p = ch;
    return c;
  }

  if (*ch == 'x') {
    ch++;
    if (!isxdigit(*ch)) {
      compErrorToken(ch, "invalid hex escache sequence");
    }
    int c = 0;
    for (; isxdigit(*ch); ch++) {
      c = (c << 4) + fromHex(*ch);
    }
    *p = ch;
    return c;
  }

  switch (*((*p)++)) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'e':
    return 27;
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  default:
    break;
  }
  return *((*p) - 1);
}

char *readFile(const char *file_path) {
  FILE *file = fopen(file_path, "rb");
  if (file == NULL) {
    fprintf(stderr, "failed to open file: '%s'\n", file_path);
    exit(EXIT_FAILURE);
  }
  fseek(file, 0, SEEK_END);
  const size_t len = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *file_content = calloc(len + 2, 1);
  if (file_content == NULL) {
    fclose(file);
    fprintf(stderr,
            "failed to allocate memory for content read from file: '%s'\n",
            file_path);
    exit(EXIT_FAILURE);
  }
  const size_t bytesRead = fread(file_content, 1, len, file);
  fclose(file);
  if (bytesRead != len) {
    free(file_content);
    fprintf(stderr,
            "failed to read the expected number of bytes from file: '%s'. "
            "attempted to read: %zu, actually read: %zu\n",
            file_path, len, bytesRead);
    exit(EXIT_FAILURE);
  }
  if (file_content[len - 1] != '\n') {
    file_content[len] = '\n';
  }
  return file_content;
}

bool isKeyword(const char *str, size_t len) {
  static const char *kwds[] = {
      "int",   "char",     "short",  "void", "long",   "struct",
      "union", "typedef",  "_Bool",  "enum", "static", "goto",
      "break", "continue", "switch", "case", "default"};
  for (size_t i = 0; i < sizeof(kwds) / sizeof(*kwds); ++i) {
    if (strlen(kwds[i]) == len && strncmp(str, kwds[i], len) == 0) {
      return true;
    }
  }
  return false;
}

long readIntLiteral(const char **start) {
  const char *p = *start;
  int base = 10;
  if (!strncasecmp(p, "0x", 2) && isalnum(p[2])) {
    p += 2;
    base = 16;
  } else if (!strncasecmp(p, "0b", 2) && isalnum(p[2])) {
    p += 2;
    base = 2;
  } else if (*p == '0') {
    base = 8;
  }

  const long val = (long)strtoul(p, (char **)&p, base);
  if (isalnum(*p)) {
    compErrorToken(p, "invalid digit");
  }

  *start = p;

  return val;
}

bool consumeKwdMatch(const char *kwd) {
  if (token->kind != TK_KWD || strlen(kwd) != token->len ||
      memcmp(token->str, kwd, token->len) != 0) {
    return false;
  }
  token = token->next;
  return true;
}

bool consumeGoto(void) { return consumeKwdMatch("goto"); }

bool consumeBreak(void) { return consumeKwdMatch("break"); }

bool consumeCont(void) { return consumeKwdMatch("continue"); }

bool consumeSwitch(void) { return consumeKwdMatch("switch"); }

bool consumeCase(void) { return consumeKwdMatch("case"); }

bool consumeDefault(void) { return consumeKwdMatch("default"); }
