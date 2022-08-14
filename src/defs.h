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
  ND_ADDR,
  ND_ASS,
  ND_BLK,
  ND_DEREF,
  ND_DIV,
  ND_EQ,
  ND_FOR,
  ND_FUNCCALL,
  ND_IF,
  ND_LE,
  ND_LT,
  ND_MUL,
  ND_NE,
  ND_NUM,
  ND_RET,
  ND_SUB,
  ND_VAR,
  ND_WHILE,
} NodeKind;

typedef enum {
  TY_INT,
  TY_PTR,
} TypeKind;

typedef enum {
  TK_ELSE,
  TK_EOF,
  TK_FOR,
  TK_IDENT,
  TK_IF,
  TK_NUM,
  TK_RESERVED,
  TK_RET,
  TK_WHILE,
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
  Var *var;
  const char* funcname;
  int val;
};

struct Var {
  Var *next;
  Type *ty;
  const char *name;
  size_t len;
  size_t offset;
};

#endif // DEFS_H
