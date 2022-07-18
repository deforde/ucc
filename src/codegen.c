#include "codegen.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

#define MAX_CODE_LEN 100

typedef enum {
  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_EQ,
  ND_NE,
  ND_LT,
  ND_LE,
  ND_NUM,
  ND_ASS,
  ND_LVAR,
  ND_RET,
} NodeType;

struct Node {
  NodeType type;
  Node *lhs;
  Node *rhs;
  int val;
  size_t offset;
};

typedef struct Lvar Lvar;
struct Lvar {
  Lvar *next;
  const char *name;
  size_t len;
  size_t offset;
};

Node *code[MAX_CODE_LEN] = {NULL};
static Lvar *locals = NULL;

static Node *assign(void);
static Node *stmt(void);
static Node *newNode(NodeType type, Node *lhs, Node *rhs);
static Node *newNodeNum(int val);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *primary(void);
static Node *mul(void);
static Node *unary(void);
static void genLval(Node *node);
static Lvar *findLvar(Token *tok);

Node *assign(void) {
  Node *node = equality();
  if (consume("=")) {
    node = newNode(ND_ASS, node, assign());
  }
  return node;
}

Node *stmt(void) {
  Node *node = NULL;
  if (consumeReturn()) {
    node = calloc(1, sizeof(Node));
    node->type = ND_RET;
    node->lhs = expr();
  } else {
    node = expr();
  }
  expect(";");
  return node;
}

void program() {
  int i = 0;
  while (!isEOF()) {
    code[i++] = stmt();
  }
  code[i] = NULL;
}

Node *newNode(NodeType type, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->type = type;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *newNodeNum(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->type = ND_NUM;
  node->val = val;
  return node;
}

Node *newNodeIdent(Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->type = ND_LVAR;

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
      node = newNode(ND_ADD, node, mul());
    } else if (consume("-")) {
      node = newNode(ND_SUB, node, mul());
    } else {
      return node;
    }
  }
}

void gen(Node *node) {
  if (node->type == ND_RET) {
    gen(node->lhs);
    puts("  pop rax");
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
    return;
  }
  switch (node->type) {
  case ND_NUM:
    printf("  push %d\n", node->val);
    return;
  case ND_LVAR:
    genLval(node);
    puts("  pop rax");
    puts("  mov rax, [rax]");
    puts("  push rax");
    return;
  case ND_ASS:
    genLval(node->lhs);
    gen(node->rhs);
    puts("  pop rdi");
    puts("  pop rax");
    puts("  mov [rax], rdi");
    puts("  push rdi");
    return;
  default:
    break;
  }
  gen(node->lhs);
  gen(node->rhs);
  puts("  pop rdi");
  puts("  pop rax");
  switch (node->type) {
  case ND_ADD:
    puts("  add rax, rdi");
    break;
  case ND_SUB:
    puts("  sub rax, rdi");
    break;
  case ND_MUL:
    puts("  imul rax, rdi");
    break;
  case ND_DIV:
    puts("  cqo");
    puts("  idiv rdi");
    break;
  case ND_EQ:
    puts("  cmp rax, rdi");
    puts("  sete al");
    puts("  movzb rax, al");
    break;
  case ND_NE:
    puts("  cmp rax, rdi");
    puts("  setne al");
    puts("  movzb rax, al");
    break;
  case ND_LT:
    puts("  cmp rax, rdi");
    puts("  setl al");
    puts("  movzb rax, al");
    break;
  case ND_LE:
    puts("  cmp rax, rdi");
    puts("  setle al");
    puts("  movzb rax, al");
    break;
  default:
    assert(false);
    break;
  }
  puts("  push rax");
}

Node *unary(void) {
  if (consume("+")) {
    return unary();
  }
  if (consume("-")) {
    return newNode(ND_SUB, newNodeNum(0), unary());
  }
  return primary();
}

void genLval(Node *node) {
  if (node->type != ND_LVAR) {
    fprintf(stderr, "Expected node type LVAR, got %i\n", node->type);
  }
  puts("  mov rax, rbp");
  printf("  sub rax, %zu\n", node->offset);
  puts("  push rax");
}

Lvar *findLvar(Token *tok) {
  for (Lvar *var = locals; var; var = var->next) {
    if (var->len == tok->len && memcmp(tok->str, var->name, var->len) == 0) {
      return var;
    }
  }
  return NULL;
}
