#ifndef DEFS_H
#define DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct InitDesg InitDesg;
typedef struct Initialiser Initialiser;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Relocation Relocation;
typedef struct Scope Scope;
typedef struct TagScope TagScope;
typedef struct Token Token;
typedef struct Type Type;
typedef struct VarAttr VarAttr;
typedef struct VarScope VarScope;

typedef enum {
  ND_ADD,
  ND_ADDR,
  ND_ASS,
  ND_BITAND,
  ND_BITNOT,
  ND_BITOR,
  ND_BITXOR,
  ND_BLK,
  ND_CASE,
  ND_CAST,
  ND_COMMA,
  ND_DEREF,
  ND_DIV,
  ND_DO,
  ND_EQ,
  ND_EXPR,
  ND_FOR,
  ND_FUNCCALL,
  ND_GOTO,
  ND_IF,
  ND_LABEL,
  ND_LE,
  ND_LOGAND,
  ND_LOGOR,
  ND_LT,
  ND_MEMBER,
  ND_MEMZERO,
  ND_MOD,
  ND_MUL,
  ND_NE,
  ND_NOT,
  ND_NULL_EXPR,
  ND_NUM,
  ND_RET,
  ND_SHL,
  ND_SHR,
  ND_STMT_EXPR,
  ND_SUB,
  ND_SWITCH,
  ND_TERN,
  ND_VAR,
  ND_WHILE,
} NodeKind;

typedef enum {
  TY_ARR,
  TY_BOOL,
  TY_CHAR,
  TY_ENUM,
  TY_FUNC,
  TY_INT,
  TY_LONG,
  TY_PTR,
  TY_SHORT,
  TY_STRUCT,
  TY_UNION,
  TY_VOID,
} TypeKind;

typedef enum {
  TK_ELSE,
  TK_EOF,
  TK_FOR,
  TK_IDENT,
  TK_IF,
  TK_KWD,
  TK_NUM,
  TK_RESERVED,
  TK_RET,
  TK_SIZEOF,
  TK_STR,
  TK_WHILE,
} TokenKind;

struct Type {
  Type *next;
  TypeKind kind;
  Type *base;
  Obj *members;
  bool is_flexible;
  Type *ret_ty;
  Type *params;
  bool is_variadic;
  ssize_t size;
  size_t align;
  bool is_unsigned;
  ssize_t arr_len;
  Token *tok;
};

struct Token {
  TokenKind kind;
  Token *next;
  int64_t val;
  Type *ty;
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
  size_t align;
  bool is_global;
  bool is_definition;
  bool is_static;
  const char *init_data;
  Relocation *rel;
  // function
  Node *body;
  Obj *params;
  size_t param_cnt;
  size_t stack_size;
  Obj *locals;
  Obj *va_area;
};

struct Node {
  NodeKind kind;
  Type *ty;
  Type *func_ty;
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
  int64_t val;
  const char *funcname;
  Node *args;
  Token *tok;
  char *label;
  char *unique_label;
  Node *goto_next;
  char *brk_label;
  char *cont_label;
  Node *case_next;
  Node *default_case;
};

struct VarScope {
  VarScope *next;
  char *name;
  Obj *var;
  Type *type_def;
  Type *enum_ty;
  int enum_val;
};

struct Scope {
  Scope *next;
  VarScope *vars;
  TagScope *tags;
};

struct TagScope {
  TagScope *next;
  const char *name;
  Type *ty;
};

struct VarAttr {
  bool is_extern;
  bool is_static;
  bool is_typedef;
  size_t align;
};

struct Initialiser {
  Initialiser *next;
  Type *ty;
  Node *expr;
  Initialiser **children;
  bool is_flexible;
};

struct InitDesg {
  InitDesg *next;
  size_t idx;
  Obj *var;
  bool is_member;
};

struct Relocation {
  Relocation *next;
  size_t offset;
  char *label;
  size_t addend;
};

#endif // DEFS_H
