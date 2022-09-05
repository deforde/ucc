#include "codegen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "comp_err.h"
#include "defs.h"

extern Function *prog;
static size_t label_num = 1;
static const char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static Function *cur_fn = NULL;

static void genAddr(Node *node);
static void genStmt(Node *node);
static void genExpr(Node *node);
static void load(Type *ty);

void gen() {
  puts(".intel_syntax noprefix");
  for (Function *fn = prog; fn; fn = fn->next) {
    printf(".globl %s\n", fn->name);
    printf("%s:\n", fn->name);
    cur_fn = fn;

    puts("  push rbp");
    puts("  mov rbp, rsp");
    printf("  sub rsp, %zu\n", fn->stack_size);

    size_t i = 0;
    for (Var *param = fn->params; param; param = param->next) {
      printf("  mov [rbp-%zu], %s\n", param->offset,
             argreg[fn->param_cnt - (i++) - 1]);
    }

    genStmt(fn->body);

    printf(".L.return.%s:\n", fn->name);
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
  }
}

void genStmt(Node *node) {
  switch (node->kind) {
  case ND_BLK:
    for (Node *n = node->body; n; n = n->next) {
      genStmt(n);
    }
    return;
  case ND_IF:
    genExpr(node->cond);
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
    if (node->pre) {
      genStmt(node->pre);
    }
    printf(".L.begin%zu:\n", label_num);
    if (node->cond) {
      genExpr(node->cond);
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
      genExpr(node->cond);
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
    printf("  jmp .L.return.%s\n", cur_fn->name);
    return;
  default:
    break;
  }
  genExpr(node);
}

void genExpr(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("  mov rax, %d\n", node->val);
    return;
  case ND_VAR:
    genAddr(node);
    load(node->ty);
    return;
  case ND_ASS:
    genAddr(node->lhs);
    puts("  push rax");
    genExpr(node->rhs);
    puts("  pop rdi");
    puts("  mov [rdi], rax");
    return;
  case ND_ADDR:
    genAddr(node->body);
    return;
  case ND_DEREF:
    genExpr(node->body);
    load(node->ty);
    return;
  case ND_FUNCCALL: {
    size_t nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      genExpr(arg);
      puts("  push rax");
      ++nargs;
    }
    for (ssize_t i = (ssize_t)nargs - 1; i >= 0; --i) {
      printf("  pop %s\n", argreg[i]);
    }
    puts("  mov rax, 0");
    printf("  call %s\n", node->funcname);
    return;
  }
  default:
    break;
  }

  genExpr(node->rhs);
  puts("  push rax");
  genExpr(node->lhs);
  puts("  pop rdi");

  switch (node->kind) {
  case ND_ADD:
    puts("  add rax, rdi");
    return;
  case ND_SUB:
    puts("  sub rax, rdi");
    return;
  case ND_MUL:
    puts("  imul rax, rdi");
    return;
  case ND_DIV:
    puts("  cqo");
    puts("  idiv rdi");
    return;
  case ND_EQ:
    puts("  cmp rax, rdi");
    puts("  sete al");
    puts("  movzb rax, al");
    return;
  case ND_NE:
    puts("  cmp rax, rdi");
    puts("  setne al");
    puts("  movzb rax, al");
    return;
  case ND_LT:
    puts("  cmp rax, rdi");
    puts("  setl al");
    puts("  movzb rax, al");
    return;
  case ND_LE:
    puts("  cmp rax, rdi");
    puts("  setle al");
    puts("  movzb rax, al");
    return;
  default:
    break;
  }

  compError("invalid expression");
}

void genAddr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    printf("  lea rax, [rbp-%zu]\n", node->var->offset);
    return;
  case ND_DEREF:
    genExpr(node->body);
    return;
  default:
    break;
  }
  compError("not an lvalue");
}

void load(Type *ty) {
  if (ty->kind == TY_ARR) {
    return;
  }
  puts("  mov rax, [rax]");
}
