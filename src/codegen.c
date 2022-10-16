#include "codegen.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "comp_err.h"
#include "defs.h"
#include "parse.h"

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64 };

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
static const char f32f64[] = "cvtss2sd  xmm0, xmm0";
static const char f32i16[] = "cvttss2si eax, xmm0; movsx eax, ax";
static const char f32i32[] = "cvttss2si  eax, xmm0";
static const char f32i64[] = "cvttss2si  rax, xmm0";
static const char f32i8[] = "cvttss2si eax, xmm0; movsx eax, al";
static const char f32u16[] = "cvttss2si eax, xmm0; movzx eax, ax";
static const char f32u32[] = "cvttss2si  rax, xmm0";
static const char f32u64[] = "cvttss2si  rax, xmm0";
static const char f32u8[] = "cvttss2si eax, xmm0; movzx eax, al";
static const char f64f32[] = "cvtsd2ss  xmm0, xmm0";
static const char f64i16[] = "cvttsd2si eax, xmm0; movsx eax, ax";
static const char f64i32[] = "cvttsd2si  eax, xmm0";
static const char f64i64[] = "cvttsd2si  rax, xmm0";
static const char f64i8[] = "cvttsd2si eax, xmm0; movsx eax, al";
static const char f64u16[] = "cvttsd2si eax, xmm0; movzx eax, ax";
static const char f64u32[] = "cvttsd2si  rax, xmm0";
static const char f64u64[] = "cvttsd2si  rax, xmm0";
static const char f64u8[] = "cvttsd2si eax, xmm0; movzx eax, al";
static const char i32f32[] = "cvtsi2ss  xmm0, eax";
static const char i32f64[] = "cvtsi2sd  xmm0, eax";
static const char i32i16[] = "movsx eax, ax";
static const char i32i64[] = "movsxd rax, eax";
static const char i32i8[] = "movsx eax, al";
static const char i32u16[] = "movzx eax, ax";
static const char i32u8[] = "movzx eax, al";
static const char i64f32[] = "cvtsi2ss  xmm0, rax";
static const char i64f64[] = "cvtsi2sd  xmm0, rax";
static const char u32f32[] = "mov eax, eax; cvtsi2ss xmm0, rax";
static const char u32f64[] = "mov eax, eax; cvtsi2sd xmm0, rax";
static const char u32i64[] = "mov eax, eax";
static const char u64f32[] = "cvtsi2ss  xmm0, rax";
static const char u64f64[] =
    "test rax, rax; js 1f; pxor xmm0, xmm0; cvtsi2sd xmm0, rax; jmp 2f; "
    "1: mov rdi, rax; and eax, 1; pxor xmm0, xmm0; shr rdi; "
    "or rdi, rax; cvtsi2sd xmm0, rdi; addsd xmm0, xmm0; 2:";

static const char *cast_table[][10] = {
    // clang-format off
  // i8   i16     i32     i64     u8     u16     u32     u64     f32     f64
  {NULL,  NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64}, // i8
  {i32i8, NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64}, // i16
  {i32i8, i32i16, NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64}, // i32
  {i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   i64f32, i64f64}, // i64

  {i32i8, NULL,   NULL,   i32i64, NULL,  NULL,   NULL,   i32i64, i32f32, i32f64}, // u8
  {i32i8, i32i16, NULL,   i32i64, i32u8, NULL,   NULL,   i32i64, i32f32, i32f64}, // u16
  {i32i8, i32i16, NULL,   u32i64, i32u8, i32u16, NULL,   u32i64, u32f32, u32f64}, // u32
  {i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   u64f32, u64f64}, // u64

  {f32i8, f32i16, f32i32, f32i64, f32u8, f32u16, f32u32, f32u64, NULL,   f32f64}, // f32
  {f64i8, f64i16, f64i32, f64i64, f64u8, f64u16, f64u32, f64u64, f64f32, NULL},   // f64
    // clang-format on
};

static int getTypeId(Type *ty);
static void cast(Type *from, Type *to);
static void cmpZero(Type *ty);
static void genAddr(Node *node);
static void genExpr(Node *node);
static void genStmt(Node *node);
static void load(Type *ty);
static void pop(const char *arg);
static void popf(size_t i);
static void println(const char *fmt, ...);
static void push(void);
static void pushArgs(Node *args);
static void pushf(void);
static void store(Type *ty);
static void storeFp(size_t r, size_t offset, size_t sz);
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

    if (fn->va_area) {
      size_t gp = 0;
      size_t fp = 0;
      for (Obj *var = fn->params; var; var = var->next) {
        if (isFloat(var->ty)) {
          fp++;
        } else {
          gp++;
        }
      }
      size_t offset = fn->va_area->offset;
      println("  mov DWORD PTR [rbp-%zu], %zu", offset, gp * 8);
      println("  mov DWORD PTR [rbp-%zu], %zu", offset - 4, fp * 8 + 48);
      println("  mov QWORD PTR [rbp-%zu], rbp", offset - 16);
      println("  sub QWORD PTR [rbp-%zu], %zu", offset - 16, offset - 24);
      println("  mov QWORD PTR [rbp-%zu], rdi", offset - 24);
      println("  mov QWORD PTR [rbp-%zu], rsi", offset - 32);
      println("  mov QWORD PTR [rbp-%zu], rdx", offset - 40);
      println("  mov QWORD PTR [rbp-%zu], rcx", offset - 48);
      println("  mov QWORD PTR [rbp-%zu], r8", offset - 56);
      println("  mov QWORD PTR [rbp-%zu], r9", offset - 64);
      println("  movsd [rbp-%zu], xmm0", offset - 72);
      println("  movsd [rbp-%zu], xmm1", offset - 80);
      println("  movsd [rbp-%zu], xmm2", offset - 88);
      println("  movsd [rbp-%zu], xmm3", offset - 96);
      println("  movsd [rbp-%zu], xmm4", offset - 104);
      println("  movsd [rbp-%zu], xmm5", offset - 112);
      println("  movsd [rbp-%zu], xmm6", offset - 120);
      println("  movsd [rbp-%zu], xmm7", offset - 128);
    }

    size_t gp = 0;
    size_t fp = 0;
    for (Obj *param = fn->params; param; param = param->next) {
      if (isFloat(param->ty)) {
        storeFp(fn->param_cnt - (fp++) - 1, param->offset, param->ty->size);
      } else {
        storeArgReg(fn->param_cnt - (gp++) - 1, param->offset, param->ty->size);
      }
    }

    genStmt(fn->body);

    println(".L.return.%s:", fn->name);
    println("  mov rsp, rbp");
    println("  pop rbp");
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
    cmpZero(node->cond->ty);
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
      cmpZero(node->cond->ty);
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
      cmpZero(node->cond->ty);
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
    cmpZero(node->cond->ty);
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
  case ND_NUM: {
    union {
      float f32;
      double f64;
      uint32_t u32;
      uint64_t u64;
    } u;
    switch (node->ty->kind) {
    case TY_FLOAT:
      u.f32 = (float)node->fval;
      println("  mov eax, %u", u.u32);
      println("  movq xmm0, rax");
      return;
    case TY_DOUBLE:
      u.f64 = node->fval;
      println("  mov rax, %lu", u.u64);
      println("  movq xmm0, rax");
      return;
    default:
      println("  mov rax, %ld", node->val);
      return;
    }
  }
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
    cmpZero(node->cond->ty);
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
    cmpZero(node->lhs->ty);
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
    cmpZero(node->lhs->ty);
    println("  je .L.false.%zu", c);
    genExpr(node->rhs);
    cmpZero(node->rhs->ty);
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
    cmpZero(node->lhs->ty);
    println("  jne .L.true.%zu", c);
    genExpr(node->rhs);
    cmpZero(node->rhs->ty);
    println("  jne .L.true.%zu", c);
    println("  mov rax, 0");
    println("  jmp .L.end.%zu", c);
    println(".L.true.%zu:", c);
    println("  mov rax, 1");
    println(".L.end.%zu:", c);
    return;
  }
  case ND_FUNCCALL: {
    pushArgs(node->args);
    size_t gp = 0;
    size_t fp = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      if (isFloat(arg->ty)) {
        popf(fp++);
      } else {
        pop(argreg64[gp++]);
      }
    }
    if (stack_depth % 2 == 0) {
      println("  call %s", node->funcname);
    } else {
      println("  sub rsp, 8");
      println("  call %s", node->funcname);
      println("  add rsp, 8");
    }
    switch (node->ty->kind) {
    case TY_BOOL:
      println("  movzx eax, al");
      break;
    case TY_CHAR:
      if (node->ty->is_unsigned) {
        println("  movzx eax, al");
      } else {
        println("  movsx eax, al");
      }
      break;
    case TY_SHORT:
      if (node->ty->is_unsigned) {
        println("  movzx eax, ax");
      } else {
        println("  movsx eax, ax");
      }
      break;
    default:
      break;
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

  if (isFloat(node->lhs->ty)) {
    genExpr(node->rhs);
    pushf();
    genExpr(node->lhs);
    popf(1);

    const char *sz = (node->lhs->ty->kind == TY_FLOAT) ? "ss" : "sd";

    switch (node->kind) {
    case ND_ADD:
      println("  add%s xmm0, xmm1", sz);
      return;
    case ND_SUB:
      println("  sub%s xmm0, xmm1", sz);
      return;
    case ND_MUL:
      println("  mul%s xmm0, xmm1", sz);
      return;
    case ND_DIV:
      println("  div%s xmm0, xmm1", sz);
      return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
      println("  ucomi%s xmm1, xmm0", sz);
      if (node->kind == ND_EQ) {
        println("  sete al");
        println("  setnp dl");
        println("  and al, dl");
      } else if (node->kind == ND_NE) {
        println("  setne al");
        println("  setp dl");
        println("  or al, dl");
      } else if (node->kind == ND_LT) {
        println("  seta al");
      } else {
        println("  setae al");
      }
      println("  and al, 1");
      println("  movzx rax, al");
      return;
    default:
      break;
    }

    compErrorToken(node->tok->str, "invalid expression");
  }

  genExpr(node->rhs);
  push();
  genExpr(node->lhs);
  pop("rdi");

  char *ax = NULL;
  char *di = NULL;
  char *dx = NULL;

  if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base) {
    ax = "rax";
    di = "rdi";
    dx = "rdx";
  } else {
    ax = "eax";
    di = "edi";
    dx = "edx";
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
    if (node->ty->is_unsigned) {
      println("  mov %s, 0", dx);
      println("  div %s", di);
    } else {
      if (node->lhs->ty->size == 8) {
        println("  cqo");
      } else {
        println("  cdq");
      }
      println("  idiv %s", di);
    }
    return;
  case ND_MOD:
    if (node->ty->is_unsigned) {
      println("  mov %s, 0", dx);
      println("  div %s", di);
    } else {
      if (node->lhs->ty->size == 8) {
        println("  cqo");
      } else {
        println("  cdq");
      }
      println("  idiv %s", di);
    }
    println("  mov rax, rdx");
    return;
  case ND_BITAND:
    println("  and %s, %s", ax, di);
    return;
  case ND_BITOR:
    println("  or %s, %s", ax, di);
    return;
  case ND_BITXOR:
    println("  xor %s, %s", ax, di);
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
    if (node->lhs->ty->is_unsigned) {
      println("  setb al");
    } else {
      println("  setl al");
    }
    println("  movzx %s, al", ax);
    return;
  case ND_LE:
    println("  cmp %s, %s", ax, di);
    if (node->lhs->ty->is_unsigned) {
      println("  setbe al");
    } else {
      println("  setle al");
    }
    println("  movzx %s, al", ax);
    return;
  case ND_SHL:
    println("  mov rcx, rdi");
    println("  shl %s, cl", ax);
    return;
  case ND_SHR:
    println("  mov rcx, rdi");
    if (node->lhs->ty->is_unsigned) {
      println("  shr %s, cl", ax);
    } else {
      println("  sar %s, cl", ax);
    }
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
  switch (ty->kind) {
  case TY_STRUCT:
  case TY_UNION:
    for (ssize_t i = 0; i < ty->size; i++) {
      println("  mov r8b, [rax+%zu]", i);
      println("  mov [rdi+%zu], r8b", i);
    }
    return;
  case TY_FLOAT:
    println("  movss [rdi], xmm0");
    return;
  case TY_DOUBLE:
    println("  movsd [rdi], xmm0");
    return;
  default:
    break;
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
  switch (ty->kind) {
  case TY_ARR:
  case TY_STRUCT:
  case TY_UNION:
    return;
  case TY_FLOAT:
    println("  movss xmm0, [rax]");
    return;
  case TY_DOUBLE:
    println("  movsd xmm0, [rax]");
    return;
  default:
    break;
  }
  char *mov_prefix = ty->is_unsigned ? "movz" : "movs";
  if (ty->size == 1) {
    println("  %sx eax, byte ptr [rax]", mov_prefix);
  } else if (ty->size == 2) {
    println("  %sx eax, word ptr [rax]", mov_prefix);
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
    cmpZero(from);
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
    return ty->is_unsigned ? U8 : I8;
  case TY_SHORT:
    return ty->is_unsigned ? U16 : I16;
  case TY_INT:
    return ty->is_unsigned ? U32 : I32;
  case TY_LONG:
    return ty->is_unsigned ? U64 : I64;
  case TY_FLOAT:
    return F32;
  case TY_DOUBLE:
    return F64;
  default:
    break;
  }
  return U64;
}

void cmpZero(Type *ty) {
  switch (ty->kind) {
  case TY_FLOAT:
    println("  xorps xmm1, xmm1");
    println("  ucomiss xmm0, xmm1");
    return;
  case TY_DOUBLE:
    println("  xorpd xmm1, xmm1");
    println("  ucomisd xmm0, xmm1");
    return;
  default:
    break;
  }
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

void popf(size_t i) {
  println("  movsd xmm%zu, [rsp]", i);
  println("  add rsp, 8");
  stack_depth--;
}

void pushf(void) {
  println("  sub rsp, 8");
  println("  movsd [rsp], xmm0");
  stack_depth++;
}

void pushArgs(Node *args) {
  if (args) {
    pushArgs(args->next);
    genExpr(args);
    if (isFloat(args->ty)) {
      pushf();
    } else {
      push();
    }
  }
}

void storeFp(size_t r, size_t offset, size_t sz) {
  switch (sz) {
  case 4:
    println("  movss [rbp-%zu], xmm%zu", offset, r);
    return;
  case 8:
    println("  movsd [rbp-%zu], xmm%zu", offset, r);
    return;
  default:
    break;
  }
  assert(false);
}
