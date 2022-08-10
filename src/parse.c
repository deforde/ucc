#include "parse.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp_err.h"
#include "node.h"
#include "token.h"
#include "tokenise.h"
#include "type.h"

typedef struct Lvar Lvar;
struct Lvar {
  Lvar *next;
  const char *name;
  size_t len;
  size_t offset;
};

Node prog = {0};
static Lvar *locals = NULL;
extern Type *ty_int;

static Node *expr(void);
static Node *assign(void);
static Node *stmt(void);
static Node *newNode(NodeKind kind, Node *lhs, Node *rhs);
static Node *newNodeNum(int val);
static Node *newNodeAdd(Node *lhs, Node *rhs);
static Node *newNodeSub(Node *lhs, Node *rhs);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *primary(void);
static Node *mul(void);
static Node *unary(void);
static Lvar *findLvar(Token *tok);
static Node *cmpnd_stmt(void);

void parse() {
  expect("{");
  prog.body = cmpnd_stmt();
}

Node *cmpnd_stmt(void) {
  Node head = {0};
  Node *cur = &head;
  while (!consume("}")) {
    cur = cur->next = stmt();
    addType(cur);
  }
  Node *node = newNode(ND_BLK, NULL, NULL);
  node->body = head.next;
  return node;
}

Node *assign(void) {
  Node *node = equality();
  if (consume("=")) {
    node = newNode(ND_ASS, node, assign());
  }
  return node;
}

Node *stmt(void) {
  Node *node = NULL;
  if (consume(";")) {
    node = newNode(ND_BLK, NULL, NULL);
  } else if (consume("{")) {
    node = cmpnd_stmt();
  } else if (consumeIf()) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consumeElse()) {
      node->els = stmt();
    }
  } else if (consumeFor()) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    expect("(");
    node->pre = stmt();
    node->cond = stmt();
    if (!consume(")")) {
      node->post = expr();
      expect(")");
    }
    node->body = stmt();
  } else if (consumeWhile()) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_WHILE;
    expect("(");
    node->cond = expr();
    expect(")");
    node->body = stmt();
  } else if (consumeReturn()) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RET;
    node->lhs = expr();
    expect(";");
  } else {
    node = expr();
    expect(";");
  }
  return node;
}

Node *newNode(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *newNodeNum(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

Node *newNodeAdd(Node *lhs, Node *rhs) {
  addType(lhs);
  addType(rhs);
  if (isInteger(lhs->ty) && isInteger(rhs->ty)) {
    return newNode(ND_ADD, lhs, rhs);
  }
  if (lhs->ty->base && rhs->ty->base) {
    compError("invalid operands");
  }
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }
  rhs = newNode(ND_MUL, rhs, newNodeNum(8)); // Size of ptr on 64 system
  return newNode(ND_ADD, lhs, rhs);
}

Node *newNodeSub(Node *lhs, Node *rhs) {
  addType(lhs);
  addType(rhs);
  if (isInteger(lhs->ty) && isInteger(rhs->ty)) {
    return newNode(ND_SUB, lhs, rhs);
  }
  if (lhs->ty->base && isInteger(rhs->ty)) {
    rhs = newNode(ND_MUL, rhs, newNodeNum(8)); // Size of ptr on 64 system
    addType(rhs);
    Node *node = newNode(ND_SUB, lhs, rhs);
    node->ty = lhs->ty;
    return node;
  }
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = newNode(ND_SUB, lhs, rhs);
    node->ty = ty_int;
    return newNode(ND_DIV, node, newNodeNum(8)); // Size of ptr on 64 system
  }
  compError("invalid operands");
  assert(false);
  return NULL;
}

Node *newNodeIdent(Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_LVAR;

  Lvar *lvar = findLvar(tok);
  if (lvar) {
    node->offset = lvar->offset;
  } else {
    lvar = calloc(1, sizeof(Lvar));
    lvar->next = locals;
    lvar->name = tok->str;
    lvar->len = tok->len;
    lvar->offset = (locals ? locals->offset : 0) + 8;
    node->offset = lvar->offset;
    locals = lvar;
  }

  return node;
}

Node *primary(void) {
  Token *tok = consumeIdent();
  if (tok) {
    return newNodeIdent(tok);
  }
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }
  return newNodeNum(expectNumber());
}

Node *mul(void) {
  Node *node = unary();
  for (;;) {
    if (consume("*")) {
      node = newNode(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = newNode(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

Node *expr(void) { return assign(); }

Node *equality(void) {
  Node *node = relational();
  for (;;) {
    if (consume("==")) {
      node = newNode(ND_EQ, node, relational());
    } else if (consume("!=")) {
      node = newNode(ND_NE, node, relational());
    } else {
      return node;
    }
  }
}

Node *relational(void) {
  Node *node = add();
  for (;;) {
    if (consume("<")) {
      node = newNode(ND_LT, node, add());
    } else if (consume("<=")) {
      node = newNode(ND_LE, node, add());
    } else if (consume(">")) {
      node = newNode(ND_LT, add(), node);
    } else if (consume(">=")) {
      node = newNode(ND_LE, add(), node);
    } else {
      return node;
    }
  }
}

Node *add(void) {
  Node *node = mul();
  for (;;) {
    if (consume("+")) {
      node = newNodeAdd(node, mul());
    } else if (consume("-")) {
      node = newNodeSub(node, mul());
    } else {
      return node;
    }
  }
}

Node *unary(void) {
  if (consume("+")) {
    return unary();
  }
  if (consume("-")) {
    return newNode(ND_SUB, newNodeNum(0), unary());
  }
  if (consume("*")) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_DEREF;
    node->body = unary();
    return node;
  }
  if (consume("&")) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_ADDR;
    node->body = unary();
    return node;
  }
  return primary();
}

Lvar *findLvar(Token *tok) {
  for (Lvar *var = locals; var; var = var->next) {
    if (var->len == tok->len && memcmp(tok->str, var->name, var->len) == 0) {
      return var;
    }
  }
  return NULL;
}
