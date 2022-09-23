#include "codegen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "comp_err.h"
#include "defs.h"
#include "parse.h"

enum { I8, I16, I32, I64 };

extern FILE *output;
extern const char *input_file_path;
extern Obj *prog;
extern Obj *globals;
static Obj *cur_fn = NULL;
static size_t label_num = 1;
static const char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static const char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static const char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static const char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static const char i32i8[] = "movsx eax, al";
static const char i32i16[] = "movsx eax, ax";
static const char i32i64[] = "movsxd rax, eax";

static const char *cast_table[][4] = {
    {NULL, NULL, NULL, i32i64},
    {i32i8, NULL, NULL, i32i64},
    {i32i8, i32i16, NULL, i32i64},
    {i32i8, i32i16, NULL, NULL},
};

static int getTypeId(Type *ty);
static void cast(Type *from, Type *to);
static void cmpZero(Type *ty);
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

    fprintf(output, ".%s %s\n", fn->is_global ? "globl" : "local", fn->name);
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
  case ND_CAST:
    genExpr(node->lhs);
    cast(node->lhs->ty, node->ty);
    return;
  case ND_NOT:
    genExpr(node->lhs);
    fprintf(output, "  cmp rax, 0\n");
    fprintf(output, "  sete al\n");
    fprintf(output, "  movzx rax, al\n");
    return;
  case ND_BITNOT:
    genExpr(node->lhs);
    fprintf(output, "  not rax\n");
    return;
  case ND_LOGAND: {
    const size_t c = label_num++;
    genExpr(node->lhs);
    fprintf(output, "  cmp rax, 0\n");
    fprintf(output, "  je .L.false.%zu\n", c);
    genExpr(node->rhs);
    fprintf(output, "  cmp rax, 0\n");
    fprintf(output, "  je .L.false.%zu\n", c);
    fprintf(output, "  mov rax, 1\n");
    fprintf(output, "  jmp .L.end.%zu\n", c);
    fprintf(output, ".L.false.%zu:\n", c);
    fprintf(output, "  mov rax, 0\n");
    fprintf(output, ".L.end.%zu:\n", c);
    return;
  }
  case ND_LOGOR: {
    const size_t c = label_num++;
    genExpr(node->lhs);
    fprintf(output, "  cmp rax, 0\n");
    fprintf(output, "  jne .L.true.%zu\n", c);
    genExpr(node->rhs);
    fprintf(output, "  cmp rax, 0\n");
    fprintf(output, "  jne .L.true.%zu\n", c);
    fprintf(output, "  mov rax, 0\n");
    fprintf(output, "  jmp .L.end.%zu\n", c);
    fprintf(output, ".L.true.%zu:\n", c);
    fprintf(output, "  mov rax, 1\n");
    fprintf(output, ".L.end.%zu:\n", c);
    return;
  }
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

  char *ax = NULL;
  char *di = NULL;

  if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base) {
    ax = "%rax";
    di = "%rdi";
  } else {
    ax = "%eax";
    di = "%edi";
  }

  switch (node->kind) {
  case ND_ADD:
    fprintf(output, "  add %s, %s\n", ax, di);
    return;
  case ND_SUB:
    fprintf(output, "  sub %s, %s\n", ax, di);
    return;
  case ND_MUL:
    fprintf(output, "  imul %s, %s\n", ax, di);
    return;
  case ND_DIV:
    if (node->lhs->ty->size == 8) {
      fprintf(output, "  cqo\n");
    } else {
      fprintf(output, "  cdq\n");
    }
    fprintf(output, "  idiv %s\n", di);
    return;
  case ND_MOD:
    if (node->lhs->ty->size == 8) {
      fprintf(output, "  cqo\n");
    } else {
      fprintf(output, "  cdq\n");
    }
    fprintf(output, "  idiv %s\n", di);
    fprintf(output, "  mov rax, rdx\n");
    return;
  case ND_BITAND:
    fprintf(output, "  and rax, rdi\n");
    return;
  case ND_BITOR:
    fprintf(output, "  or rax, rdi\n");
    return;
  case ND_BITXOR:
    fprintf(output, "  xor rax, rdi\n");
    return;
  case ND_EQ:
    fprintf(output, "  cmp %s, %s\n", ax, di);
    fprintf(output, "  sete al\n");
    fprintf(output, "  movzb %s, al\n", ax);
    return;
  case ND_NE:
    fprintf(output, "  cmp %s, %s\n", ax, di);
    fprintf(output, "  setne al\n");
    fprintf(output, "  movzb %s, al\n", ax);
    return;
  case ND_LT:
    fprintf(output, "  cmp %s, %s\n", ax, di);
    fprintf(output, "  setl al\n");
    fprintf(output, "  movzb %s, al\n", ax);
    return;
  case ND_LE:
    fprintf(output, "  cmp %s, %s\n", ax, di);
    fprintf(output, "  setle al\n");
    fprintf(output, "  movzb %s, al\n", ax);
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
    fprintf(output, "  movsbl eax, [rax]\n");
  } else if (ty->size == 2) {
    fprintf(output, "  movswl eax, [rax]\n");
  } else if (ty->size == 4) {
    fprintf(output, "  movsxd rax, [rax]\n");
  } else {
    fprintf(output, "  mov rax, [rax]\n");
  }
}

void cast(Type *from, Type *to) {
  if (to->kind == TY_VOID) {
    return;
  }
  if (to->kind == TY_BOOL) {
    cmpZero(to);
    fprintf(output, "  setne al\n");
    fprintf(output, "  movzx eax, al\n");
    return;
  }
  int t1 = getTypeId(from);
  int t2 = getTypeId(to);
  if (cast_table[t1][t2]) {
    fprintf(output, "  %s\n", cast_table[t1][t2]);
  }
}

int getTypeId(Type *ty) {
  switch (ty->kind) {
  case TY_CHAR:
    return I8;
  case TY_SHORT:
    return I16;
  case TY_INT:
    return I32;
  default:
    break;
  }
  return I64;
}

void cmpZero(Type *ty) {
  if (isInteger(ty) && ty->size <= 4) {
    fprintf(output, "  cmp eax, 0\n");
  } else {
    fprintf(output, "  cmp rax, 0\n");
  }
}
