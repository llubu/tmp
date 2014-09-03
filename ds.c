#include <stdio.h>
#include <stdlib.h>
#include "ds.h"

Stack *push(Stack **top, Node *node)
{
    Stack *tmp = NULL;

    tmp = malloc(sizeof(Stack));
    if ( !tmp )
	return NULL;

    tmp->ptr = node;
    tmp->next = *top;
    *top = tmp;
    return tmp;
}

Node *pop(Stack **top)
{
    Stack *tmp = *top;
    Node *pt = NULL;

    if ( !tmp )
	return NULL;

    pt = (Node *) tmp->ptr;
    *top = tmp->next;
    free(tmp);
    return pt;
}

/* Do not pop top.. just returns a pointer to the top node*/
Node *seek_stack(Stack **top)
{
    Stack *tmp =  *top;
    Node *pt = NULL;

    if ( !tmp )
	return NULL;

    pt = (Node *) tmp->ptr;
    return pt;
}
