#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "err.h"
#include "parse.h"

extern Token* token;

Node* newNode(NodeType type, Node* lhs, Node* rhs);
Node* newNodeNum(int val);
Node* equality(void);
Node* relational(void);
Node* add(void);
Node* primary(void);
Node* mul(void);
Node* unary(void);

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
