#ifndef PARSE_H
#define PARSE_H

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
  Node *lhs;
  Node *rhs;
  int val;
  size_t offset;
};

Node *expr(void);
void program(void);

#endif // PARSE_H
