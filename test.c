#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    struct node *next;
    int data;
} Node;

Node *push(Node **top, int data)
{
    Node *tmp = NULL;

    if ( NULL == (tmp = malloc(sizeof(Node))) )
	return NULL;

    tmp->data = data;
    tmp->next = *top;
    *top = tmp;

    return *top;
}


int pop(Node **top)
{
    Node *tmp = *top;
    int data;
    
    if ( !tmp )
	return -1;

    tmp = *top;
    data = (*top)->data;
    *top = tmp->next;
    free(tmp);

    return data;
}

int main()
{

    Node * head = NULL;

    push(&head, 4);
    push(&head, 3);
    push(&head, 6);
    push(&head, 9);

    printf("%d\n", pop(&head) );
    printf("%d\n", pop(&head) );
    printf("%d\n", pop(&head) );
    printf("%d\n", pop(&head) );
    printf("%d\n", pop(&head) );

    return 0;
}
