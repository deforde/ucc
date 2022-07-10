#ifndef CODEGEN_H
#define CODEGEN_H

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

Node* expr(void);
void gen(Node* node);

#endif //CODEGEN_H
