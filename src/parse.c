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
static Obj *cur_fn = NULL;
static Scope *scopes = &(Scope){0};
Obj *prog = NULL;
Obj *globals = NULL;
Type *ty_char = &(Type){.kind = TY_CHAR, .size = 1, .align = 1};
Type *ty_bool = &(Type){.kind = TY_BOOL, .size = 1, .align = 1};
Type *ty_int = &(Type){.kind = TY_INT, .size = 4, .align = 4};
Type *ty_long = &(Type){.kind = TY_LONG, .size = 8, .align = 8};
Type *ty_short = &(Type){.kind = TY_SHORT, .size = 2, .align = 2};
Type *ty_void = &(Type){.kind = TY_VOID, .size = 1, .align = 1};

static Node * bitor (void);
static Node *add(void);
static Node *assign(void);
static Node *bitand(void);
static Node *bitxor(void);
static Node *cast(void);
static Node *cmpndStmt(void);
static Node *declaration(Type *basety);
static Node *equality(void);
static Node *expr(void);
static Node *funcCall(Token *tok);
static Node *logand(void);
static Node *logor(void);
static Node *mul(void);
static Node *newNode(NodeKind kind);
static Node *newNodeAdd(Node *lhs, Node *rhs);
static Node *newNodeAddr(Node *body);
static Node *newNodeBinary(NodeKind kind, Node *lhs, Node *rhs);
static Node *newNodeCast(Node *expr, Type *ty);
static Node *newNodeDeref(Node *body);
static Node *newNodeFor(void);
static Node *newNodeIdent(Token *tok);
static Node *newNodeIf(void);
static Node *newNodeInc(Node *node, int i);
static Node *newNodeLong(int64_t val);
static Node *newNodeMember(Node *body);
static Node *newNodeNum(int64_t val);
static Node *newNodeReturn(void);
static Node *newNodeSub(Node *lhs, Node *rhs);
static Node *newNodeVar(Obj *var);
static Node *newNodeWhile(void);
static Node *postfix(void);
static Node *primary(void);
static Node *relational(void);
static Node *stmt(void);
static Node *structRef(Node *node);
static Node *toAssign(Node *node);
static Node *unary(void);
static Obj *function(Type *ty, VarAttr *attr);
static Obj *newGlobalVar(Type *ty, Token *ident);
static Obj *newLocalVar(Type *ty, Token *ident);
static Obj *newStrLitVar(Token *tok, Type *ty);
static Obj *newVar(Type *ty, Token *ident, Obj **vars);
static Type *abstractDeclarator(Type *ty);
static Type *arrayDimensions(Type *ty);
static Type *arrayOf(Type *base, size_t len);
static Type *declarator(Type *ty, Token **ident);
static Type *declspec(VarAttr *attr);
static Type *enumSpec();
static Type *findTag(Token *tok);
static Type *findTypedef(Token *tok);
static Type *getCommonType(Type *ty1, Type *ty2);
static Type *newType(TypeKind kind, ssize_t size, size_t align);
static Type *pointerTo(Type *base);
static Type *structDecl(Type *ty);
static Type *structUnionDecl(Type *ty);
static Type *typeSuffix(Type *ty);
static Type *typename(void);
static Type *unionDecl(Type *ty);
static VarScope *findVarScope(Token *tok);
static bool equal(Token *tok, const char *str);
static bool isFunc(void);
static bool isTypename(Token *tok);
static size_t alignTo(size_t n, size_t align);
static void addType(Node *node);
static void enterScope(void);
static void exitScope(void);
static void globalVar(Type *ty);
static void newParam(Type *ty, Token *ident);
static void parseTypedef(Type *basety);
static void pushScope(char *name, Obj *var, Type *type_def);
static void pushTagScope(Token *tok, Type *ty);
static void usualArithConv(Node **lhs, Node **rhs);

void parse() {
  Obj head = {0};
  Obj *cur = &head;
  while (!isEOF()) {
    VarAttr attr = {0};
    Type *ty = declspec(&attr);
    if (attr.is_typedef) {
      parseTypedef(ty);
      continue;
    }
    if (isFunc()) {
      cur = cur->next = function(ty, &attr);
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
    if (isTypename(token)) {
      VarAttr attr = {0};
      Type *basety = declspec(&attr);
      if (attr.is_typedef) {
        parseTypedef(basety);
        continue;
      }
      cur = cur->next = declaration(basety);
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
  Node *node = logor();
  if (consume("=")) {
    node = newNodeBinary(ND_ASS, node, assign());
  }
  if (consume("+=")) {
    return toAssign(newNodeAdd(node, assign()));
  }
  if (consume("-=")) {
    return toAssign(newNodeSub(node, assign()));
  }
  if (consume("*=")) {
    return toAssign(newNodeBinary(ND_MUL, node, assign()));
  }
  if (consume("/=")) {
    return toAssign(newNodeBinary(ND_DIV, node, assign()));
  }
  if (consume("%=")) {
    return toAssign(newNodeBinary(ND_MOD, node, assign()));
  }
  if (consume("&=")) {
    return toAssign(newNodeBinary(ND_BITAND, node, assign()));
  }
  if (consume("|=")) {
    return toAssign(newNodeBinary(ND_BITOR, node, assign()));
  }
  if (consume("^=")) {
    return toAssign(newNodeBinary(ND_BITXOR, node, assign()));
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

Node *newNodeLong(int64_t val) {
  Node *node = newNodeNum(val);
  node->ty = ty_long;
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
  rhs = newNodeBinary(ND_MUL, rhs, newNodeLong((int64_t)lhs->ty->base->size));
  return newNodeBinary(ND_ADD, lhs, rhs);
}

Node *newNodeSub(Node *lhs, Node *rhs) {
  addType(lhs);
  addType(rhs);
  if (isInteger(lhs->ty) && isInteger(rhs->ty)) {
    return newNodeBinary(ND_SUB, lhs, rhs);
  }
  if (lhs->ty->base && isInteger(rhs->ty)) {
    rhs = newNodeBinary(ND_MUL, rhs, newNodeLong((int64_t)lhs->ty->base->size));
    addType(rhs);
    Node *node = newNodeBinary(ND_SUB, lhs, rhs);
    node->ty = lhs->ty;
    return node;
  }
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = newNodeBinary(ND_SUB, lhs, rhs);
    node->ty = ty_int;
    return newNodeBinary(ND_DIV, node,
                         newNodeNum((int64_t)lhs->ty->base->size));
  }
  compErrorToken(lhs->tok->str, "invalid operands");
  assert(false);
  return NULL;
}

Node *newNodeIdent(Token *tok) {
  if (consume("(")) {
    return funcCall(tok);
  }

  VarScope *sc = findVarScope(tok);
  if (sc && sc->enum_ty) {
    return newNodeNum(sc->enum_val);
  }
  if (!sc || !sc->var) {
    compErrorToken(tok->str, "undefined variable");
  }

  return newNodeVar(sc->var);
}

Type *typeSuffix(Type *ty) {
  if (consume("[")) {
    return arrayDimensions(ty);
  }
  return ty;
}

Obj *newVar(Type *ty, Token *ident, Obj **vars) {
  Obj *var = calloc(1, sizeof(Obj));
  var->next = *vars;
  var->name = strndup(ident->str, ident->len);
  var->ty = ty;
  *vars = var;
  size_t offset = 0;
  for (Obj *ext_var = *vars; ext_var; ext_var = ext_var->next) {
    offset += ext_var->ty->size;
    offset = alignTo(offset, ext_var->ty->align);
    ext_var->offset = offset;
  }
  cur_fn->stack_size = alignTo(offset, 16);
  pushScope(var->name, var, NULL);
  return var;
}

Obj *newGlobalVar(Type *ty, Token *ident) {
  Obj *var = calloc(1, sizeof(Obj));
  var->next = globals;
  var->name = strndup(ident->str, ident->len);
  var->ty = ty;
  var->is_global = true;
  globals = var;
  pushScope(var->name, var, NULL);
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
  pushScope(var->name, var, NULL);
  return var;
}

Obj *newLocalVar(Type *ty, Token *ident) {
  return newVar(ty, ident, &cur_fn->locals);
}

void newParam(Type *ty, Token *ident) {
  newVar(ty, ident, &cur_fn->params);
  cur_fn->param_cnt++;
}

Type *declspec(VarAttr *attr) {
  enum {
    VOID = 1 << 0,
    BOOL = 1 << 2,
    CHAR = 1 << 4,
    SHORT = 1 << 6,
    INT = 1 << 8,
    LONG = 1 << 10,
    OTHER = 1 << 12,
  };

  size_t counter = 0;
  Type *ty = ty_int;

  while (isTypename(token)) {
    Token *tok = token;
    token = token->next;

    bool is_typedef = false;
    bool is_static = false;
    if ((is_typedef = equal(tok, "typedef")) ||
        (is_static = equal(tok, "static"))) {
      if (!attr) {
        compErrorToken(
            tok->str, "storage class specifier is not allowed in this context");
      }
      attr->is_typedef |= is_typedef;
      attr->is_static |= is_static;
      if (attr->is_static && attr->is_typedef) {
        compErrorToken(tok->str, "typedef and static may not be used together");
      }
      continue;
    }

    Type *typedef_ty = NULL;
    bool isStructKwd = false;
    bool isUnionKwd = false;
    bool isEnumKwd = false;
    if ((isStructKwd = equal(tok, "struct")) ||
        (isUnionKwd = equal(tok, "union")) ||
        (isEnumKwd = equal(tok, "enum")) || (typedef_ty = findTypedef(tok))) {
      if (counter) {
        token = tok;
        break;
      }
      if (isStructKwd) {
        ty = structDecl(newType(TY_STRUCT, 0, 1));
      } else if (isUnionKwd) {
        ty = unionDecl(newType(TY_UNION, 0, 1));
      } else if (isEnumKwd) {
        ty = enumSpec();
      } else {
        ty = typedef_ty;
      }
      counter += OTHER;
      continue;
    }

    if (equal(tok, "void")) {
      counter += VOID;
    } else if (equal(tok, "_Bool")) {
      counter += BOOL;
    } else if (equal(tok, "char")) {
      counter += CHAR;
    } else if (equal(tok, "short")) {
      counter += SHORT;
    } else if (equal(tok, "int")) {
      counter += INT;
    } else if (equal(tok, "long")) {
      counter += LONG;
    } else {
      assert(false);
    }

    switch (counter) {
    case VOID:
      ty = ty_void;
      break;
    case BOOL:
      ty = ty_bool;
      break;
    case CHAR:
      ty = ty_char;
      break;
    case SHORT:
    case SHORT + INT:
      ty = ty_short;
      break;
    case INT:
      ty = ty_int;
      break;
    case LONG:
    case LONG + INT:
    case LONG + LONG:
    case LONG + LONG + INT:
      ty = ty_long;
      break;
    default:
      compErrorToken(tok->str, "invalid type");
    }
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
    Type *mem_ty = declspec(NULL);
    bool first = true;
    while (!consume(";")) {
      if (!first) {
        expect(",");
      }
      first = false;
      Obj *mem = calloc(1, sizeof(Obj));
      Token *ident = NULL;
      mem->ty = declarator(mem_ty, &ident);
      mem->name = strndup(ident->str, ident->len);
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
  ty->size = (ssize_t)alignTo(offset, ty->align);

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
  ty->size = (ssize_t)alignTo(ty->size, ty->align);

  return ty;
}

Type *abstractDeclarator(Type *ty) {
  while (consume("*")) {
    ty = pointerTo(ty);
  }
  if (consume("(")) {
    Type *super_ty = abstractDeclarator(ty_int);
    expect(")");
    ty = typeSuffix(ty);
    if (super_ty->kind == TY_PTR || super_ty->kind == TY_ARR) {
      super_ty->base = ty;
      return super_ty;
    }
    return ty;
  }
  ty = typeSuffix(ty);
  return ty;
}

Type *declarator(Type *ty, Token **ident) {
  while (consume("*")) {
    ty = pointerTo(ty);
  }
  if (consume("(")) {
    Type *super_ty = declarator(ty_int, ident);
    expect(")");
    ty = typeSuffix(ty);
    if (super_ty->kind == TY_PTR || super_ty->kind == TY_ARR) {
      super_ty->base = ty;
      return super_ty;
    }
    return ty;
  }
  *ident = expectIdent();
  ty = typeSuffix(ty);
  return ty;
}

Obj *function(Type *ty, VarAttr *attr) {
  Token *fn_ident = NULL;
  ty = declarator(ty, &fn_ident);

  Obj *fn = calloc(1, sizeof(Obj));
  fn->ty = newType(TY_FUNC, 0, 0);
  fn->ty->ret_ty = ty;
  fn->name = strndup(fn_ident->str, fn_ident->len);
  fn->is_global = !attr->is_static;
  cur_fn = fn;

  Obj *var = calloc(1, sizeof(Obj));
  var->name = strndup(fn_ident->str, fn_ident->len);
  var->ty = fn->ty;
  var->is_global = fn->is_global;
  pushScope(var->name, var, NULL);

  enterScope();

  expect("(");
  bool first = true;
  while (!consume(")")) {
    if (!first) {
      expect(",");
    }
    first = false;
    Type *ty = declspec(NULL);
    Token *param_ident = NULL;
    ty = declarator(ty, &param_ident);
    newParam(ty, param_ident);
  }

  Type params_head = {0};
  Type *param_ty = &params_head;
  for (Obj *param = fn->params; param; param = param->next) {
    Type *new_param_ty = calloc(1, sizeof(Type));
    *new_param_ty = *param->ty;
    param_ty = param_ty->next = new_param_ty;
  }
  fn->ty->params = param_ty;

  if (!consume(";")) {
    expect("{");
    fn->locals = fn->params;
    fn->body = cmpndStmt();
  }

  exitScope();
  return fn;
}

Node *declaration(Type *basety) {
  Node head = {0};
  Node *cur = &head;
  bool first = true;

  while (!consume(";")) {
    if (!first) {
      expect(",");
    }
    first = false;

    Token *ident = NULL;
    Type *ty = declarator(basety, &ident);
    if (ty->size < 0) {
      compErrorToken(ident->str, "variable has incomplete type");
    }
    if (ty->kind == TY_VOID) {
      compError(ident->str, "variable declared void");
    }
    Obj *var = newLocalVar(ty, ident);

    if (!consume("=")) {
      continue;
    }

    Node *lhs = newNodeVar(var);
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
    if (equal(token, "(") && isTypename(token->next)) {
      expect("(");
      Type *ty = typename();
      expect(")");
      return newNodeNum((int64_t)ty->size);
    }
    Node *node = unary();
    addType(node);
    return newNodeNum((int64_t)node->ty->size);
  }
  tok = consumeStrLit();
  if (tok) {
    Type *ty = arrayOf(ty_char, tok->len);
    Obj *var = newStrLitVar(tok, ty);
    return newNodeVar(var);
  }
  return newNodeNum(expectNumber());
}

Node *mul(void) {
  Node *node = cast();
  for (;;) {
    if (consume("*")) {
      node = newNodeBinary(ND_MUL, node, cast());
    } else if (consume("/")) {
      node = newNodeBinary(ND_DIV, node, cast());
    } else if (consume("%")) {
      node = newNodeBinary(ND_MOD, node, cast());
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
    return cast();
  }
  if (consume("-")) {
    return newNodeBinary(ND_SUB, newNodeNum(0), cast());
  }
  if (consume("*")) {
    return newNodeDeref(cast());
  }
  if (consume("&")) {
    return newNodeAddr(cast());
  }
  if (consume("++")) {
    return toAssign(newNodeAdd(unary(), newNodeNum(1)));
  }
  if (consume("--")) {
    return toAssign(newNodeSub(unary(), newNodeNum(1)));
  }
  if (consume("!")) {
    Node *node = newNode(ND_NOT);
    node->lhs = cast();
    return node;
  }
  if (consume("~")) {
    Node *node = newNode(ND_BITNOT);
    node->lhs = cast();
    return node;
  }
  return postfix();
}

VarScope *findVarScope(Token *tok) {
  for (Scope *sc = scopes; sc; sc = sc->next) {
    for (VarScope *vs = sc->vars; vs; vs = vs->next) {
      if (strlen(vs->name) == tok->len &&
          memcmp(tok->str, vs->name, strlen(vs->name)) == 0) {
        return vs;
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
  case TY_BOOL:
  case TY_CHAR:
  case TY_ENUM:
  case TY_INT:
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
  case ND_NUM:
    node->ty = (node->val == (int)node->val) ? ty_int : ty_long;
    break;
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
    usualArithConv(&node->lhs, &node->rhs);
    node->ty = node->lhs->ty;
    break;
  case ND_ASS:
    if (node->lhs->ty->kind == TY_ARR) {
      compErrorToken(node->tok->str, "not an lvalue");
    }
    if (node->lhs->ty->kind != TY_STRUCT) {
      node->rhs = newNodeCast(node->rhs, node->lhs->ty);
    }
    node->ty = node->lhs->ty;
    break;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    usualArithConv(&node->lhs, &node->rhs);
    node->ty = ty_int;
    break;
  case ND_FUNCCALL:
    node->ty = ty_long;
    break;
  case ND_NOT:
  case ND_LOGOR:
  case ND_LOGAND:
    node->ty = ty_int;
    break;
  case ND_BITNOT:
    node->ty = node->lhs->ty;
    break;
  case ND_VAR:
    node->ty = node->var->ty;
    break;
  case ND_COMMA:
    node->ty = node->rhs->ty;
    break;
  case ND_MEMBER:
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
    if (node->body->ty->base->kind == TY_VOID) {
      compErrorToken(node->tok->str, "dereferencing void pointer");
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
    break;
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
  Type *ty = newType(TY_ARR, base->size * (ssize_t)len, base->align);
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
  enterScope();
  if (!consume(";")) {
    if (isTypename(token)) {
      Type *basety = declspec(NULL);
      node->pre = declaration(basety);
    } else {
      node->pre = expr();
      expect(";");
    }
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
  exitScope();
  return node;
}

Node *newNodeReturn(void) {
  Node *node = newNode(ND_RET);
  Node *exp = expr();
  expect(";");
  addType(exp);
  node->lhs = newNodeCast(exp, cur_fn->ty->ret_ty);
  return node;
}

Node *funcCall(Token *tok) {
  VarScope *sc = findVarScope(tok);
  if (!sc) {
    compErrorToken(tok->str, "implicit declaration of a function");
  }
  if (!sc->var || sc->var->ty->kind != TY_FUNC) {
    compErrorToken(tok->str, "not a function");
  }

  Type *ty = sc->var->ty;
  Type *param_ty = ty->params;
  Node head = {0};
  Node *cur = &head;

  while (!consume(")")) {
    if (cur != &head) {
      expect(",");
    }
    Node *arg = assign();
    addType(arg);

    if (param_ty) {
      if (param_ty->kind == TY_STRUCT || param_ty->kind == TY_UNION) {
        compErrorToken(arg->tok->str,
                       "struct or union function arguments are not supported");
      }
      arg = newNodeCast(arg, param_ty);
      param_ty = param_ty->next;
    }

    cur = cur->next = arg;
  }

  Node *node = newNode(ND_FUNCCALL);
  node->funcname = strndup(tok->str, tok->len);
  node->args = head.next;
  node->func_ty = ty;
  node->ty = ty->ret_ty;
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
    if (equal(tok, mem->name)) {
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
    if (consume("++")) {
      node = newNodeInc(node, 1);
      continue;
    }
    if (consume("--")) {
      node = newNodeInc(node, -1);
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
    Token *ident = NULL;
    Type *ty = declarator(base_ty, &ident);
    newGlobalVar(ty, ident);
  }
}

void enterScope(void) {
  Scope *sc = calloc(1, sizeof(Scope));
  sc->next = scopes;
  scopes = sc;
}

void exitScope(void) { scopes = scopes->next; }

void pushScope(char *name, Obj *var, Type *type_def) {
  VarScope *sc = calloc(1, sizeof(VarScope));
  sc->name = name;
  sc->var = var;
  sc->type_def = type_def;
  sc->next = scopes->vars;
  scopes->vars = sc;
}

size_t alignTo(size_t n, size_t align) {
  return (n + align - 1) / align * align;
}

Type *newType(TypeKind kind, ssize_t size, size_t align) {
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

bool equal(Token *tok, const char *str) {
  return tok->len == strlen(str) && strncmp(tok->str, str, tok->len) == 0;
}

void parseTypedef(Type *basety) {
  bool first = true;

  while (!consume(";")) {
    if (!first) {
      expect(",");
    }
    first = false;

    Token *ident = NULL;
    Type *ty = declarator(basety, &ident);

    pushScope(strndup(ident->str, ident->len), NULL, ty);
  }
}

Type *findTypedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    VarScope *sc = findVarScope(tok);
    if (sc) {
      return sc->type_def;
    }
  }
  return NULL;
}

bool isTypename(Token *tok) {
  if (tok->kind == TK_KWD) {
    return true;
  }
  return findTypedef(tok) != NULL;
}

Type *typename(void) {
  Type *ty = declspec(NULL);
  return abstractDeclarator(ty);
}

Node *newNodeCast(Node *expr, Type *ty) {
  addType(expr);
  Node *node = newNode(ND_CAST);
  node->lhs = expr;
  node->ty = ty;
  return node;
}

Node *cast(void) {
  if (equal(token, "(") && isTypename(token->next)) {
    Token *start = token;
    consume("(");
    Type *ty = typename();
    consume(")");
    Node *node = newNodeCast(cast(), ty);
    node->tok = start;
    return node;
  }
  return unary();
}

Type *getCommonType(Type *ty1, Type *ty2) {
  if (ty1->base) {
    return pointerTo(ty1->base);
  }
  if (ty1->size == 8 || ty2->size == 8) {
    return ty_long;
  }
  return ty_int;
}

void usualArithConv(Node **lhs, Node **rhs) {
  Type *ty = getCommonType((*lhs)->ty, (*rhs)->ty);
  *lhs = newNodeCast(*lhs, ty);
  *rhs = newNodeCast(*rhs, ty);
}

bool isFunc() {
  if (token->str[0] == ';') {
    return false;
  }
  Token *ident = NULL;
  Type dummy = {0};
  Token *start = token;
  declarator(&dummy, &ident);
  bool is_func = consume("(");
  token = start;
  return is_func;
}

Type *enumSpec() {
  Token *tag = consumeIdent();
  if (tag && !equal(token, "{")) {
    Type *ty = findTag(tag);
    if (!ty) {
      compErrorToken(tag->str, "unknown enum type");
    }
    if (ty->kind != TY_ENUM) {
      compErrorToken(tag->str, "not an enum tag");
    }
    return ty;
  }

  Type *ty = newType(TY_ENUM, 4, 4);
  expect("{");
  int val = 0;
  for (;;) {
    Token *econst = expectIdent();
    if (consume("=")) {
      val = (int)expectNumber();
    }
    pushScope(strndup(econst->str, econst->len), NULL, NULL);
    VarScope *sc = scopes->vars;
    sc->enum_ty = ty;
    sc->enum_val = val++;
    if (consume("}")) {
      break;
    }
    expect(",");
  }

  if (tag) {
    pushTagScope(tag, ty);
  }

  return ty;
}

Node *toAssign(Node *node) {
  addType(node->lhs);
  addType(node->rhs);

  Obj *var = calloc(1, sizeof(Obj));
  var->name = "";
  var->is_global = false;
  var->ty = pointerTo(node->lhs->ty);

  Node *expr1 = newNodeBinary(ND_ASS, newNodeVar(var), newNodeAddr(node->lhs));
  Node *expr2 = newNodeBinary(
      ND_ASS, newNodeDeref(newNodeVar(var)),
      newNodeBinary(node->kind, newNodeDeref(newNodeVar(var)), node->rhs));
  return newNodeBinary(ND_COMMA, expr1, expr2);
}

Node *newNodeVar(Obj *var) {
  Node *node = newNode(ND_VAR);
  node->var = var;
  return node;
}

Node *newNodeAddr(Node *body) {
  Node *node = newNode(ND_ADDR);
  node->body = body;
  return node;
}

Node *newNodeInc(Node *node, int i) {
  addType(node);
  return newNodeCast(
      newNodeAdd(toAssign(newNodeAdd(node, newNodeNum(i))), newNodeNum(-i)),
      node->ty);
}

Node *bitand(void) {
  Node *node = equality();
  while (consume("&")) {
    node = newNodeBinary(ND_BITAND, node, equality());
  }
  return node;
}

Node * bitor (void) {
  Node *node = bitxor();
  while (consume("|")) {
    node = newNodeBinary(ND_BITOR, node, bitxor());
  }
  return node;
}

Node *bitxor(void) {
  Node *node = bitand();
  while (consume("^")) {
    node = newNodeBinary(ND_BITXOR, node, bitand());
  }
  return node;
}

Node *logand(void) {
  Node *node = bitor ();
  while (consume("&&")) {
    node = newNodeBinary(ND_LOGAND, node, bitor ());
  }
  return node;
}

Node *logor(void) {
  Node *node = logand();
  while (consume("||")) {
    node = newNodeBinary(ND_LOGOR, node, logand());
  }
  return node;
}

Type *arrayDimensions(Type *ty) {
  if (consume("]")) {
    ty = typeSuffix(ty);
    return arrayOf(ty, -1);
  }
  const int64_t sz = expectNumber();
  expect("]");
  ty = typeSuffix(ty);
  return arrayOf(ty, sz);
}
