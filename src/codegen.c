#include "codegen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "node.h"
#include "parse.h"

static size_t label_num = 1;
extern Node prog;

static void genLval(Node *node);
static void genStmt(Node *node);
static void genExpr(Node *node);

void gen() {
  puts(".intel_syntax noprefix");
  puts(".globl main");
  puts("main:");
  puts("  push rbp");
  puts("  mov rbp, rsp");
  puts("  sub rsp, 208");

  genStmt(prog.body);

  puts("  mov rsp, rbp");
  puts("  pop rbp");
  puts("  ret");
}

void genStmt(Node *node) {
  switch (node->type) {
  case ND_BLK:
    for (Node *n = node->body; n; n = n->next) {
      genStmt(n);
      puts("  pop rax");
    }
    return;
  case ND_IF:
    genExpr(node->cond);
    puts("  pop rax");
    puts("  cmp rax, 0");
    printf("  je .L.else%zu\n", label_num);
    genStmt(node->then);
    printf("  jmp .L.end%zu\n", label_num);
    printf(".L.else%zu:\n", label_num);
    if (node->els) {
      genStmt(node->els);
    }
    printf(".L.end%zu:\n", label_num);
    label_num++;
    return;
  case ND_FOR:
    genStmt(node->pre);
    printf(".L.begin%zu:\n", label_num);
    if (node->cond) {
      genStmt(node->cond);
      puts("  cmp rax, 0");
      printf("  je .L.end%zu\n", label_num);
    }
    genStmt(node->body);
    if (node->post) {
      genExpr(node->post);
    }
    printf("  jmp .L.begin%zu\n", label_num);
    printf(".L.end%zu:\n", label_num);
    label_num++;
    return;
  case ND_WHILE:
    printf(".L.begin%zu:\n", label_num);
    if (node->cond) {
      genStmt(node->cond);
      puts("  cmp rax, 0");
      printf("  je .L.end%zu\n", label_num);
    }
    genStmt(node->body);
    if (node->post) {
      genExpr(node->post);
    }
    printf("  jmp .L.begin%zu\n", label_num);
    printf(".L.end%zu:\n", label_num);
    label_num++;
    return;
  case ND_RET:
    genExpr(node->lhs);
    puts("  pop rax");
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
    return;
  default:
    genExpr(node);
    break;
  }
}

void genExpr(Node *node) {
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
    genExpr(node->rhs);
    puts("  pop rdi");
    puts("  pop rax");
    puts("  mov [rax], rdi");
    puts("  push rdi");
    return;
  default:
    break;
  }

  genExpr(node->lhs);
  genExpr(node->rhs);
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

void genLval(Node *node) {
  if (node->type != ND_LVAR) {
    fprintf(stderr, "Expected node type LVAR, got %i\n", node->type);
  }
  puts("  mov rax, rbp");
  printf("  sub rax, %zu\n", node->offset);
  puts("  push rax");
}
