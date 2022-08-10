#include "type.h"

#include <stddef.h>
#include <stdlib.h>

#include "node.h"

static Type *pointerTo(Type *base);

Type *ty_int = &(Type){.kind = TY_INT, .base = NULL};

bool isInteger(Type *ty) { return ty->kind == TY_INT; }

void addType(Node *node) {
  if (!node || node->ty) {
    return;
  }

  addType(node->next);
  addType(node->lhs);
  addType(node->rhs);
  addType(node->cond);
  addType(node->then);
  addType(node->els);
  addType(node->pre);
  addType(node->post);

  for (Node *n = node->body; n; n = n->next) {
    addType(n);
  }

  switch (node->kind) {
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_ASS:
    node->ty = node->lhs->ty;
    break;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
  case ND_NUM:
  case ND_LVAR:
    node->ty = ty_int;
    break;
  case ND_ADDR:
    node->ty = pointerTo(node->body->ty);
    break;
  case ND_DEREF:
    if (node->body->ty->kind == TY_PTR) {
      node->ty = node->body->ty->base;
    } else {
      node->ty = ty_int;
    }
    break;
  default:
    break;
  }
}

Type *pointerTo(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->base = base;
  return ty;
}
