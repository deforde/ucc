#ifndef NODE_H
#define NODE_H

#include <stddef.h>

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
  ND_LVAR,
  ND_RET,
  ND_IF,
  ND_FOR,
  ND_WHILE,
  ND_ADDR,
  ND_DEREF,
} NodeKind;

typedef struct Type Type;
typedef struct Node Node;
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

#endif // NODE_H
