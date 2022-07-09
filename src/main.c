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

Token* token = NULL;
const char* input = NULL;

void error(const char* loc, const char* fmt, ...);
bool consume(char op);
void expect(char op);
int expectNumber();
bool isEOF();
Token* newToken(TokenType type, Token* cur, const char* str);
Token* tokenise(const char* p);

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

int expectNumber()
{
    if(token->type != TK_NUM) {
        error(token->str, "Expected token type: %i (TK_NUM), got: %i", TK_NUM, token->type);
    }
    const int val = token->val;
    token = token->next;
    return val;
}

bool isEOF()
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
        if(*p == '+' || *p == '-') {
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

int main(int argc, char* argv[])
{
    if(argc != 2) {
        error(token->str, "Error! Exactly 1 command line argument should be provided");
    }

    input = argv[1];
    token = tokenise(input);

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    printf("  mov rax, %d\n", expectNumber());

    while(!isEOF()) {
        if(consume('+')) {
            printf("  add rax, %d\n", expectNumber());
            continue;
        }
        expect('-');
        printf("  sub rax, %d\n", expectNumber());
    }

    puts("  ret");
    return EXIT_SUCCESS;
}

