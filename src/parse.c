#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "parse.h"

extern Token* token;

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
