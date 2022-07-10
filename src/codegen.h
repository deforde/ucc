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

#endif //CODEGEN_H
