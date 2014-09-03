#ifndef _DS_H
#define _DS_H

typedef struct stack {
    struct stack *next;
    void *ptr;
} l_node;

typedef struct node {
    struct node *left;
    struct node *right;
    int data;
} Node;


typedef l_node Stack;
typedef l_node Que;

Stack *push(Stack **top, Node *node);
Node *pop(Stack **top);
Node *seek_stack(Stack **top);




#endif
