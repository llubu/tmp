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


typedef struct trie {
    struct trie *elm[26];
    int end;
} Trie;

typedef l_node Stack;
typedef l_node Que;

Stack *push(Stack **top, Node *node);
Node *pop(Stack **top);
Node *seek_stack(Stack **top);

void trie_insert(Trie *root, char *arr);
int trie_search(Trie *root, char *arr);




#endif
