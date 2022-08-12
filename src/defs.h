#ifndef DEFS_H
#define DEFS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Var Var;
typedef struct Type Type;
typedef struct Node Node;
typedef struct Token Token;
typedef struct Function Function;
typedef struct Var Var;

typedef enum {
  ND_ADD,
  ND_BLK,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_EQ,
  ND_NE,
  ND_LT,
  ND_LE,
  ND_NUM,
  ND_ASS,
  ND_VAR,
  ND_RET,
  ND_IF,
  ND_FOR,
  ND_WHILE,
  ND_ADDR,
  ND_DEREF,
} NodeKind;

typedef enum {
  TY_INT,
  TY_PTR,
} TypeKind;

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

struct Type {
  TypeKind kind;
  Type *base;
};

struct Token {
  TokenKind kind;
  Token *next;
  int val;
  const char *str;
  size_t len;
};

struct Function {
  Node *body;
  Var *locals;
  size_t stack_size;
};

struct Node {
  NodeKind kind;
  Type *ty;
  Node *next;
  Node *lhs;
  Node *rhs;
  Node *body;
  Node *cond;
  Node *then;
  Node *els;
  Node *pre;
  Node *post;
  int val;
  size_t offset;
};

struct Var {
  Var *next;
  const char *name;
  size_t len;
  size_t offset;
};

#endif // DEFS_H
