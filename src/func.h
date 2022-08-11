#ifndef FUNC_H
#define FUNC_H

#include <stddef.h>

typedef struct Node Node;
typedef struct Lvar Lvar;

typedef struct {
  Node *body;
  Lvar *locals;
  size_t stack_size;
} Function;

#endif // FUNC_H
