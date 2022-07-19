#ifndef PARSE_H
#define PARSE_H

typedef struct Node Node;

Node *expr(void);
void gen(Node *node);
void program(void);

#endif // PARSE_H
