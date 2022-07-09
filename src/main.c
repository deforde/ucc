#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TK_RESERVED,
    TK_NUM,
    TK_EOF,
} TokenType;

typedef struct Token Token;
struct Token {
    TokenType type;
    Token* next;
    int val;
    const char* str;
    size_t len;
};

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
} NodeType;

typedef struct Node Node;
struct Node {
    NodeType type;
    Node* lhs;
    Node* rhs;
    int val;
};

Token* token = NULL;
const char* input = NULL;

void error(const char* loc, const char* fmt, ...);
bool startsWith(const char* p, const char* q);
bool consume(char* op);
void expect(char* op);
int expectNumber(void);
bool isEOF(void);
Token* newToken(TokenType type, Token* cur, const char* str, size_t len);
Token* tokenise(const char* p);
Node* newNode(NodeType type, Node* lhs, Node* rhs);
Node* newNodeNum(int val);
Node* expr(void);
Node* equality(void);
Node* relational(void);
Node* add(void);
Node* primary(void);
Node* mul(void);
Node* unary(void);
void gen(Node* node);

void error(const char* loc, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int pos = (int)(loc - input);
    fprintf(stderr, "%s\n", input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

bool startsWith(const char* p, const char* q)
{
    return memcmp(p, q, strlen(q)) == 0;
}

bool consume(char* op)
{
    if(token->type != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len) != 0) {
        return false;
    }
    token = token->next;
    return true;
}

void expect(char* op)
{
    if(token->type != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len) != 0) {
        error(token->str, "Expected: '%c'", *op);
    }
    token = token->next;
}

int expectNumber(void)
{
    if(token->type != TK_NUM) {
        error(token->str, "Expected number");
    }
    const int val = token->val;
    token = token->next;
    return val;
}

bool isEOF(void)
{
    return token->type == TK_EOF;
}

Token* newToken(TokenType type, Token* cur, const char* str, size_t len)
{
    Token* tok = calloc(1, sizeof(Token));
    tok->type = type;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

Token* tokenise(const char* p)
{
    Token head = {0};
    Token* cur = &head;
    while(*p) {
        if(isspace(*p)) {
            ++p;
            continue;
        }
        if(startsWith(p, "==") ||
           startsWith(p, "!=") ||
           startsWith(p, "<=") ||
           startsWith(p, ">=")) {
            cur = newToken(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }
        if(strchr("+-*/()<>", *p)) {
            cur = newToken(TK_RESERVED, cur, p++, 1);
            continue;
        }
        if(isdigit(*p)) {
            cur = newToken(TK_NUM, cur, p, 0);
            const char* q = p;
            cur->val = (int)strtol(p, (char**)&p, 10);
            cur->len = p - q;
            continue;
        }
        error(p, "Invalid token");
    }
    newToken(TK_EOF, cur, p, 0);
    return head.next;
}

Node* newNode(NodeType type, Node* lhs, Node* rhs)
{
    Node* node = calloc(1, sizeof(Node));
    node->type = type;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node* newNodeNum(int val)
{
    Node* node = calloc(1, sizeof(Node));
    node->type = ND_NUM;
    node->val = val;
    return node;
}

Node* primary(void)
{
    if(consume("(")) {
        Node* node = expr();
        expect(")");
        return node;
    }
    return newNodeNum(expectNumber());
}

Node* mul(void)
{
    Node* node = unary();
    for(;;) {
        if(consume("*")) {
            node = newNode(ND_MUL, node, unary());
        }
        else if (consume("/")) {
            node = newNode(ND_DIV, node, unary());
        }
        else {
            return node;
        }
    }
}

Node* expr(void)
{
    return equality();
}

Node* equality(void)
{
    Node* node = relational();
    for(;;) {
        if(consume("==")) {
            node = newNode(ND_EQ, node, relational());
        }
        else if (consume("!=")) {
            node = newNode(ND_NE, node, relational());
        }
        else {
            return node;
        }
    }
}

Node* relational(void)
{
    Node* node = add();
    for(;;) {
        if(consume("<")) {
            node = newNode(ND_LT, node, add());
        }
        else if(consume("<=")) {
            node = newNode(ND_LE, node, add());
        }
        else if(consume(">")) {
            node = newNode(ND_LT, add(), node);
        }
        else if(consume(">=")) {
            node = newNode(ND_LE, add(), node);
        }
        else {
            return node;
        }
    }
}

Node* add(void)
{
    Node* node = mul();
    for(;;) {
        if(consume("+")) {
            node = newNode(ND_ADD, node, mul());
        }
        else if(consume("-")) {
            node = newNode(ND_SUB, node, mul());
        }
        else {
            return node;
        }
    }
}

void gen(Node* node)
{
    if(node->type == ND_NUM) {
        printf("  push %d\n", node->val);
        return;
    }
    gen(node->lhs);
    gen(node->rhs);
    puts("  pop rdi");
    puts("  pop rax");
    switch(node->type) {
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
            error(token->str, "Unexpected node type: %i", node->type);
    }
    puts("  push rax");
}

Node* unary(void)
{
    if(consume("+")) {
        return primary();
    }
    if(consume("-")) {
        return newNode(ND_SUB, newNodeNum(0), primary());
    }
    return primary();
}

int main(int argc, char* argv[])
{
    if(argc != 2) {
        error(token->str, "Error! Exactly 1 command line argument should be provided");
    }

    input = argv[1];
    token = tokenise(input);
    Node* node = expr();

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    gen(node);

    puts("  pop rax");
    puts("  ret");
    return EXIT_SUCCESS;
}

