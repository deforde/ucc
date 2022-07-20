#ifndef NODE_H
#define NODE_H

#include <stddef.h>

typedef enum {
  ND_ADD,
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
} NodeType;

typedef struct Node Node;
struct Node {
  NodeType type;
  Node *next;
  Node *lhs;
  Node *rhs;
  Node *body;
  int val;
  size_t offset;
};

#endif // NODE_H
