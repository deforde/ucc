#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
};

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
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
bool consume(char op);
void expect(char op);
int expectNumber(void);
bool isEOF(void);
Token* newToken(TokenType type, Token* cur, const char* str);
Token* tokenise(const char* p);
Node* newNode(NodeType type, Node* lhs, Node* rhs);
Node* newNodeNum(int val);
Node* primary(void);
Node* mul(void);
Node* expr(void);
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

bool consume(char op)
{
    if(token->type != TK_RESERVED || token->str[0] != op) {
        return false;
    }
    token = token->next;
    return true;
}

void expect(char op)
{
    if(token->type != TK_RESERVED || token->str[0] != op) {
        error(token->str, "Unexpected token: '%c'", op);
    }
    token = token->next;
}

int expectNumber(void)
{
    if(token->type != TK_NUM) {
        error(token->str, "Expected token type: %i (TK_NUM), got: %i", TK_NUM, token->type);
    }
    const int val = token->val;
    token = token->next;
    return val;
}

bool isEOF(void)
{
    return token->type == TK_EOF;
}

Token* newToken(TokenType type, Token* cur, const char* str)
{
    Token* tok = calloc(1, sizeof(Token));
    tok->type = type;
    tok->str = str;
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
        if(*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')') {
            cur = newToken(TK_RESERVED, cur, p++);
            continue;
        }
        if(isdigit(*p)) {
            cur = newToken(TK_NUM, cur, p);
            cur->val = (int)strtol(p, (char**)&p, 10);
            continue;
        }
        error(token->str, "Unrecognised token: '%c'", *p);
    }
    newToken(TK_EOF, cur, p);
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
    if(consume('(')) {
        Node* node = expr();
        expect(')');
        return node;
    }
    return newNodeNum(expectNumber());
}

Node* mul(void)
{
    Node* node = unary();
    for(;;) {
        if(consume('*')) {
            node = newNode(ND_MUL, node, unary());
        }
        else if (consume('/')) {
            node = newNode(ND_DIV, node, unary());
        }
        else {
            return node;
        }
    }
}

Node* expr(void)
{
    Node* node = mul();
    for(;;) {
        if(consume('+')) {
            node = newNode(ND_ADD, node, mul());
        }
        else if(consume('-')) {
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
        default:
            error(token->str, "Unexpected node type: %i", node->type);
    }
    puts("  push rax");
}

Node* unary(void)
{
    if(consume('+')) {
        return primary();
    }
    if(consume('-')) {
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

