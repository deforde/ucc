#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include <stddef.h>

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

bool consume(char* op);
void expect(char* op);
int expectNumber(void);
Token* tokenise(const char* p);

#endif //PARSE_H
