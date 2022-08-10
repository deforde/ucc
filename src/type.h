#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>

typedef struct Type Type;
typedef struct Node Node;

typedef enum {
  TY_INT,
  TY_PTR,
} TypeKind;

struct Type {
  TypeKind kind;
  Type *base;
};

bool isInteger(Type *ty);
void addType(Node *node);

#endif // TYPE_H
