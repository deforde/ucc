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
extern Token *token;
Function *prog = NULL;
Type *ty_int = &(Type){.kind = TY_INT, .size = 8, .base = NULL};
static Function *cur_fn = NULL;

static Node *add(void);
static Node *assign(void);
static Node *cmpndStmt(void);
static Node *declaration(Token *ident);
static Node *equality(void);
static Node *expr(void);
static Node *funcCall(Token *tok);
static Node *mul(void);
static Node *postfix(void);
static Node *newNode(NodeKind kind);
static Node *newNodeBinary(NodeKind kind, Node *lhs, Node *rhs);
static Node *newNodeAdd(Node *lhs, Node *rhs);
static Node *newNodeFor(void);
static Node *newNodeIdent(Token *tok);
static Node *newNodeIf(void);
static Node *newNodeNum(int val);
static Node *newNodeReturn(void);
static Node *newNodeSub(Node *lhs, Node *rhs);
static Node *newNodeWhile(void);
static Node *newNodeDeref(Node *body);
static Node *primary(void);
static Node *relational(void);
static Node *stmt(void);
static Node *unary(void);
static Type *pointerTo(Type *base);
static Type *arrayOf(Type *base, size_t len);
static Var *findVar(Token *tok);
static Var *newVar(Type *ty, Var **vars);
static Var *newLocalVar(Type *ty);
static void newParam(Type *ty);
static bool isInteger(Type *ty);
static void addType(Node *node);
static Type *declspec(Token *ident);
static Type *declarator(Type *ty);
static Function *function(void);
static Type *typeSuffix(Type *ty);

void parse() {
  Function head = {0};
  Function *cur = &head;
  while (!isEOF()) {
    cur = cur->next = function();
  }
  prog = head.next;
}

Node *cmpndStmt(void) {
  Node head = {0};
  Node *cur = &head;
  while (!consume("}")) {
    Token *tok = NULL;
    if ((tok = consumeTypeIdent())) {
      cur = cur->next = declaration(tok);
    } else {
      cur = cur->next = stmt();
    }
    addType(cur);
  }
  Node *node = newNode(ND_BLK);
  node->body = head.next;
  return node;
}

Node *assign(void) {
  Node *node = equality();
  if (consume("=")) {
    node = newNodeBinary(ND_ASS, node, assign());
  }
  return node;
}

Node *stmt(void) {
  Node *node = NULL;
  if (consume(";")) {
    node = newNode(ND_BLK);
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

Node *newNode(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = token;
  return node;
}

Node *newNodeBinary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = newNode(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *newNodeNum(int val) {
  Node *node = newNode(ND_NUM);
  node->val = val;
  return node;
}

Node *newNodeAdd(Node *lhs, Node *rhs) {
  addType(lhs);
  addType(rhs);
  if (isInteger(lhs->ty) && isInteger(rhs->ty)) {
    return newNodeBinary(ND_ADD, lhs, rhs);
  }
  if (lhs->ty->base && rhs->ty->base) {
    compErrorToken(lhs->tok->str, "invalid operands");
  }
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }
  rhs = newNodeBinary(ND_MUL, rhs, newNodeNum((int)lhs->ty->base->size));
  return newNodeBinary(ND_ADD, lhs, rhs);
}

Node *newNodeSub(Node *lhs, Node *rhs) {
  addType(lhs);
  addType(rhs);
  if (isInteger(lhs->ty) && isInteger(rhs->ty)) {
    return newNodeBinary(ND_SUB, lhs, rhs);
  }
  if (lhs->ty->base && isInteger(rhs->ty)) {
    rhs = newNodeBinary(ND_MUL, rhs, newNodeNum((int)lhs->ty->base->size));
    addType(rhs);
    Node *node = newNodeBinary(ND_SUB, lhs, rhs);
    node->ty = lhs->ty;
    return node;
  }
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = newNodeBinary(ND_SUB, lhs, rhs);
    node->ty = ty_int;
    return newNodeBinary(ND_DIV, node, newNodeNum((int)lhs->ty->base->size));
  }
  compErrorToken(lhs->tok->str, "invalid operands");
  assert(false);
  return NULL;
}

Node *newNodeIdent(Token *tok) {

  if (consume("(")) {
    return funcCall(tok);
  }

  Node *node = newNode(ND_VAR);
  Var *var = findVar(tok);
  if (!var) {
    compErrorToken(tok->str, "undefined variable");
  }
  node->var = var;
  return node;
}

Type *typeSuffix(Type *ty) {
  if (consume("[")) {
    const size_t size = expectNumber();
    expect("]");
    ty = typeSuffix(ty);
    return arrayOf(ty, size);
  }
  return ty;
}

Var *newVar(Type *ty, Var **vars) {
  Token *tok = expectIdent();
  Var *var = calloc(1, sizeof(Var));
  var->next = (*vars);
  var->name = tok->str;
  var->len = tok->len;
  var->ty = typeSuffix(ty);
  var->offset = ((*vars) ? (*vars)->offset + var->ty->size : var->ty->size);
  (*vars) = var;
  cur_fn->stack_size = var->offset + var->ty->size;
  return var;
}

Var *newLocalVar(Type *ty) { return newVar(ty, &cur_fn->locals); }

void newParam(Type *ty) {
  newVar(ty, &cur_fn->params);
  cur_fn->param_cnt++;
}

Type *declspec(Token *ident) {
  // TODO: Add mapping of type keywords to Type objects
  if (strncmp(ident->str, "int", ident->len) != 0) {
    compErrorToken(ident->str, "expected 'int'");
  }
  return ty_int;
}

Type *declarator(Type *ty) {
  while (consume("*")) {
    ty = pointerTo(ty);
  }
  return ty;
}

Function *function(void) {
  Token *ty_ident = expectIdent();
  Type *ty = declspec(ty_ident);
  ty = declarator(ty);

  Token *fn_ident = expectIdent();

  Function *fn = calloc(1, sizeof(Function));
  fn->ret_ty = ty;
  fn->name = strndup(fn_ident->str, fn_ident->len);
  cur_fn = fn;

  expect("(");
  bool first = true;
  while (!consume(")")) {
    if (!first) {
      expect(",");
    }
    first = false;
    Token *ty_ident = expectIdent();
    Type *ty = declspec(ty_ident);
    ty = declarator(ty);
    newParam(ty);
  }
  expect("{");

  fn->locals = fn->params;
  fn->body = cmpndStmt();
  return fn;
}

Node *declaration(Token *ident) {
  Type *basety = declspec(ident);

  Node head = {0};
  Node *cur = &head;
  bool first = true;

  while (!consume(";")) {
    if (!first) {
      expect(",");
    }
    first = false;

    Type *ty = declarator(basety);
    Var *var = newLocalVar(ty);

    if (!consume("=")) {
      continue;
    }

    Node *lhs = newNode(ND_VAR);
    lhs->var = var;
    Node *rhs = assign();
    Node *node = newNodeBinary(ND_ASS, lhs, rhs);
    cur = cur->next = node;
  }

  Node *node = newNode(ND_BLK);
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
      node = newNodeBinary(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = newNodeBinary(ND_DIV, node, unary());
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
      node = newNodeBinary(ND_EQ, node, relational());
    } else if (consume("!=")) {
      node = newNodeBinary(ND_NE, node, relational());
    } else {
      return node;
    }
  }
}

Node *relational(void) {
  Node *node = add();
  for (;;) {
    if (consume("<")) {
      node = newNodeBinary(ND_LT, node, add());
    } else if (consume("<=")) {
      node = newNodeBinary(ND_LE, node, add());
    } else if (consume(">")) {
      node = newNodeBinary(ND_LT, add(), node);
    } else if (consume(">=")) {
      node = newNodeBinary(ND_LE, add(), node);
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
    return newNodeBinary(ND_SUB, newNodeNum(0), unary());
  }
  if (consume("*")) {
    return newNodeDeref(unary());
  }
  if (consume("&")) {
    Node *node = newNode(ND_ADDR);
    node->body = unary();
    return node;
  }
  return postfix();
}

Var *findVar(Token *tok) {
  for (Var *var = cur_fn->locals; var; var = var->next) {
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
  for (Node *n = node->args; n; n = n->next) {
    addType(n);
  }

  switch (node->kind) {
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
    node->ty = node->lhs->ty;
    break;
  case ND_ASS:
    if (node->lhs->ty->kind == TY_ARR) {
      compErrorToken(node->tok->str, "not an lvalue");
    }
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
    if (node->body->ty->kind == TY_ARR) {
      node->ty = pointerTo(node->body->ty->base);
    } else {
      node->ty = pointerTo(node->body->ty);
    }
    break;
  case ND_DEREF:
    if (!node->body->ty->base) {
      compErrorToken(node->tok->str, "invalid pointer dereference");
    }
    node->ty = node->body->ty->base;
    break;
  default:
    break;
  }
}

Type *pointerTo(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->base = base;
  ty->size = 8;
  return ty;
}

Type *arrayOf(Type *base, size_t len) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_ARR;
  ty->arr_len = len;
  ty->base = base;
  ty->size = base->size * ty->arr_len;
  return ty;
}

Node *newNodeIf(void) {
  Node *node = newNode(ND_IF);
  expect("(");
  node->cond = expr();
  expect(")");
  node->then = stmt();
  if (consumeElse()) {
    node->els = stmt();
  }
  return node;
}

Node *newNodeWhile(void) {
  Node *node = newNode(ND_WHILE);
  expect("(");
  node->cond = expr();
  expect(")");
  node->body = stmt();
  return node;
}

Node *newNodeDeref(Node *body) {
  Node *node = newNode(ND_DEREF);
  node->body = body;
  return node;
}

Node *newNodeFor(void) {
  Node *node = newNode(ND_FOR);
  expect("(");
  if (!consume(";")) {
    node->pre = expr();
    expect(";");
  }
  if (!consume(";")) {
    node->cond = expr();
    expect(";");
  }
  if (!consume(")")) {
    node->post = expr();
    expect(")");
  }
  node->body = stmt();
  return node;
}

Node *newNodeReturn(void) {
  Node *node = newNode(ND_RET);
  node->lhs = expr();
  expect(";");
  return node;
}

Node *funcCall(Token *tok) {
  Node head = {0};
  Node *cur = &head;

  while (!consume(")")) {
    if (cur != &head) {
      expect(",");
    }
    cur = cur->next = assign();
  }

  Node *node = newNode(ND_FUNCCALL);
  node->funcname = strndup(tok->str, tok->len);
  node->args = head.next;
  return node;
}

Node *postfix(void) {
  Node *node = primary();
  while (consume("[")) {
    Node *idx = expr();
    expect("]");
    node = newNodeDeref(newNodeAdd(node, idx));
  }
  return node;
}
