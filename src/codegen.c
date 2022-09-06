#include "codegen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "comp_err.h"
#include "defs.h"

extern Obj *prog;
extern Obj *globals;
static size_t label_num = 1;
static const char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static const char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static Obj *cur_fn = NULL;

static void genAddr(Node *node);
static void genStmt(Node *node);
static void genExpr(Node *node);
static void load(Type *ty);
static void store(Type *ty);

void gen() {
  puts(".intel_syntax noprefix");
  for (Obj *var = globals; var; var = var->next) {
    puts(".data");
    printf(".globl %s\n", var->name);
    printf("%s:\n", var->name);
    if (var->init_data) {
      for (size_t i = 0; i < var->ty->size; ++i) {
        printf("  .byte %d\n", var->init_data[i]);
      }
    } else {
      printf("  .zero %zu\n", var->ty->size);
    }
  }
  for (Obj *fn = prog; fn; fn = fn->next) {
    printf(".globl %s\n", fn->name);
    puts(".text");
    printf("%s:\n", fn->name);
    cur_fn = fn;

    puts("  push rbp");
    puts("  mov rbp, rsp");
    printf("  sub rsp, %zu\n", fn->stack_size);

    size_t i = 0;
    for (Obj *param = fn->params; param; param = param->next) {
      if (param->ty->size == 1) {
        printf("  mov [rbp-%zu], %s\n", param->offset,
               argreg8[fn->param_cnt - (i++) - 1]);
      } else {
        printf("  mov [rbp-%zu], %s\n", param->offset,
               argreg64[fn->param_cnt - (i++) - 1]);
      }
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
    store(node->ty);
    return;
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next) {
      genStmt(n);
    }
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
      printf("  pop %s\n", argreg64[i]);
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
    if (node->var->is_global) {
      printf("  lea rax, [rip+%s]\n", node->var->name);
    } else {
      printf("  lea rax, [rbp-%zu]\n", node->var->offset);
    }
    return;
  case ND_DEREF:
    genExpr(node->body);
    return;
  default:
    break;
  }
  compError("not an lvalue");
}

void store(Type *ty) {
  puts("  pop rdi");
  if (ty->size == 1) {
    puts("  mov [rdi], al");
  } else {
    puts("  mov [rdi], rax");
  }
}

void load(Type *ty) {
  if (ty->kind == TY_ARR) {
    return;
  }
  if (ty->size == 1) {
    puts("  movsbq rax, [rax]");
  } else {
    puts("  mov rax, [rax]");
  }
}
