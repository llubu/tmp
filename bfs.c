#include <stdio.h>
#include <stdlib.h>
#include "ds.h"

#define seek_que(head) seek_stack(head)

void dfs(Node *root) 
{
    Node *tmp = NULL;
    Que *head = NULL, *tail = NULL;

    if ( !root )
	return;

    enque(&tail, root);
    head = tail;
    
    while ( head ) {
	tmp = deque(&head);
	if ( tmp->right )
	    enque(&tail, tmp->right);
	if ( tmp->left )
	    enque(&tail, tmp->left);

	





}

