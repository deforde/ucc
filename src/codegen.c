#include "codegen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "comp_err.h"
#include "defs.h"

extern Obj *prog;
extern Obj *globals;
extern FILE *output;
extern const char *input_file_path;
static size_t label_num = 1;
static const char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static const char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static const char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static const char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static Obj *cur_fn = NULL;

static void genAddr(Node *node);
static void genExpr(Node *node);
static void genStmt(Node *node);
static void load(Type *ty);
static void store(Type *ty);
static void storeArgReg(size_t r, size_t offset, size_t sz);

void gen() {
  fprintf(output, ".file 1 \"%s\"\n", input_file_path);
  fprintf(output, ".intel_syntax noprefix\n");
  for (Obj *var = globals; var; var = var->next) {
    fprintf(output, ".data\n");
    fprintf(output, ".globl %s\n", var->name);
    fprintf(output, "%s:\n", var->name);
    if (var->init_data) {
      for (size_t i = 0; i < var->ty->size; ++i) {
        fprintf(output, "  .byte %d\n", var->init_data[i]);
      }
    } else {
      fprintf(output, "  .zero %zu\n", var->ty->size);
    }
  }
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->body) {
      continue;
    }

    fprintf(output, ".globl %s\n", fn->name);
    fprintf(output, ".text\n");
    fprintf(output, "%s:\n", fn->name);
    cur_fn = fn;

    fprintf(output, "  push rbp\n");
    fprintf(output, "  mov rbp, rsp\n");
    fprintf(output, "  sub rsp, %zu\n", fn->stack_size);

    size_t i = 0;
    for (Obj *param = fn->params; param; param = param->next) {
      storeArgReg(fn->param_cnt - (i++) - 1, param->offset, param->ty->size);
    }

    genStmt(fn->body);

    fprintf(output, ".L.return.%s:\n", fn->name);
    fprintf(output, "  mov rsp, rbp\n");
    fprintf(output, "  pop rbp\n");
    fprintf(output, "  ret\n");
  }
}

void genStmt(Node *node) {
  fprintf(output, "  .loc 1 %zu\n", node->tok->line_num);
  switch (node->kind) {
  case ND_BLK:
    for (Node *n = node->body; n; n = n->next) {
      genStmt(n);
    }
    return;
  case ND_IF:
    genExpr(node->cond);
    fprintf(output, "  cmp rax, 0\n");
    fprintf(output, "  je .L.else%zu\n", label_num);
    genStmt(node->then);
    fprintf(output, "  jmp .L.end%zu\n", label_num);
    fprintf(output, ".L.else%zu:\n", label_num);
    if (node->els) {
      genStmt(node->els);
    }
    fprintf(output, ".L.end%zu:\n", label_num);
    label_num++;
    return;
  case ND_FOR:
    if (node->pre) {
      genStmt(node->pre);
    }
    fprintf(output, ".L.begin%zu:\n", label_num);
    if (node->cond) {
      genExpr(node->cond);
      fprintf(output, "  cmp rax, 0\n");
      fprintf(output, "  je .L.end%zu\n", label_num);
    }
    genStmt(node->body);
    if (node->post) {
      genExpr(node->post);
    }
    fprintf(output, "  jmp .L.begin%zu\n", label_num);
    fprintf(output, ".L.end%zu:\n", label_num);
    label_num++;
    return;
  case ND_WHILE:
    fprintf(output, ".L.begin%zu:\n", label_num);
    if (node->cond) {
      genExpr(node->cond);
      fprintf(output, "  cmp rax, 0\n");
      fprintf(output, "  je .L.end%zu\n", label_num);
    }
    genStmt(node->body);
    if (node->post) {
      genExpr(node->post);
    }
    fprintf(output, "  jmp .L.begin%zu\n", label_num);
    fprintf(output, ".L.end%zu:\n", label_num);
    label_num++;
    return;
  case ND_RET:
    genExpr(node->lhs);
    fprintf(output, "  jmp .L.return.%s\n", cur_fn->name);
    return;
  default:
    break;
  }
  genExpr(node);
}

void genExpr(Node *node) {
  fprintf(output, "  .loc 1 %zu\n", node->tok->line_num);
  switch (node->kind) {
  case ND_NUM:
    fprintf(output, "  mov rax, %ld\n", node->val);
    return;
  case ND_VAR:
  case ND_MEMBER:
    genAddr(node);
    load(node->ty);
    return;
  case ND_ASS:
    genAddr(node->lhs);
    fprintf(output, "  push rax\n");
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
  case ND_COMMA:
    genExpr(node->lhs);
    genExpr(node->rhs);
    return;
  case ND_FUNCCALL: {
    size_t nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      genExpr(arg);
      fprintf(output, "  push rax\n");
      ++nargs;
    }
    for (ssize_t i = (ssize_t)nargs - 1; i >= 0; --i) {
      fprintf(output, "  pop %s\n", argreg64[i]);
    }
    fprintf(output, "  mov rax, 0\n");
    fprintf(output, "  call %s\n", node->funcname);
    return;
  }
  default:
    break;
  }

  genExpr(node->rhs);
  fprintf(output, "  push rax\n");
  genExpr(node->lhs);
  fprintf(output, "  pop rdi\n");

  switch (node->kind) {
  case ND_ADD:
    fprintf(output, "  add rax, rdi\n");
    return;
  case ND_SUB:
    fprintf(output, "  sub rax, rdi\n");
    return;
  case ND_MUL:
    fprintf(output, "  imul rax, rdi\n");
    return;
  case ND_DIV:
    fprintf(output, "  cqo\n");
    fprintf(output, "  idiv rdi\n");
    return;
  case ND_EQ:
    fprintf(output, "  cmp rax, rdi\n");
    fprintf(output, "  sete al\n");
    fprintf(output, "  movzb rax, al\n");
    return;
  case ND_NE:
    fprintf(output, "  cmp rax, rdi\n");
    fprintf(output, "  setne al\n");
    fprintf(output, "  movzb rax, al\n");
    return;
  case ND_LT:
    fprintf(output, "  cmp rax, rdi\n");
    fprintf(output, "  setl al\n");
    fprintf(output, "  movzb rax, al\n");
    return;
  case ND_LE:
    fprintf(output, "  cmp rax, rdi\n");
    fprintf(output, "  setle al\n");
    fprintf(output, "  movzb rax, al\n");
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
      fprintf(output, "  lea rax, [rip+%s]\n", node->var->name);
    } else {
      fprintf(output, "  lea rax, [rbp-%zu]\n", node->var->offset);
    }
    return;
  case ND_DEREF:
    genExpr(node->body);
    return;
  case ND_COMMA:
    genExpr(node->lhs);
    genAddr(node->rhs);
    return;
  case ND_MEMBER:
    genAddr(node->lhs);
    fprintf(output, "  add rax, %zu\n", node->var->offset);
    return;
  default:
    break;
  }
  compError("not an lvalue");
}

void store(Type *ty) {
  fprintf(output, "  pop rdi\n");
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    for (size_t i = 0; i < ty->size; i++) {
      fprintf(output, "  mov r8b, [rax+%zu]\n", i);
      fprintf(output, "  mov [rdi+%zu], r8b\n", i);
    }
    return;
  }
  if (ty->size == 1) {
    fprintf(output, "  mov [rdi], al\n");
  } else if (ty->size == 2) {
    fprintf(output, "  mov [rdi], ax\n");
  } else if (ty->size == 4) {
    fprintf(output, "  mov [rdi], eax\n");
  } else {
    fprintf(output, "  mov [rdi], rax\n");
  }
}

void storeArgReg(size_t r, size_t offset, size_t sz) {
  switch (sz) {
  case 1:
    fprintf(output, "  mov [rbp-%zu], %s\n", offset, argreg8[r]);
    return;
  case 2:
    fprintf(output, "  mov [rbp-%zu], %s\n", offset, argreg16[r]);
    return;
  case 4:
    fprintf(output, "  mov [rbp-%zu], %s\n", offset, argreg32[r]);
    return;
  case 8:
    fprintf(output, "  mov [rbp-%zu], %s\n", offset, argreg64[r]);
    return;
  }
  assert(false);
}

void load(Type *ty) {
  if (ty->kind == TY_ARR || ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    return;
  }
  if (ty->size == 1) {
    fprintf(output, "  movsbq rax, [rax]\n");
  } else if (ty->size == 2) {
    fprintf(output, "  movswq rax, [rax]\n");
  } else if (ty->size == 4) {
    fprintf(output, "  movsxd rax, [rax]\n");
  } else {
    fprintf(output, "  mov rax, [rax]\n");
  }
}
