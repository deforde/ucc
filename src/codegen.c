#include "codegen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "node.h"
#include "parse.h"

static size_t jump_label_num = 1;
extern Node *code[];

static void genLval(Node *node);
static void genNode(Node *node);

void gen() {
  for (size_t i = 0; code[i]; ++i) {
    genNode(code[i]);
    puts("  pop rax");
  }
}

void genNode(Node *node) {
  switch (node->type) {
  case ND_IF:
    genNode(node->lhs);
    puts("  pop rax");
    puts("  cmp rax, 0");
    printf("  je .Lend%zu\n", jump_label_num);
    genNode(node->rhs);
    printf(".Lend%zu:\n", jump_label_num);
    jump_label_num++;
    return;
  case ND_RET:
    genNode(node->lhs);
    puts("  pop rax");
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
    return;
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
    genNode(node->rhs);
    puts("  pop rdi");
    puts("  pop rax");
    puts("  mov [rax], rdi");
    puts("  push rdi");
    return;
  default:
    break;
  }
  genNode(node->lhs);
  genNode(node->rhs);
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
