#ifndef DEFS_H
#define DEFS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Type Type;
typedef struct Node Node;
typedef struct Token Token;
typedef struct Obj Obj;
typedef struct VarScope VarScope;
typedef struct Scope Scope;

typedef enum {
  ND_ADD,
  ND_ADDR,
  ND_ASS,
  ND_BLK,
  ND_COMMA,
  ND_DEREF,
  ND_DIV,
  ND_EQ,
  ND_FOR,
  ND_FUNCCALL,
  ND_IF,
  ND_LE,
  ND_LT,
  ND_MEMBER,
  ND_MUL,
  ND_NE,
  ND_NUM,
  ND_RET,
  ND_STMT_EXPR,
  ND_SUB,
  ND_VAR,
  ND_WHILE,
} NodeKind;

typedef enum {
  TY_ARR,
  TY_CHAR,
  TY_INT,
  TY_PTR,
  TY_STRUCT,
} TypeKind;

typedef enum {
  TK_ELSE,
  TK_EOF,
  TK_FOR,
  TK_IDENT,
  TK_IF,
  TK_NUM,
  TK_RESERVED,
  TK_RET,
  TK_SIZEOF,
  TK_STR,
  TK_WHILE,
} TokenKind;

struct Type {
  TypeKind kind;
  Type *base;
  Obj *members;
  size_t size;
  size_t align;
  size_t arr_len;
};

struct Token {
  TokenKind kind;
  Token *next;
  int val;
  const char *str;
  size_t len;
  size_t line_num;
};

struct Obj {
  Obj *next;
  char *name;
  Type *ty;
  // variable
  size_t offset;
  bool is_global;
  const char *init_data;
  // function
  Node *body;
  Obj *params;
  size_t param_cnt;
  size_t stack_size;
  Obj *locals;
};

struct Node {
  NodeKind kind;
  Type *ty;
  Node *next;
  Node *lhs;
  Node *rhs;
  Node *body;
  Node *cond;
  Node *then;
  Node *els;
  Node *pre;
  Node *post;
  Obj *var;
  int val;
  const char *funcname;
  Node *args;
  Token *tok;
};

struct VarScope {
  VarScope *next;
  Obj *var;
};

struct Scope {
  Scope *next;
  VarScope *vars;
};

#endif // DEFS_H
