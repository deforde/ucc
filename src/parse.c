#include "parse.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp_err.h"
#include "defs.h"
#include "tokenise.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

extern Type *ty_int;
extern Token *token;
static Obj *cur_fn = NULL;
static Scope *scopes = &(Scope){0};
static Node *gotos = NULL;
static Node *labels = NULL;
static char *cur_brk_label = NULL;
static char *cur_cont_label = NULL;
static Node *cur_switch = NULL;
Obj *prog = NULL;
Obj *globals = NULL;
Type *ty_char = &(Type){.kind = TY_CHAR, .size = 1, .align = 1};
Type *ty_bool = &(Type){.kind = TY_BOOL, .size = 1, .align = 1};
Type *ty_int = &(Type){.kind = TY_INT, .size = 4, .align = 4};
Type *ty_long = &(Type){.kind = TY_LONG, .size = 8, .align = 8};
Type *ty_short = &(Type){.kind = TY_SHORT, .size = 2, .align = 2};
Type *ty_void = &(Type){.kind = TY_VOID, .size = 1, .align = 1};
Type *ty_uchar =
    &(Type){.kind = TY_CHAR, .size = 1, .align = 1, .is_unsigned = true};
Type *ty_uint =
    &(Type){.kind = TY_INT, .size = 4, .align = 4, .is_unsigned = true};
Type *ty_ulong =
    &(Type){.kind = TY_LONG, .size = 8, .align = 8, .is_unsigned = true};
Type *ty_ushort =
    &(Type){.kind = TY_SHORT, .size = 2, .align = 2, .is_unsigned = true};
Type *ty_float = &(Type){.kind = TY_FLOAT, .size = 4, .align = 4};
Type *ty_double = &(Type){.kind = TY_DOUBLE, .size = 8, .align = 8};

static Initialiser *initialiser(Type **ty);
static Initialiser *newInitialiser(Type *ty, bool is_flexible);
static Node * bitor (void);
static Node *add(void);
static Node *assign(void);
static Node *bitand(void);
static Node *bitshift(void);
static Node *bitxor(void);
static Node *cast(void);
static Node *cmpndStmt(void);
static Node *createLvalInit(Initialiser *init, Type *ty, InitDesg *desg);
static Node *declaration(Type *basety, VarAttr *attr);
static Node *equality(void);
static Node *expr(void);
static Node *funcCall(Token *tok);
static Node *initDesgExpr(InitDesg *desg);
static Node *logand(void);
static Node *logor(void);
static Node *lvalInitialiser(Obj *var);
static Node *mul(void);
static Node *newNode(NodeKind kind);
static Node *newNodeAdd(Node *lhs, Node *rhs);
static Node *newNodeAddr(Node *body);
static Node *newNodeBinary(NodeKind kind, Node *lhs, Node *rhs);
static Node *newNodeBreak();
static Node *newNodeCase();
static Node *newNodeCast(Node *expr, Type *ty);
static Node *newNodeCont();
static Node *newNodeDefault();
static Node *newNodeDeref(Node *body);
static Node *newNodeDo(void);
static Node *newNodeFlonum(double fval);
static Node *newNodeFor(void);
static Node *newNodeGoto(Token *label);
static Node *newNodeIdent(Token *tok);
static Node *newNodeIf(void);
static Node *newNodeInc(Node *node, int i);
static Node *newNodeLabel(Token *label);
static Node *newNodeLong(int64_t val);
static Node *newNodeMember(Node *body);
static Node *newNodeNum(int64_t val);
static Node *newNodeReturn(void);
static Node *newNodeSub(Node *lhs, Node *rhs);
static Node *newNodeSwitch();
static Node *newNodeUlong(int64_t val);
static Node *newNodeVar(Obj *var);
static Node *newNodeWhile(void);
static Node *postfix(void);
static Node *primary(void);
static Node *relational(void);
static Node *stmt(void);
static Node *structRef(Node *node);
static Node *ternary(void);
static Node *toAssign(Node *node);
static Node *unary(void);
static Obj *function(Type *ty, VarAttr *attr);
static Obj *newAnonGlobalVar(Type *ty);
static Obj *newGlobalVar(Type *ty, Token *ident);
static Obj *newLocalVar(Type *ty, Token *ident);
static Obj *newStrLitVar(Token *tok, Type *ty);
static Obj *newVar(Type *ty, Token *ident, Obj **vars);
static Relocation *writeGlobalVarData(Relocation *cur, Initialiser *init,
                                      Type *ty, char *buf, size_t offset);
static Token *createIdent(const char *name);
static Type *abstractDeclarator(Type *ty);
static Type *arrayDimensions(Type *ty);
static Type *arrayOf(Type *base, ssize_t len);
static Type *copyStructType(Type *src);
static Type *declarator(Type *ty, Token **ident);
static Type *declspec(VarAttr *attr);
static Type *enumSpec();
static Type *findTag(Token *tok);
static Type *findTypedef(Token *tok);
static Type *getCommonType(Type *ty1, Type *ty2);
static Type *newType(TypeKind kind, ssize_t size, size_t align);
static Type *pointers(Type *ty);
static Type *pointerTo(Type *base);
static Type *structDecl(Type *ty);
static Type *structUnionDecl(Type *ty);
static Type *typeSuffix(Type *ty);
static Type *typename(void);
static Type *unionDecl(Type *ty);
static VarScope *findVarScope(Token *tok);
static bool atInitialiserListEnd(void);
static bool consumeInitialiserListEnd(void);
static bool equal(Token *tok, const char *str);
static bool isFunc(void);
static bool isTypename(Token *tok);
static char *newUniqueLabel(void);
static int64_t constExpr(void);
static int64_t eval(Node *node);
static int64_t eval2(Node *node, char **label);
static int64_t evalRval(Node *node, char **label);
static size_t alignTo(size_t n, size_t align);
static size_t countInitialserElems(Type *ty);
static void addType(Node *node);
static void arrayInitialiser1(Initialiser *init);
static void arrayInitialiser2(Initialiser *init);
static void enterScope(void);
static void exitScope(void);
static void globalVar(Type *ty, VarAttr *attr);
static void globalVarInitialiser(Obj *var);
static void initialiser2(Initialiser *init);
static void newParam(Type *ty, Token *ident);
static void parseTypedef(Type *basety);
static void pushScope(char *name, Obj *var, Type *type_def);
static void pushTagScope(Token *tok, Type *ty);
static void resolveGotoLabels(void);
static void skipExcessInitialiserElems(void);
static void stringInitialiser(Initialiser *init, Token *tok);
static void structInitialiser1(Initialiser *init);
static void structInitialiser2(Initialiser *init);
static void unionInitialiser(Initialiser *init);
static void usualArithConv(Node **lhs, Node **rhs);
static void writeBuf(char *buf, uint64_t val, size_t sz);

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
    globalVar(ty, &attr);
  }
  prog = head.next;
}

Node *cmpndStmt(void) {
  Node head = {0};
  Node *cur = &head;
  enterScope();
  while (!consume("}")) {
    if (isTypename(token) && !equal(token->next, ":")) {
      VarAttr attr = {0};
      Type *basety = declspec(&attr);
      if (attr.is_typedef) {
        parseTypedef(basety);
        continue;
      }
      if (isFunc()) {
        function(basety, &attr);
        continue;
      }
      if (attr.is_extern) {
        globalVar(basety, &attr);
        continue;
      }
      cur = cur->next = declaration(basety, &attr);
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
  Node *node = ternary();
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
  if (consume("<<=")) {
    return toAssign(newNodeBinary(ND_SHL, node, assign()));
  }
  if (consume(">>=")) {
    return toAssign(newNodeBinary(ND_SHR, node, assign()));
  }
  return node;
}

Node *stmt(void) {
  Node *node = NULL;
  Token *label = NULL;
  Token *cur_tok = token;
  if (consume(";")) {
    node = newNode(ND_BLK);
  } else if (consumeDo()) {
    node = newNodeDo();
  } else if (consumeGoto()) {
    node = newNodeGoto(expectIdent());
  } else if ((label = consumeLabel())) {
    node = newNodeLabel(label);
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
  } else if (consumeBreak()) {
    if (!cur_brk_label) {
      compErrorToken(cur_tok->str, "stray break");
    }
    node = newNodeBreak();
  } else if (consumeCont()) {
    if (!cur_cont_label) {
      compErrorToken(cur_tok->str, "stray continue");
    }
    node = newNodeCont();
  } else if (consumeSwitch()) {
    node = newNodeSwitch();
  } else if (consumeCase()) {
    if (!cur_switch) {
      compErrorToken(cur_tok->str, "stray case");
    }
    node = newNodeCase();
  } else if (consumeDefault()) {
    if (!cur_switch) {
      compErrorToken(cur_tok->str, "stray default");
    }
    node = newNodeDefault();
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

Node *newNodeGoto(Token *label) {
  Node *node = newNode(ND_GOTO);
  node->label = strndup(label->str, label->len);
  node->goto_next = gotos;
  gotos = node;
  return node;
}

Node *newNodeLabel(Token *label) {
  Node *node = newNode(ND_LABEL);
  node->label = strndup(label->str, label->len);
  node->unique_label = newUniqueLabel();
  node->lhs = stmt();
  node->goto_next = labels;
  labels = node;
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

Node *newNodeFlonum(double fval) {
  Node *node = newNode(ND_NUM);
  node->fval = fval;
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
  if (isNumeric(lhs->ty) && isNumeric(rhs->ty)) {
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
  if (isNumeric(lhs->ty) && isNumeric(rhs->ty)) {
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
    node->ty = ty_long;
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
  if (ident) {
    var->name = strndup(ident->str, ident->len);
  }
  var->ty = ty;
  var->align = ty->align;
  *vars = var;
  size_t offset = 0;
  for (Obj *ext_var = *vars; ext_var; ext_var = ext_var->next) {
    offset += ext_var->ty->size;
    offset = alignTo(offset, ext_var->align);
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
  var->align = ty->align;
  var->is_global = true;
  var->is_definition = true;
  globals = var;
  pushScope(var->name, var, NULL);
  return var;
}

char *newUniqueLabel(void) {
  static size_t id = 0;
  char *label = calloc(1, 20); // TODO: 20?
  sprintf(label, ".lbl..%zu", id++);
  return label;
}

Obj *newStrLitVar(Token *tok, Type *ty) {
  Obj *var = newAnonGlobalVar(ty);
  var->init_data = tok->str;
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
    FLOAT = 1 << 12,
    DOUBLE = 1 << 14,
    OTHER = 1 << 16,
    SIGNED = 1 << 17,
    UNSIGNED = 1 << 18,
  };

  size_t counter = 0;
  Type *ty = ty_int;
  ty->tok = token;

  while (isTypename(token)) {
    Token *tok = token;
    token = token->next;

    bool is_typedef = false;
    bool is_static = false;
    bool is_extern = false;
    if ((is_typedef = equal(tok, "typedef")) ||
        (is_static = equal(tok, "static")) ||
        (is_extern = equal(tok, "extern"))) {
      if (!attr) {
        compErrorToken(
            tok->str, "storage class specifier is not allowed in this context");
      }
      attr->is_typedef |= is_typedef;
      attr->is_static |= is_static;
      attr->is_extern |= is_extern;
      if ((attr->is_static && attr->is_typedef) ||
          (attr->is_extern && attr->is_typedef) ||
          (attr->is_extern && attr->is_static)) {
        compErrorToken(tok->str,
                       "invalid combination of storage class specifiers");
      }
      continue;
    }

    if (equal(tok, "const") || equal(tok, "volatile") || equal(tok, "auto") ||
        equal(tok, "register") || equal(tok, "restrict") ||
        equal(tok, "__restrict") || equal(tok, "__restrict__") ||
        equal(tok, "_Noreturn")) {
      continue;
    }

    if (equal(tok, "_Alignas")) {
      if (!attr) {
        compErrorToken(tok->str, "_Alignas is not allowed in this context");
      }
      expect("(");
      if (isTypename(token)) {
        attr->align = typename()->align;
      } else {
        attr->align = constExpr();
      }
      expect(")");
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
    } else if (equal(tok, "float")) {
      counter += FLOAT;
    } else if (equal(tok, "double")) {
      counter += DOUBLE;
    } else if (equal(tok, "signed")) {
      counter |= SIGNED;
    } else if (equal(tok, "unsigned")) {
      counter |= UNSIGNED;
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
    case SIGNED + CHAR:
      ty = ty_char;
      break;
    case UNSIGNED + CHAR:
      ty = ty_uchar;
      break;
    case SHORT:
    case SHORT + INT:
    case SIGNED + SHORT:
    case SIGNED + SHORT + INT:
      ty = ty_short;
      break;
    case UNSIGNED + SHORT:
    case UNSIGNED + SHORT + INT:
      ty = ty_ushort;
      break;
    case INT:
    case SIGNED:
    case SIGNED + INT:
      ty = ty_int;
      break;
    case UNSIGNED:
    case UNSIGNED + INT:
      ty = ty_uint;
      break;
    case LONG:
    case LONG + INT:
    case LONG + LONG:
    case LONG + LONG + INT:
    case SIGNED + LONG:
    case SIGNED + LONG + INT:
    case SIGNED + LONG + LONG:
    case SIGNED + LONG + LONG + INT:
      ty = ty_long;
      break;
    case UNSIGNED + LONG:
    case UNSIGNED + LONG + INT:
    case UNSIGNED + LONG + LONG:
    case UNSIGNED + LONG + LONG + INT:
      ty = ty_ulong;
      break;
    case FLOAT:
      ty = ty_float;
      break;
    case DOUBLE:
      ty = ty_double;
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
      Type *ty2 = findTag(tag);
      if (ty2) {
        return ty2;
      }
      ty->size = -1;
      pushTagScope(tag, ty);
      return ty;
    }
  } else {
    expect("{");
  }

  Obj head = {0};
  Obj *cur = &head;
  while (!consume("}")) {
    VarAttr attr = {0};
    Type *mem_ty = declspec(&attr);
    bool first = true;
    while (!consume(";")) {
      if (!first) {
        expect(",");
      }
      first = false;
      Obj *mem = calloc(1, sizeof(Obj));
      Token *ident = NULL;
      mem->ty = declarator(mem_ty, &ident);
      mem->align = attr.align ? attr.align : mem->ty->align;
      mem->name = strndup(ident->str, ident->len);
      cur = cur->next = mem;
    }
  }
  if (cur != &head && cur->ty->kind == TY_ARR && cur->ty->arr_len < 0) {
    cur->ty = arrayOf(cur->ty->base, 0);
    ty->is_flexible = true;
  }
  ty->members = head.next;
  ty->align = 1;

  if (tag) {
    for (TagScope *ts = scopes->tags; ts; ts = ts->next) {
      if (strlen(ts->name) == tag->len &&
          memcmp(tag->str, ts->name, tag->len) == 0) {
        *ts->ty = *ty;
        return ts->ty;
      }
    }
    pushTagScope(tag, ty);
  }

  return ty;
}

Type *structDecl(Type *ty) {
  ty = structUnionDecl(ty);
  if (ty->size < 0) {
    return ty;
  }

  size_t offset = 0;
  for (Obj *mem = ty->members; mem; mem = mem->next) {
    offset = alignTo(offset, mem->align);
    mem->offset = offset;
    offset += mem->ty->size;
    if (mem->align > ty->align) {
      ty->align = mem->align;
    }
  }
  ty->size = (ssize_t)alignTo(offset, ty->align);

  return ty;
}

Type *unionDecl(Type *ty) {
  ty = structUnionDecl(ty);
  if (ty->size < 0) {
    return ty;
  }

  for (Obj *mem = ty->members; mem; mem = mem->next) {
    if (mem->align > ty->align) {
      ty->align = mem->align;
    }
    if (mem->ty->size > ty->size) {
      ty->size = mem->ty->size;
    }
  }
  ty->size = (ssize_t)alignTo(ty->size, ty->align);

  return ty;
}

Type *abstractDeclarator(Type *ty) {
  ty = pointers(ty);
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
  ty = pointers(ty);
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
  *ident = consumeIdent();
  ty = typeSuffix(ty);
  return ty;
}

Obj *function(Type *ty, VarAttr *attr) {
  Token *fn_ident = NULL;
  ty = declarator(ty, &fn_ident);
  if (!fn_ident) {
    compErrorToken(ty->tok->str, "function name omitted");
  }

  Obj *fn = calloc(1, sizeof(Obj));
  fn->ty = newType(TY_FUNC, 0, 0);
  fn->align = fn->ty->align;
  fn->ty->ret_ty = ty;
  fn->name = strndup(fn_ident->str, fn_ident->len);
  fn->is_global = !attr->is_static;
  cur_fn = fn;

  Obj *var = calloc(1, sizeof(Obj));
  var->name = strndup(fn_ident->str, fn_ident->len);
  var->ty = fn->ty;
  var->align = var->ty->align;
  var->is_global = fn->is_global;
  pushScope(var->name, var, NULL);

  enterScope();

  bool is_variadic = false;
  expect("(");
  if (equal(token, "void") && equal(token->next, ")")) {
    token = token->next->next;
  } else {
    bool first = true;
    while (!consume(")")) {
      if (!first) {
        expect(",");
      }
      first = false;
      if (consume("...")) {
        is_variadic = true;
        expect(")");
        break;
      }
      Type *ty2 = declspec(NULL);
      Token *param_ident = NULL;
      ty2 = declarator(ty2, &param_ident);
      if (ty2->kind == TY_ARR) {
        assert(ty2->base);
        ty2 = pointerTo(ty2->base);
      }
      newParam(ty2, param_ident);
    }
    if (first) {
      is_variadic = true;
    }
  }

  Type *param_ty = NULL;
  for (Obj *param = fn->params; param; param = param->next) {
    Type *new_param_ty = calloc(1, sizeof(Type));
    *new_param_ty = *param->ty;
    new_param_ty->next = param_ty;
    param_ty = new_param_ty;
  }
  fn->ty->params = param_ty;
  fn->ty->is_variadic = is_variadic;
  if (!consume(";")) {
    for (Obj *param = fn->params; param; param = param->next) {
      if (!param->name) {
        compErrorToken(param->ty->tok->str, "argument name omitted");
      }
    }
    expect("{");
    fn->locals = fn->params;
    if (fn->ty->is_variadic) {
      Token *ident = createIdent("__va_area__");
      fn->va_area = newLocalVar(arrayOf(ty_char, 136), ident);
    }
    fn->body = cmpndStmt();
  }

  exitScope();
  resolveGotoLabels();
  return fn;
}

Node *declaration(Type *basety, VarAttr *attr) {
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
    if (ty->kind == TY_VOID) {
      compError(ident->str, "variable declared void");
    }
    if (!ident) {
      compErrorToken(ty->tok->str, "variable name omitted");
    }

    if (attr && attr->is_static) {
      Obj *gvar = newAnonGlobalVar(ty);
      pushScope(strndup(ident->str, ident->len), gvar, NULL);

      Token *ident = createIdent(gvar->name);
      newLocalVar(ty, ident);

      if (consume("=")) {
        globalVarInitialiser(gvar);
      }

      continue;
    }

    Obj *var = newLocalVar(ty, ident);
    if (attr && attr->align) {
      var->align = attr->align;
    }

    if (consume("=")) {
      cur = cur->next = lvalInitialiser(var);
    }

    if (var->ty->size < 0) {
      compErrorToken(ident->str, "variable has incomplete type");
    }
    if (var->ty->kind == TY_VOID) {
      compError(ident->str, "variable declared void");
    }
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
      return newNodeUlong((int64_t)ty->size);
    }
    Node *node = unary();
    addType(node);
    return newNodeUlong((int64_t)node->ty->size);
  }
  if (consumeAlignof()) {
    if (equal(token, "(") && isTypename(token->next)) {
      token = token->next;
      Type *ty = typename();
      expect(")");
      return newNodeUlong((int64_t)ty->align);
    }
    Node *node = unary();
    addType(node);
    return newNodeUlong((int64_t)node->ty->align);
  }
  tok = consumeStrLit();
  if (tok) {
    Type *ty = arrayOf(ty_char, (ssize_t)tok->len);
    Obj *var = newStrLitVar(tok, ty);
    return newNodeVar(var);
  }
  tok = expectNumber();
  Node *node = NULL;
  if (isFloat(tok->ty)) {
    node = newNodeFlonum(tok->fval);
  } else {
    node = newNodeNum(tok->val);
  }
  node->ty = tok->ty;
  return node;
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
  Node *node = bitshift();
  for (;;) {
    if (consume("<")) {
      node = newNodeBinary(ND_LT, node, bitshift());
    } else if (consume("<=")) {
      node = newNodeBinary(ND_LE, node, bitshift());
    } else if (consume(">")) {
      node = newNodeBinary(ND_LT, bitshift(), node);
    } else if (consume(">=")) {
      node = newNodeBinary(ND_LE, bitshift(), node);
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
      if (vs->name && strlen(vs->name) == tok->len &&
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
  return ty->kind == TY_BOOL || ty->kind == TY_CHAR || ty->kind == TY_ENUM ||
         ty->kind == TY_INT || ty->kind == TY_LONG || ty->kind == TY_SHORT;
}

bool isFloat(Type *ty) { return ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE; }

bool isNumeric(Type *ty) { return isInteger(ty) || isFloat(ty); }

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
    node->ty = ty_int;
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
  case ND_SHL:
  case ND_SHR:
    node->ty = node->lhs->ty;
    break;
  case ND_VAR:
    node->ty = node->var->ty;
    break;
  case ND_TERN:
    if (node->then->ty->kind == TY_VOID || node->els->ty->kind == TY_VOID) {
      node->ty = ty_void;
    } else {
      usualArithConv(&node->then, &node->els);
      node->ty = node->then->ty;
    }
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

Type *pointers(Type *ty) {
  while (consume("*")) {
    ty = pointerTo(ty);
    // clang-format off
    while (consumeKwdMatch("const") || consumeKwdMatch("volatile") ||
           consumeKwdMatch("restrict") || consumeKwdMatch("__restrict") ||
           consumeKwdMatch("__restrict__"));
    // clang-format on
  }
  return ty;
}

Type *pointerTo(Type *base) {
  Type *ty = newType(TY_PTR, 8, 8);
  ty->base = base;
  ty->is_unsigned = true;
  return ty;
}

Type *arrayOf(Type *base, ssize_t len) {
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
  enterScope();
  char *brk = cur_brk_label;
  char *cont = cur_cont_label;
  cur_brk_label = node->brk_label = newUniqueLabel();
  cur_cont_label = node->cont_label = newUniqueLabel();
  node->body = stmt();
  exitScope();
  cur_brk_label = brk;
  cur_cont_label = cont;
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
  char *brk = cur_brk_label;
  cur_brk_label = node->brk_label = newUniqueLabel();
  char *cont = cur_cont_label;
  cur_cont_label = node->cont_label = newUniqueLabel();
  if (!consume(";")) {
    if (isTypename(token)) {
      Type *basety = declspec(NULL);
      node->pre = declaration(basety, NULL);
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
  cur_brk_label = brk;
  cur_cont_label = cont;
  return node;
}

Node *newNodeReturn(void) {
  Node *node = newNode(ND_RET);
  if (consume(";")) {
    return node;
  }
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

    if (!param_ty && !ty->is_variadic) {
      compErrorToken(tok->str, "too many arguments");
    }

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

  if (param_ty) {
    compErrorToken(tok->str, "too few arguments");
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
  if (equal(token, "(") && isTypename(token->next)) {
    token = token->next;
    Type *ty = typename();
    expect(")");
    if (scopes->next == NULL) {
      Obj *var = newAnonGlobalVar(ty);
      globalVarInitialiser(var);
      return newNodeVar(var);
    }

    Token *ident = createIdent("");
    Obj *var = newLocalVar(ty, ident);
    Node *lhs = lvalInitialiser(var);
    Node *rhs = newNodeVar(var);
    return newNodeBinary(ND_COMMA, lhs, rhs);
  }

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

void globalVar(Type *base_ty, VarAttr *attr) {
  bool first = true;
  while (!consume(";")) {
    if (!first) {
      expect(",");
    }
    first = false;
    Token *ident = NULL;
    Type *ty = declarator(base_ty, &ident);
    if (!ident) {
      compErrorToken(ty->tok->str, "variable name omitted");
    }
    Obj *var = newGlobalVar(ty, ident);
    var->is_definition = !attr->is_extern;
    var->is_static = attr->is_static;
    if (attr->align) {
      var->align = attr->align;
    }
    if (consume("=")) {
      globalVarInitialiser(var);
    }
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
    if (!ident) {
      compErrorToken(ty->tok->str, "typedef name omitted");
    }

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
  static const char *kwds[] = {
      "_Bool",      "char",         "enum",      "int",      "long",
      "short",      "static",       "struct",    "typedef",  "union",
      "void",       "extern",       "_Alignas",  "signed",   "unsigned",
      "const",      "volatile",     "auto",      "register", "restrict",
      "__restrict", "__restrict__", "_Noreturn", "float",    "double"};
  for (size_t i = 0; i < sizeof(kwds) / sizeof(*kwds); ++i) {
    if (strlen(kwds[i]) == tok->len &&
        strncmp(tok->str, kwds[i], tok->len) == 0) {
      return true;
    }
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
    if (equal(token, "{")) {
      token = start;
      return unary();
    }
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
  if (ty1->kind == TY_DOUBLE || ty2->kind == TY_DOUBLE) {
    return ty_double;
  }
  if (ty1->kind == TY_FLOAT || ty2->kind == TY_FLOAT) {
    return ty_float;
  }
  if (ty1->size < 4) {
    ty1 = ty_int;
  }
  if (ty2->size < 4) {
    ty2 = ty_int;
  }
  if (ty1->size != ty2->size) {
    return (ty1->size < ty2->size) ? ty2 : ty1;
  }
  if (ty2->is_unsigned) {
    return ty2;
  }
  return ty1;
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
      val = (int)constExpr();
    }
    pushScope(strndup(econst->str, econst->len), NULL, NULL);
    VarScope *sc = scopes->vars;
    sc->enum_ty = ty;
    sc->enum_val = val++;
    if (consumeInitialiserListEnd()) {
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

  Token *ident = createIdent("");
  Obj *var = newLocalVar(pointerTo(node->lhs->ty), ident);

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
  // clang-format off
  while (consumeKwdMatch("static") || consumeKwdMatch("restrict"));
  // clang-format on
  if (consume("]")) {
    ty = typeSuffix(ty);
    return arrayOf(ty, -1);
  }
  const ssize_t sz = (int)constExpr();
  expect("]");
  ty = typeSuffix(ty);
  return arrayOf(ty, sz);
}

void resolveGotoLabels(void) {
  for (Node *x = gotos; x; x = x->goto_next) {
    for (Node *y = labels; y; y = y->goto_next) {
      if (strcmp(x->label, y->label) == 0) {
        x->unique_label = y->unique_label;
      }
    }
    if (!x->unique_label) {
      compErrorToken(x->tok->str, "use of undeclared label");
    }
  }
  gotos = labels = NULL;
}

Node *newNodeBreak() {
  assert(cur_brk_label);
  Node *node = newNode(ND_GOTO);
  node->unique_label = cur_brk_label;
  expect(";");
  return node;
}

Node *newNodeCont() {
  Node *node = newNode(ND_GOTO);
  assert(cur_cont_label);
  node->unique_label = cur_cont_label;
  expect(";");
  return node;
}

Node *newNodeSwitch() {
  Node *node = newNode(ND_SWITCH);
  expect("(");
  node->cond = expr();
  expect(")");
  Node *sw = cur_switch;
  cur_switch = node;
  char *brk = cur_brk_label;
  cur_brk_label = node->brk_label = newUniqueLabel();
  node->then = stmt();
  cur_switch = sw;
  cur_brk_label = brk;
  return node;
}

Node *newNodeCase() {
  assert(cur_switch);
  const int64_t val = constExpr();
  Node *node = newNode(ND_CASE);
  expect(":");
  node->label = newUniqueLabel();
  node->lhs = stmt();
  node->val = val;
  node->case_next = cur_switch->case_next;
  cur_switch->case_next = node;
  return node;
}

Node *newNodeDefault() {
  assert(cur_switch);
  Node *node = newNode(ND_CASE);
  expect(":");
  node->label = newUniqueLabel();
  node->lhs = stmt();
  cur_switch->default_case = node;
  return node;
}

Node *bitshift(void) {
  Node *node = add();
  for (;;) {
    if (consume("<<")) {
      node = newNodeBinary(ND_SHL, node, add());
      continue;
    }
    if (consume(">>")) {
      node = newNodeBinary(ND_SHR, node, add());
      continue;
    }
    break;
  }
  return node;
}

Node *ternary(void) {
  Node *cond = logor();
  if (!consume("?")) {
    return cond;
  }
  Node *node = newNode(ND_TERN);
  node->cond = cond;
  node->then = expr();
  expect(":");
  node->els = ternary();
  return node;
}

int64_t constExpr(void) {
  Node *node = ternary();
  return eval(node);
}

int64_t eval(Node *node) { return eval2(node, NULL); }

int64_t eval2(Node *node, char **label) {
  addType(node);
  switch (node->kind) {
  case ND_ADD:
    return eval2(node->lhs, label) + eval(node->rhs);
  case ND_BITAND:
    return eval(node->lhs) & eval(node->rhs);
  case ND_BITNOT:
    return ~eval(node->lhs);
  case ND_BITOR:
    return eval(node->lhs) | eval(node->rhs);
  case ND_BITXOR:
    return eval(node->lhs) ^ eval(node->rhs);
  case ND_CAST: {
    const int64_t val = eval2(node->lhs, label);
    if (isInteger(node->ty)) {
      switch (node->ty->size) {
      case 1:
        return node->ty->is_unsigned ? (uint8_t)val : (int8_t)val;
      case 2:
        return node->ty->is_unsigned ? (uint16_t)val : (int16_t)val;
      case 4:
        return node->ty->is_unsigned ? (uint32_t)val : (int32_t)val;
      }
    }
    return val;
  }
  case ND_COMMA:
    return eval2(node->rhs, label);
  case ND_TERN:
    return eval(node->cond) ? eval2(node->then, label)
                            : eval2(node->els, label);
  case ND_DIV:
    if (node->ty->is_unsigned) {
      return (uint64_t)eval(node->lhs) / eval(node->rhs);
    }
    return eval(node->lhs) / eval(node->rhs);
  case ND_EQ:
    return eval(node->lhs) == eval(node->rhs);
  case ND_LE:
    if (node->lhs->ty->is_unsigned) {
      return (uint64_t)eval(node->lhs) <= (uint64_t)eval(node->rhs);
    }
    return eval(node->lhs) <= eval(node->rhs);
  case ND_LOGAND:
    return eval(node->lhs) && eval(node->rhs);
  case ND_LOGOR:
    return eval(node->lhs) || eval(node->rhs);
  case ND_LT:
    if (node->lhs->ty->is_unsigned) {
      return (uint64_t)eval(node->lhs) < (uint64_t)eval(node->rhs);
    }
    return eval(node->lhs) < eval(node->rhs);
  case ND_MOD:
    if (node->ty->is_unsigned) {
      return (uint64_t)eval(node->lhs) % eval(node->rhs);
    }
    return eval(node->lhs) % eval(node->rhs);
  case ND_MUL:
    return eval(node->lhs) * eval(node->rhs);
  case ND_NE:
    return eval(node->lhs) != eval(node->rhs);
  case ND_NOT:
    return !eval(node->lhs);
  case ND_NUM:
    return node->val;
  case ND_SHL:
    return eval(node->lhs) << eval(node->rhs);
  case ND_SHR:
    if (node->ty->is_unsigned && node->ty->size == 8) {
      return (uint64_t)eval(node->lhs) >> eval(node->rhs);
    }
    return eval(node->lhs) >> eval(node->rhs);
  case ND_SUB:
    return eval2(node->lhs, label) - eval(node->rhs);
  case ND_ADDR:
    return evalRval(node->body, label);
  case ND_MEMBER:
    if (!label) {
      compErrorToken(node->tok->str, "not a compile-time constant");
    }
    if (node->ty->kind != TY_ARR) {
      compErrorToken(node->tok->str, "invalid initialiser");
    }
    return evalRval(node->lhs, label) + (int64_t)node->var->offset;
  case ND_VAR:
    if (!label) {
      compErrorToken(node->tok->str, "not a compile-time constant");
    }
    if (node->var->ty->kind != TY_ARR && node->var->ty->kind != TY_FUNC) {
      compErrorToken(node->tok->str, "invalid initialiser");
    }
    *label = node->var->name;
    return 0;
  default:
    break;
  }
  compErrorToken(node->tok->str, "not a compile time constant");
  return 0;
}

Initialiser *newInitialiser(Type *ty, bool is_flexible) {
  Initialiser *init = calloc(1, sizeof(Initialiser));
  init->ty = ty;
  if (ty->kind == TY_ARR) {
    if (is_flexible && ty->size < 0) {
      init->is_flexible = true;
      return init;
    }
    init->children = calloc(ty->arr_len, sizeof(*init->children));
    for (ssize_t i = 0; i < ty->arr_len; i++) {
      init->children[i] = newInitialiser(ty->base, false);
    }
    return init;
  }
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    size_t len = 0;
    for (Obj *mem = ty->members; mem; mem = mem->next) {
      len++;
    }
    init->children = calloc(len, sizeof(*init->children));
    size_t idx = 0;
    for (Obj *mem = ty->members; mem; mem = mem->next) {
      if (is_flexible && ty->is_flexible && !mem->next) {
        Initialiser *child = calloc(1, sizeof(Initialiser));
        child->ty = mem->ty;
        child->is_flexible = true;
        init->children[idx++] = child;
      } else {
        init->children[idx++] = newInitialiser(mem->ty, false);
      }
    }
    return init;
  }
  return init;
}

Initialiser *initialiser(Type **base_ty) {
  Type *ty = *base_ty;
  Initialiser *init = newInitialiser(ty, true);
  initialiser2(init);

  if ((ty->kind == TY_STRUCT || ty->kind == TY_UNION) && ty->is_flexible) {
    ty = copyStructType(ty);

    Obj *mem = ty->members;
    size_t idx = 0;
    while (mem->next) {
      mem = mem->next;
      idx++;
    }
    mem->ty = init->children[idx]->ty;
    ty->size += mem->ty->size;
    *base_ty = ty;
    return init;
  }

  *base_ty = init->ty;
  return init;
}

void initialiser2(Initialiser *init) {
  if (init->ty->kind == TY_ARR) {
    Token *tok = consumeStrLit();
    if (tok) {
      stringInitialiser(init, tok);
      return;
    }
    if (equal(token, "{")) {
      arrayInitialiser1(init);
    } else {
      arrayInitialiser2(init);
    }
    return;
  }
  if (init->ty->kind == TY_STRUCT) {
    if (equal(token, "{")) {
      structInitialiser1(init);
      return;
    }
    Token *start = token;
    Node *expr = assign();
    addType(expr);
    if (expr->ty->kind == TY_STRUCT) {
      init->expr = expr;
      return;
    }
    token = start;
    structInitialiser2(init);
    return;
  }
  if (init->ty->kind == TY_UNION) {
    unionInitialiser(init);
    return;
  }
  if (consume("{")) {
    initialiser2(init);
    expect("}");
    return;
  }
  init->expr = assign();
}

Node *lvalInitialiser(Obj *var) {
  Initialiser *init = initialiser(&var->ty);
  InitDesg desg = {.next = NULL, .idx = 0, .var = var, .is_member = false};
  Node *lhs = newNode(ND_MEMZERO);
  lhs->var = var;
  Node *rhs = createLvalInit(init, var->ty, &desg);
  return newNodeBinary(ND_COMMA, lhs, rhs);
}

Node *createLvalInit(Initialiser *init, Type *ty, InitDesg *desg) {
  if (ty->kind == TY_ARR) {
    Node *node = newNode(ND_NULL_EXPR);
    for (ssize_t i = 0; i < ty->arr_len; i++) {
      InitDesg desg2 = {
          .next = desg, .idx = i, .var = NULL, .is_member = false};
      Node *rhs = createLvalInit(init->children[i], ty->base, &desg2);
      node = newNodeBinary(ND_COMMA, node, rhs);
    }
    return node;
  }
  if (ty->kind == TY_STRUCT && !init->expr) {
    Node *node = newNode(ND_NULL_EXPR);
    size_t idx = 0;
    for (Obj *mem = ty->members; mem; mem = mem->next) {
      InitDesg desg2 = {.next = desg, .idx = 0, .var = mem, .is_member = true};
      Node *rhs = createLvalInit(init->children[idx++], mem->ty, &desg2);
      node = newNodeBinary(ND_COMMA, node, rhs);
    }
    return node;
  }
  if (ty->kind == TY_UNION) {
    InitDesg desg2 = {
        .next = desg, .idx = 0, .var = ty->members, .is_member = true};
    return createLvalInit(init->children[0], ty->members->ty, &desg2);
  }
  if (!init->expr) {
    return newNode(ND_NULL_EXPR);
  }
  Node *lhs = initDesgExpr(desg);
  Node *rhs = init->expr;
  return newNodeBinary(ND_ASS, lhs, rhs);
}

Node *initDesgExpr(InitDesg *desg) {
  if (desg->is_member) {
    Node *node = newNodeMember(initDesgExpr(desg->next));
    node->var = desg->var;
    return node;
  }
  if (desg->var) {
    return newNodeVar(desg->var);
  }
  Node *lhs = initDesgExpr(desg->next);
  Node *rhs = newNodeNum((int64_t)desg->idx);
  return newNodeDeref(newNodeAdd(lhs, rhs));
}

void skipExcessInitialiserElems(void) {
  if (consume("{")) {
    skipExcessInitialiserElems();
    expect("}");
  }
  assign();
}

void stringInitialiser(Initialiser *init, Token *tok) {
  if (init->is_flexible) {
    *init = *newInitialiser(arrayOf(init->ty->base, (ssize_t)tok->len), false);
  }
  size_t len = MIN(init->ty->arr_len, (ssize_t)tok->len);
  for (size_t i = 0; i < len; i++) {
    init->children[i]->expr = newNodeNum(tok->str[i]);
  }
}

void arrayInitialiser1(Initialiser *init) {
  expect("{");
  if (init->is_flexible) {
    size_t len = countInitialserElems(init->ty);
    *init = *newInitialiser(arrayOf(init->ty->base, (ssize_t)len), false);
  }
  for (ssize_t i = 0; !consumeInitialiserListEnd(); i++) {
    if (i > 0) {
      expect(",");
    }
    if (i < init->ty->arr_len) {
      initialiser2(init->children[i]);
    } else {
      skipExcessInitialiserElems();
    }
  }
}

void arrayInitialiser2(Initialiser *init) {
  if (init->is_flexible) {
    size_t len = countInitialserElems(init->ty);
    *init = *newInitialiser(arrayOf(init->ty->base, len), false);
  }
  for (ssize_t i = 0; i < init->ty->arr_len && !atInitialiserListEnd(); i++) {
    if (i > 0) {
      expect(",");
    }
    initialiser2(init->children[i]);
  }
}

size_t countInitialserElems(Type *ty) {
  Initialiser *dummy = newInitialiser(ty->base, false);
  Token *start = token;
  size_t i = 0;
  for (; !consumeInitialiserListEnd(); i++) {
    if (i > 0) {
      expect(",");
    }
    initialiser2(dummy);
  }
  token = start;
  return i;
}

void structInitialiser1(Initialiser *init) {
  expect("{");
  Obj *mem = init->ty->members;
  size_t idx = 0;
  while (!consumeInitialiserListEnd()) {
    if (mem != init->ty->members) {
      expect(",");
    }
    if (mem) {
      initialiser2(init->children[idx++]);
      mem = mem->next;
    } else {
      skipExcessInitialiserElems();
    }
  }
}

void structInitialiser2(Initialiser *init) {
  bool first = true;
  size_t idx = 0;
  for (Obj *mem = init->ty->members; mem && !atInitialiserListEnd();
       mem = mem->next) {
    if (!first) {
      expect(",");
    }
    first = false;
    initialiser2(init->children[idx++]);
  }
}

void unionInitialiser(Initialiser *init) {
  if (consume("{")) {
    initialiser2(init->children[0]);
    consume(",");
    expect("}");
  } else {
    initialiser2(init->children[0]);
  }
}

void globalVarInitialiser(Obj *var) {
  Initialiser *init = initialiser(&var->ty);
  Relocation head = {0};
  char *buf = calloc(1, var->ty->size);
  writeGlobalVarData(&head, init, var->ty, buf, 0);
  var->init_data = buf;
  var->rel = head.next;
}

Relocation *writeGlobalVarData(Relocation *cur, Initialiser *init, Type *ty,
                               char *buf, size_t offset) {
  if (ty->kind == TY_ARR) {
    size_t sz = ty->base->size;
    for (ssize_t i = 0; i < ty->arr_len; i++) {
      cur = writeGlobalVarData(cur, init->children[i], ty->base, buf,
                               offset + sz * i);
    }
    return cur;
  }
  if (ty->kind == TY_STRUCT) {
    size_t idx = 0;
    for (Obj *mem = ty->members; mem; mem = mem->next) {
      cur = writeGlobalVarData(cur, init->children[idx++], mem->ty, buf,
                               offset + mem->offset);
    }
    return cur;
  }
  if (ty->kind == TY_UNION) {
    return writeGlobalVarData(cur, init->children[0], ty->members->ty, buf,
                              offset);
  }
  if (!init->expr) {
    return cur;
  }
  char *label = NULL;
  uint64_t val = eval2(init->expr, &label);
  if (!label) {
    writeBuf(buf + offset, val, ty->size);
    return cur;
  }
  Relocation *rel = calloc(1, sizeof(Relocation));
  rel->offset = offset;
  rel->label = label;
  rel->addend = val;
  cur->next = rel;
  return cur->next;
}

void writeBuf(char *buf, uint64_t val, size_t sz) {
  switch (sz) {
  case 1:
    *buf = (char)val;
    break;
  case 2:
    *(uint16_t *)buf = val;
    break;
  case 4:
    *(uint32_t *)buf = val;
    break;
  case 8:
    *(uint64_t *)buf = val;
    break;
  default:
    assert(false);
    break;
  }
}

int64_t evalRval(Node *node, char **label) {
  switch (node->kind) {
  case ND_VAR:
    if (!node->var->is_global) {
      compErrorToken(node->tok->str, "not a compile-time constant");
    }
    *label = node->var->name;
    return 0;
  case ND_DEREF:
    return eval2(node->body, label);
  case ND_MEMBER:
    return evalRval(node->lhs, label) + (int64_t)node->var->offset;
  default:
    break;
  }
  compErrorToken(node->tok->str, "invalid initialiser");
  assert(false);
}

bool atInitialiserListEnd(void) {
  return equal(token, "}") || (equal(token, ",") && equal(token->next, "}"));
}

bool consumeInitialiserListEnd(void) {
  if (consume("}")) {
    return true;
  }
  if (equal(token, ",") && equal(token->next, "}")) {
    token = token->next->next;
    return true;
  }
  return false;
}

Type *copyStructType(Type *src) {
  Type *ty = calloc(1, sizeof(Type));
  *ty = *src;

  Obj head = {0};
  Obj *cur = &head;
  for (Obj *mem = ty->members; mem; mem = mem->next) {
    Obj *m = calloc(1, sizeof(Obj));
    *m = *mem;
    cur = cur->next = m;
  }

  ty->members = head.next;
  return ty;
}

Obj *newAnonGlobalVar(Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->next = globals;
  var->name = newUniqueLabel();
  var->ty = ty;
  var->align = ty->align;
  var->is_global = true;
  var->is_definition = true;
  globals = var;
  return var;
}

Node *newNodeDo(void) {
  Node *node = newNode(ND_DO);

  char *brk = cur_brk_label;
  char *cont = cur_cont_label;
  cur_brk_label = node->brk_label = newUniqueLabel();
  cur_cont_label = node->cont_label = newUniqueLabel();

  node->then = stmt();

  cur_brk_label = brk;
  cur_cont_label = cont;

  expectWhile();
  expect("(");
  node->cond = expr();
  expect(")");
  expect(";");
  return node;
}

Token *createIdent(const char *name) {
  Token *ident = calloc(1, sizeof(Token));
  ident->str = name;
  ident->len = strlen(name);
  return ident;
}

Node *newNodeUlong(int64_t val) {
  Node *node = newNodeNum(val);
  node->ty = ty_ulong;
  return node;
}
