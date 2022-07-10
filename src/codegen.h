#ifndef CODEGEN_H
#define CODEGEN_H

typedef struct Node Node;

Node* expr(void);
void gen(Node* node);

#endif //CODEGEN_H
