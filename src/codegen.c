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
static size_t stack_depth = 0;
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
static void pop(const char *arg);
static void println(const char *fmt, ...);
static void push(void);
static void store(Type *ty);
static void storeArgReg(size_t r, size_t offset, size_t sz);

void gen() {
  println(".file 1 \"%s\"", input_file_path);
  println(".intel_syntax noprefix");
  for (Obj *var = globals; var; var = var->next) {
    if (!var->is_definition) {
      continue;
    }
    if (var->is_static) {
      println(".local %s", var->name);
    } else {
      println(".globl %s", var->name);
    }
    println(".align %zu", var->align);
    if (var->init_data) {
      println("  .data");
      println("%s:", var->name);
      Relocation *rel = var->rel;
      ssize_t pos = 0;
      while (pos < var->ty->size) {
        if (rel && (ssize_t)rel->offset == pos) {
          println("  .quad %s%+ld", rel->label, rel->addend);
          rel = rel->next;
          pos += 8;
        } else {
          println("  .byte %d", var->init_data[pos++]);
        }
      }
      continue;
    }
    println("  .bss");
    println("%s:", var->name);
    println("  .zero %zu", var->ty->size);
  }
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->body) {
      continue;
    }

    println(".%s %s", fn->is_global ? "globl" : "local", fn->name);
    println(".text");
    println("%s:", fn->name);
    cur_fn = fn;

    println("  push rbp");
    println("  mov rbp, rsp");
    println("  sub rsp, %zu", fn->stack_size);

    size_t i = 0;
    for (Obj *param = fn->params; param; param = param->next) {
      storeArgReg(fn->param_cnt - (i++) - 1, param->offset, param->ty->size);
    }

    genStmt(fn->body);

    println(".L.return.%s:", fn->name);
    println("  mov rsp, rbp");
    pop("rbp");
    println("  ret");
  }
}

void genStmt(Node *node) {
  println("  .loc 1 %zu", node->tok->line_num);
  switch (node->kind) {
  case ND_BLK:
    for (Node *n = node->body; n; n = n->next) {
      genStmt(n);
    }
    return;
  case ND_IF: {
    const size_t c = label_num++;
    genExpr(node->cond);
    println("  cmp rax, 0");
    println("  je .L.else%zu", c);
    genStmt(node->then);
    println("  jmp .L.end%zu", c);
    println(".L.else%zu:", c);
    if (node->els) {
      genStmt(node->els);
    }
    println(".L.end%zu:", c);
    return;
  }
  case ND_FOR: {
    const size_t c = label_num++;
    if (node->pre) {
      genStmt(node->pre);
    }
    println(".L.begin%zu:", c);
    if (node->cond) {
      genExpr(node->cond);
      println("  cmp rax, 0");
      println("  je %s", node->brk_label);
    }
    genStmt(node->body);
    println("%s:", node->cont_label);
    if (node->post) {
      genExpr(node->post);
    }
    println("  jmp .L.begin%zu", c);
    println("%s:", node->brk_label);
    return;
  }
  case ND_WHILE: {
    const size_t c = label_num++;
    println(".L.begin%zu:", c);
    println("%s:", node->cont_label);
    if (node->cond) {
      genExpr(node->cond);
      println("  cmp rax, 0");
      println("  je %s", node->brk_label);
    }
    genStmt(node->body);
    println("  jmp .L.begin%zu", c);
    println("%s:", node->brk_label);
    return;
  }
  case ND_GOTO:
    println("  jmp %s", node->unique_label);
    return;
  case ND_LABEL:
    println("%s:", node->unique_label);
    genStmt(node->lhs);
    return;
  case ND_RET:
    if (node->lhs) {
      genExpr(node->lhs);
    }
    println("  jmp .L.return.%s", cur_fn->name);
    return;
  case ND_SWITCH:
    genExpr(node->cond);
    for (Node *n = node->case_next; n; n = n->case_next) {
      const char *reg = (node->cond->ty->size == 8) ? "rax" : "eax";
      println("  cmp %s, %ld", reg, n->val);
      println("  je %s", n->label);
    }
    if (node->default_case) {
      println("  jmp %s", node->default_case->label);
    }
    println("  jmp %s", node->brk_label);
    genStmt(node->then);
    println("%s:", node->brk_label);
    return;
  case ND_CASE:
    println("%s:", node->label);
    genStmt(node->lhs);
    return;
  case ND_DO: {
    const size_t c = label_num++;
    println(".L.begin%zu:", c);
    genStmt(node->then);
    println("%s:", node->cont_label);
    genExpr(node->cond);
    println("  cmp rax, 0");
    println("  jne .L.begin%zu", c);
    println("%s:", node->brk_label);
    return;
  }
  default:
    break;
  }
  genExpr(node);
}

void genExpr(Node *node) {
  println("  .loc 1 %zu", node->tok->line_num);
  switch (node->kind) {
  case ND_NULL_EXPR:
    return;
  case ND_NUM:
    println("  mov rax, %ld", node->val);
    return;
  case ND_VAR:
  case ND_MEMBER:
    genAddr(node);
    load(node->ty);
    return;
  case ND_ASS:
    genAddr(node->lhs);
    push();
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
  case ND_TERN: {
    const size_t c = label_num++;
    genExpr(node->cond);
    println("  cmp rax, 0");
    println("  je .L.else.%zu", c);
    genExpr(node->then);
    println("  jmp .L.end.%zu", c);
    println(".L.else.%zu:", c);
    genExpr(node->els);
    println(".L.end.%zu:", c);
    return;
  }
  case ND_NOT:
    genExpr(node->lhs);
    println("  cmp rax, 0");
    println("  sete al");
    println("  movzx rax, al");
    return;
  case ND_BITNOT:
    genExpr(node->lhs);
    println("  not rax");
    return;
  case ND_LOGAND: {
    const size_t c = label_num++;
    genExpr(node->lhs);
    println("  cmp rax, 0");
    println("  je .L.false.%zu", c);
    genExpr(node->rhs);
    println("  cmp rax, 0");
    println("  je .L.false.%zu", c);
    println("  mov rax, 1");
    println("  jmp .L.end.%zu", c);
    println(".L.false.%zu:", c);
    println("  mov rax, 0");
    println(".L.end.%zu:", c);
    return;
  }
  case ND_LOGOR: {
    const size_t c = label_num++;
    genExpr(node->lhs);
    println("  cmp rax, 0");
    println("  jne .L.true.%zu", c);
    genExpr(node->rhs);
    println("  cmp rax, 0");
    println("  jne .L.true.%zu", c);
    println("  mov rax, 0");
    println("  jmp .L.end.%zu", c);
    println(".L.true.%zu:", c);
    println("  mov rax, 1");
    println(".L.end.%zu:", c);
    return;
  }
  case ND_FUNCCALL: {
    size_t nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      genExpr(arg);
      push();
      ++nargs;
    }
    for (ssize_t i = (ssize_t)nargs - 1; i >= 0; --i) {
      pop(argreg64[i]);
    }
    println("  mov rax, 0");
    if (stack_depth % 2 == 0) {
      println("  call %s", node->funcname);
    } else {
      println("  sub rsp, 8");
      println("  call %s", node->funcname);
      println("  add rsp, 8");
    }
    return;
  }
  case ND_MEMZERO:
    println("  mov rcx, %zu", node->var->ty->size);
    println("  lea rdi, [rbp-%zu]", node->var->offset);
    println("  mov al, 0");
    println("  rep stosb");
    return;
  default:
    break;
  }

  genExpr(node->rhs);
  push();
  genExpr(node->lhs);
  pop("rdi");

  char *ax = NULL;
  char *di = NULL;

  if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base) {
    ax = "rax";
    di = "rdi";
  } else {
    ax = "eax";
    di = "edi";
  }

  switch (node->kind) {
  case ND_ADD:
    println("  add %s, %s", ax, di);
    return;
  case ND_SUB:
    println("  sub %s, %s", ax, di);
    return;
  case ND_MUL:
    println("  imul %s, %s", ax, di);
    return;
  case ND_DIV:
    if (node->lhs->ty->size == 8) {
      println("  cqo");
    } else {
      println("  cdq");
    }
    println("  idiv %s", di);
    return;
  case ND_MOD:
    if (node->lhs->ty->size == 8) {
      println("  cqo");
    } else {
      println("  cdq");
    }
    println("  idiv %s", di);
    println("  mov rax, rdx");
    return;
  case ND_BITAND:
    println("  and rax, rdi");
    return;
  case ND_BITOR:
    println("  or rax, rdi");
    return;
  case ND_BITXOR:
    println("  xor rax, rdi");
    return;
  case ND_EQ:
    println("  cmp %s, %s", ax, di);
    println("  sete al");
    println("  movzx %s, al", ax);
    return;
  case ND_NE:
    println("  cmp %s, %s", ax, di);
    println("  setne al");
    println("  movzx %s, al", ax);
    return;
  case ND_LT:
    println("  cmp %s, %s", ax, di);
    println("  setl al");
    println("  movzx %s, al", ax);
    return;
  case ND_LE:
    println("  cmp %s, %s", ax, di);
    println("  setle al");
    println("  movzx %s, al", ax);
    return;
  case ND_SHL:
    println("  mov rcx, rdi");
    println("  shl %s, cl", ax);
    return;
  case ND_SHR:
    println("  mov rcx, rdi");
    println("  sar %s, cl", ax);
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
      println("  lea rax, [rip+%s]", node->var->name);
    } else {
      println("  lea rax, [rbp-%zu]", node->var->offset);
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
    println("  add rax, %zu", node->var->offset);
    return;
  default:
    break;
  }
  compError("not an lvalue");
}

void store(Type *ty) {
  pop("rdi");
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    for (ssize_t i = 0; i < ty->size; i++) {
      println("  mov r8b, [rax+%zu]", i);
      println("  mov [rdi+%zu], r8b", i);
    }
    return;
  }
  if (ty->size == 1) {
    println("  mov [rdi], al");
  } else if (ty->size == 2) {
    println("  mov [rdi], ax");
  } else if (ty->size == 4) {
    println("  mov [rdi], eax");
  } else {
    println("  mov [rdi], rax");
  }
}

void storeArgReg(size_t r, size_t offset, size_t sz) {
  switch (sz) {
  case 1:
    println("  mov [rbp-%zu], %s", offset, argreg8[r]);
    return;
  case 2:
    println("  mov [rbp-%zu], %s", offset, argreg16[r]);
    return;
  case 4:
    println("  mov [rbp-%zu], %s", offset, argreg32[r]);
    return;
  case 8:
    println("  mov [rbp-%zu], %s", offset, argreg64[r]);
    return;
  }
  assert(false);
}

void load(Type *ty) {
  if (ty->kind == TY_ARR || ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    return;
  }
  if (ty->size == 1) {
    println("  movsx eax, byte ptr [rax]");
  } else if (ty->size == 2) {
    println("  movsx eax, word ptr [rax]");
  } else if (ty->size == 4) {
    println("  movsxd rax, [rax]");
  } else {
    println("  mov rax, [rax]");
  }
}

void cast(Type *from, Type *to) {
  if (to->kind == TY_VOID) {
    return;
  }
  if (to->kind == TY_BOOL) {
    cmpZero(to);
    println("  setne al");
    println("  movzx eax, al");
    return;
  }
  int t1 = getTypeId(from);
  int t2 = getTypeId(to);
  if (cast_table[t1][t2]) {
    println("  %s", cast_table[t1][t2]);
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
    println("  cmp eax, 0");
  } else {
    println("  cmp rax, 0");
  }
}

void pop(const char *arg) {
  println("  pop %s", arg);
  stack_depth--;
}

void push(void) {
  println("  push rax");
  stack_depth++;
}

void println(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(output, fmt, args);
  va_end(args);
  fprintf(output, "\n");
}
