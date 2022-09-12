#include "parse.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp_err.h"
#include "defs.h"
#include "tokenise.h"

#define TY_INT_CMPND_LIT                                                       \
  &(Type) { .kind = TY_INT, .size = 4, .base = NULL, .align = 4 }
#define TY_LONG_CMPND_LIT                                                      \
  &(Type) { .kind = TY_LONG, .size = 8, .base = NULL, .align = 8 }
#define TY_CHAR_CMPND_LIT                                                      \
  &(Type) { .kind = TY_CHAR, .size = 1, .base = NULL, .align = 1 }
#define TY_SHORT_CMPND_LIT                                                     \
  &(Type) { .kind = TY_SHORT, .size = 2, .base = NULL, .align = 2 }

extern Type *ty_int;
extern Token *token;
Obj *prog = NULL;
Obj *globals = NULL;
static struct {
  char *kwd;
  Type *ty;
} ty_kwd_map[] = {{.kwd = "int", .ty = TY_INT_CMPND_LIT},
                  {.kwd = "char", .ty = TY_CHAR_CMPND_LIT},
                  {.kwd = "short", .ty = TY_SHORT_CMPND_LIT},
                  {.kwd = "long", .ty = TY_LONG_CMPND_LIT}};
Type *ty_int = TY_INT_CMPND_LIT;
Type *ty_long = TY_LONG_CMPND_LIT;
Type *ty_char = TY_CHAR_CMPND_LIT;
Type *ty_short = TY_SHORT_CMPND_LIT;
static Obj *cur_fn = NULL;
static Scope *scopes = &(Scope){0};

static Node *add(void);
static Node *assign(void);
static Node *cmpndStmt(void);
static Node *declaration(void);
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
static Node *newNodeNum(int64_t val);
static Node *newNodeReturn(void);
static Node *newNodeSub(Node *lhs, Node *rhs);
static Node *newNodeWhile(void);
static Node *newNodeDeref(Node *body);
static Node *newNodeMember(Node *body);
static Node *primary(void);
static Node *relational(void);
static Node *stmt(void);
static Node *unary(void);
static Type *pointerTo(Type *base);
static Type *arrayOf(Type *base, size_t len);
static Obj *findVar(Token *tok);
static Type *findTag(Token *tok);
static Obj *newVar(Type *ty, Obj **vars);
static Obj *newLocalVar(Type *ty);
static Obj *newGlobalVar(Type *ty);
static Obj *newStrLitVar(Token *tok, Type *ty);
static void newParam(Type *ty);
static bool isInteger(Type *ty);
static void addType(Node *node);
static Type *declspec(void);
static Type *declarator(Type *ty);
static Obj *function(Type *ty);
static Type *typeSuffix(Type *ty);
static void globalVar(Type *ty);
static void enterScope(void);
static void exitScope(void);
static void pushScope(Obj *var);
static size_t alignTo(size_t n, size_t align);
static Type *newType(TypeKind kind, size_t size, size_t align);
static Type *structUnionDecl(Type *ty);
static Type *structDecl(Type *ty);
static Type *unionDecl(Type *ty);
static void pushTagScope(Token *tok, Type *ty);
static Node *structRef(Node *node);

void parse() {
  Obj head = {0};
  Obj *cur = &head;
  while (!isEOF()) {
    Type *ty = declspec();
    if (isFunc()) {
      cur = cur->next = function(ty);
      continue;
    }
    globalVar(ty);
  }
  prog = head.next;
}

Node *cmpndStmt(void) {
  Node head = {0};
  Node *cur = &head;
  enterScope();
  while (!consume("}")) {
    if (getType(token->str, token->len)) {
      cur = cur->next = declaration();
    } else {
      cur = cur->next = stmt();
    }
    addType(cur);
  }
  exitScope();
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

Node *newNodeNum(int64_t val) {
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
  Obj *var = findVar(tok);
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

Obj *newVar(Type *ty, Obj **vars) {
  Token *tok = expectIdent();
  Obj *var = calloc(1, sizeof(Obj));
  var->next = *vars;
  var->name = strndup(tok->str, tok->len);
  var->ty = typeSuffix(ty);
  *vars = var;
  size_t offset = 0;
  for (Obj *ext_var = *vars; ext_var; ext_var = ext_var->next) {
    offset += ext_var->ty->size;
    offset = alignTo(offset, ext_var->ty->align);
    ext_var->offset = offset;
  }
  cur_fn->stack_size = alignTo(offset, 16);
  pushScope(var);
  return var;
}

Obj *newGlobalVar(Type *ty) {
  Token *tok = expectIdent();
  Obj *var = calloc(1, sizeof(Obj));
  var->next = globals;
  var->name = strndup(tok->str, tok->len);
  var->ty = typeSuffix(ty);
  var->is_global = true;
  globals = var;
  pushScope(var);
  return var;
}

Obj *newStrLitVar(Token *tok, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->next = globals;
  static size_t id = 0;
  var->name = calloc(1, 20); // TODO: 20?
  sprintf(var->name, ".L..%zu", id++);
  var->init_data = tok->str;
  var->ty = ty;
  var->is_global = true;
  globals = var;
  pushScope(var);
  return var;
}

Obj *newLocalVar(Type *ty) { return newVar(ty, &cur_fn->locals); }

void newParam(Type *ty) {
  newVar(ty, &cur_fn->params);
  cur_fn->param_cnt++;
}

Type *getType(const char *kwd, size_t len) {
  static const char struct_kwd[] = "struct";
  static const char union_kwd[] = "union";
  if ((sizeof(struct_kwd) - 1) == len && strncmp(kwd, struct_kwd, len) == 0) {
    return newType(TY_STRUCT, 0, 1);
  }
  if ((sizeof(union_kwd) - 1) == len && strncmp(kwd, union_kwd, len) == 0) {
    return newType(TY_UNION, 0, 1);
  }
  for (size_t i = 0; i < sizeof(ty_kwd_map) / sizeof(*ty_kwd_map); ++i) {
    if (strlen(ty_kwd_map[i].kwd) == len &&
        strncmp(kwd, ty_kwd_map[i].kwd, len) == 0) {
      return ty_kwd_map[i].ty;
    }
  }
  return NULL;
}

Type *declspec(void) {
  Token *ident = expectIdent();
  Type *ty = getType(ident->str, ident->len);
  if (!ty) {
    compErrorToken(ident->str, "unidentified type");
  }
  switch (ty->kind) {
  case TY_STRUCT:
    ty = structDecl(ty);
    break;
  case TY_UNION:
    ty = unionDecl(ty);
    break;
  default:
    break;
  }
  return ty;
}

Type *structUnionDecl(Type *ty) {
  Token *tag = consumeIdent();
  if (tag) {
    if (!consume("{")) {
      ty = findTag(tag);
      if (!ty) {
        compErrorToken(tag->str, "unkown struct/union tag");
      }
      return ty;
    }
  } else {
    expect("{");
  }

  Obj head = {0};
  Obj *cur = &head;
  while (!consume("}")) {
    Type *mem_ty = declspec();
    bool first = true;
    while (!consume(";")) {
      if (!first) {
        expect(",");
      }
      first = false;
      Token *tok = expectIdent();
      Obj *mem = calloc(1, sizeof(Obj));
      mem->name = strndup(tok->str, tok->len);
      mem->ty = typeSuffix(declarator(mem_ty));
      cur = cur->next = mem;
    }
  }
  ty->members = head.next;
  ty->align = 1;

  if (tag) {
    pushTagScope(tag, ty);
  }

  return ty;
}

Type *structDecl(Type *ty) {
  ty = structUnionDecl(ty);

  size_t offset = 0;
  for (Obj *mem = ty->members; mem; mem = mem->next) {
    offset = alignTo(offset, mem->ty->align);
    mem->offset = offset;
    offset += mem->ty->size;
    if (mem->ty->align > ty->align) {
      ty->align = mem->ty->align;
    }
  }
  ty->size = alignTo(offset, ty->align);

  return ty;
}

Type *unionDecl(Type *ty) {
  ty = structUnionDecl(ty);

  for (Obj *mem = ty->members; mem; mem = mem->next) {
    if (mem->ty->align > ty->align) {
      ty->align = mem->ty->align;
    }
    if (mem->ty->size > ty->size) {
      ty->size = mem->ty->size;
    }
  }
  ty->size = alignTo(ty->size, ty->align);

  return ty;
}

Type *declarator(Type *ty) {
  while (consume("*")) {
    ty = pointerTo(ty);
  }
  return ty;
}

Obj *function(Type *ty) {
  ty = declarator(ty);

  Token *fn_ident = expectIdent();

  Obj *fn = calloc(1, sizeof(Obj));
  fn->ty = ty;
  fn->name = strndup(fn_ident->str, fn_ident->len);
  cur_fn = fn;
  enterScope();

  expect("(");
  bool first = true;
  while (!consume(")")) {
    if (!first) {
      expect(",");
    }
    first = false;
    Type *ty = declspec();
    ty = declarator(ty);
    newParam(ty);
  }
  expect("{");

  fn->locals = fn->params;
  fn->body = cmpndStmt();
  exitScope();
  return fn;
}

Node *declaration(void) {
  Type *basety = declspec();

  Node head = {0};
  Node *cur = &head;
  bool first = true;

  while (!consume(";")) {
    if (!first) {
      expect(",");
    }
    first = false;

    Type *ty = declarator(basety);
    Obj *var = newLocalVar(ty);

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
    if (consume("{")) {
      Node *node = newNode(ND_STMT_EXPR);
      node->body = cmpndStmt()->body;
      expect(")");
      return node;
    }
    Node *node = expr();
    expect(")");
    return node;
  }
  if (consumeSizeof()) {
    Node *node = unary();
    addType(node);
    return newNodeNum((int)node->ty->size);
  }
  tok = consumeStrLit();
  if (tok) {
    Type *ty = arrayOf(ty_char, tok->len);
    Obj *var = newStrLitVar(tok, ty);
    Node *node = newNode(ND_VAR);
    node->var = var;
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

Node *expr(void) {
  Node *node = assign();
  if (consume(",")) {
    return newNodeBinary(ND_COMMA, node, expr());
  }
  return node;
}

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

Obj *findVar(Token *tok) {
  for (Scope *sc = scopes; sc; sc = sc->next) {
    for (VarScope *vs = sc->vars; vs; vs = vs->next) {
      if (strlen(vs->var->name) == tok->len &&
          memcmp(tok->str, vs->var->name, strlen(vs->var->name)) == 0) {
        return vs->var;
      }
    }
  }
  return NULL;
}

Type *findTag(Token *tok) {
  for (Scope *sc = scopes; sc; sc = sc->next) {
    for (TagScope *ts = sc->tags; ts; ts = ts->next) {
      if (strlen(ts->name) == tok->len &&
          memcmp(tok->str, ts->name, tok->len) == 0) {
        return ts->ty;
      }
    }
  }
  return NULL;
}

bool isInteger(Type *ty) {
  switch (ty->kind) {
  case TY_INT:
  case TY_CHAR:
  case TY_LONG:
  case TY_SHORT:
    return true;
  default:
    break;
  }
  return false;
}

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
    node->ty = ty_long;
    break;
  case ND_VAR:
    node->ty = node->var->ty;
    break;
  case ND_COMMA:
    node->ty = node->rhs->ty;
    return;
  case ND_MEMBER:
    node->ty = node->var->ty;
    return;
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
  case ND_STMT_EXPR:
    if (node->body) {
      Node *stmt = node->body;
      while (stmt->next) {
        stmt = stmt->next;
      }
      node->ty = stmt->ty;
    }
    return;
  default:
    break;
  }
}

Type *pointerTo(Type *base) {
  Type *ty = newType(TY_PTR, 8, 8);
  ty->base = base;
  return ty;
}

Type *arrayOf(Type *base, size_t len) {
  Type *ty = newType(TY_ARR, base->size * len, base->align);
  ty->base = base;
  ty->arr_len = len;
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

Node *newNodeMember(Node *body) {
  Node *node = newNode(ND_MEMBER);
  node->lhs = body;
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

Node *structRef(Node *node) {
  Token *tok = expectIdent();
  addType(node);
  if (node->ty->kind != TY_STRUCT && node->ty->kind != TY_UNION) {
    compErrorToken(node->tok->str, "not a struct or a union");
  }
  Node *mem_node = newNodeMember(node);
  for (Obj *mem = node->ty->members; mem; mem = mem->next) {
    if (strlen(mem->name) == tok->len &&
        strncmp(mem->name, tok->str, tok->len) == 0) {
      mem_node->var = mem;
      break;
    }
  }
  if (!mem_node->var) {
    compErrorToken(tok->str, "no such member");
  }
  return mem_node;
}

Node *postfix(void) {
  Node *node = primary();
  for (;;) {
    if (consume("[")) {
      Node *idx = expr();
      expect("]");
      node = newNodeDeref(newNodeAdd(node, idx));
      continue;
    }
    if (consume(".")) {
      node = structRef(node);
      continue;
    }
    if (consume("->")) {
      node = newNodeDeref(node);
      node = structRef(node);
      continue;
    }
    break;
  }
  return node;
}

void globalVar(Type *base_ty) {
  bool first = true;
  while (!consume(";")) {
    if (!first) {
      expect(",");
    }
    first = false;
    Type *ty = declarator(base_ty);
    newGlobalVar(ty);
  }
}

void enterScope(void) {
  Scope *sc = calloc(1, sizeof(Scope));
  sc->next = scopes;
  scopes = sc;
}

void exitScope(void) { scopes = scopes->next; }

void pushScope(Obj *var) {
  VarScope *sc = calloc(1, sizeof(VarScope));
  sc->var = var;
  sc->next = scopes->vars;
  scopes->vars = sc;
}

size_t alignTo(size_t n, size_t align) {
  return (n + align - 1) / align * align;
}

Type *newType(TypeKind kind, size_t size, size_t align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

void pushTagScope(Token *tok, Type *ty) {
  TagScope *sc = calloc(1, sizeof(TagScope));
  sc->name = strndup(tok->str, tok->len);
  sc->ty = ty;
  sc->next = scopes->tags;
  scopes->tags = sc;
}
