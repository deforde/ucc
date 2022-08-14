#include "parse.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp_err.h"
#include "defs.h"
#include "tokenise.h"

extern Type *ty_int;
Function prog = {0};
Type *ty_int = &(Type){.kind = TY_INT, .base = NULL};

static Node *add(void);
static Node *assign(void);
static Node *cmpndStmt(void);
static Node *declaration(void);
static Node *equality(void);
static Node *expr(void);
static Node *mul(void);
static Node *newNode(NodeKind kind, Node *lhs, Node *rhs);
static Node *newNodeAdd(Node *lhs, Node *rhs);
static Node *newNodeIdent(Token *tok);
static Node *newNodeNum(int val);
static Node *newNodeSub(Node *lhs, Node *rhs);
static Node *primary(void);
static Node *relational(void);
static Node *stmt(void);
static Node *unary(void);
static Node* newNodeFor(void);
static Node* newNodeIf(void);
static Node* newNodeReturn(void);
static Node* newNodeWhile(void);
static Type *pointerTo(Type *base);
static Var *findVar(Token *tok);
static Var *newVar(Type *ty);
static bool isInteger(Type *ty);
static void addType(Node *node);

void parse() {
  expect("{");
  prog.body = cmpndStmt();
}

Node *cmpndStmt(void) {
  Node head = {0};
  Node *cur = &head;
  while (!consume("}")) {
    if (consumeIdentMatch("int")) {
      cur = cur->next = declaration();
    } else {
      cur = cur->next = stmt();
    }
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
    node = cmpndStmt();
  } else if (consumeIf()) {
    node = newNodeIf();
  } else if (consumeFor()) {
    node = newNodeFor();
  } else if (consumeWhile()) {
    node = newNodeWhile();
  } else if (consumeReturn()) {
    node = newNodeReturn();
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
  rhs = newNode(ND_MUL, rhs, newNodeNum(8));
  return newNode(ND_ADD, lhs, rhs);
}

Node *newNodeSub(Node *lhs, Node *rhs) {
  addType(lhs);
  addType(rhs);
  if (isInteger(lhs->ty) && isInteger(rhs->ty)) {
    return newNode(ND_SUB, lhs, rhs);
  }
  if (lhs->ty->base && isInteger(rhs->ty)) {
    rhs = newNode(ND_MUL, rhs, newNodeNum(8));
    addType(rhs);
    Node *node = newNode(ND_SUB, lhs, rhs);
    node->ty = lhs->ty;
    return node;
  }
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = newNode(ND_SUB, lhs, rhs);
    node->ty = ty_int;
    return newNode(ND_DIV, node, newNodeNum(8));
  }
  compError("invalid operands");
  assert(false);
  return NULL;
}

Node *newNodeIdent(Token *tok) {
  Node *node = calloc(1, sizeof(Node));

  if (consume("(")) {
    expect(")");
    node->kind = ND_FUNCCALL;
    node->funcname = strndup(tok->str, tok->len);
    return node;
  }

  node->kind = ND_VAR;
  Var *var = findVar(tok);
  if (!var) {
    compErrorToken(tok->str, "undefined variable");
  }
  node->var = var;
  return node;
}

Var *newVar(Type *ty) {
  Token *tok = consumeIdent();
  if (!tok) {
    compErrorToken(tok->str, "expected identifier");
  }
  Var *var = calloc(1, sizeof(Var));
  var->next = prog.locals;
  var->name = tok->str;
  var->len = tok->len;
  var->offset = (prog.locals ? prog.locals->offset + 8 : 0);
  var->ty = ty;
  prog.locals = var;
  prog.stack_size += 8;
  return var;
}

Node *declaration(void) {
  // Type *basety = declspec();

  Node head = {0};
  Node *cur = &head;
  bool first = true;

  while (!consume(";")) {
    if (!first) {
      expect(",");
    }
    first = false;

    // Type *ty = declarator(basety);
    Type *ty = ty_int;
    Var *var = newVar(ty);

    if (!consume("=")) {
      continue;
    }

    Node *lhs = calloc(1, sizeof(Node));
    lhs->kind = ND_VAR;
    lhs->var = var;
    Node *rhs = assign();
    Node *node = newNode(ND_ASS, lhs, rhs);
    cur = cur->next = node;
  }

  Node *node = newNode(ND_BLK, NULL, NULL);
  node->body = head.next;
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

Var *findVar(Token *tok) {
  for (Var *var = prog.locals; var; var = var->next) {
    if (var->len == tok->len && memcmp(tok->str, var->name, var->len) == 0) {
      return var;
    }
  }
  return NULL;
}

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
  case ND_FUNCCALL:
    node->ty = ty_int;
    break;
  case ND_VAR:
    node->ty = node->var->ty;
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

Node* newNodeIf(void) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_IF;
  expect("(");
  node->cond = expr();
  expect(")");
  node->then = stmt();
  if (consumeElse()) {
    node->els = stmt();
  }
  return node;
}

Node* newNodeWhile(void) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_WHILE;
  expect("(");
  node->cond = expr();
  expect(")");
  node->body = stmt();
  return node;
}

Node* newNodeFor(void) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_FOR;
  expect("(");
  node->pre = stmt();
  node->cond = stmt();
  if (!consume(")")) {
    node->post = expr();
    expect(")");
  }
  node->body = stmt();
  return node;
}

Node* newNodeReturn(void) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_RET;
  node->lhs = expr();
  expect(";");
  return node;
}
