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

Trie *trie_newNode(void)
{
    Trie *tmp = NULL;
    int i = 0;

    tmp = malloc(sizeof(Trie));
    if ( !tmp )
	return NULL;
    else {
	for ( i = 0; i<26; i++ )
	    tmp->elm[i] = NULL;
	tmp->end = 0;
    }

    return tmp;
}

/* Lower case english alphabets */
void trie_insert(Trie *root, char *arr)
{
    char *tmp = arr;
    Trie *cur = root;
    int index;

    int len = 0, i = 0;

    if ( !arr )
	return;

    while( *tmp != '\0' ) {
	++len;
	++tmp;
    }
    tmp = arr;

    for ( i = 0; i<len; i++ )
    {
	index = tmp[i] - 'a';

	if ( !cur->elm[index] ) 
	    cur->elm[index] = trie_newNode();

	cur = cur->elm[index];
    }
    cur->end = 1;
}

/* Return 0 if not found, 1 if found, -1 if error */
int trie_search(Trie *root, char *arr)
{
    Trie *cur = root;
    char *tmp = arr;
    int i = 0, len = 0, index;

    if ( !cur || !arr )
	return -1;

    while( *tmp != '\0' ) {
	++len;
	++tmp;
    }

    tmp = arr;
    for ( i = 0; i<len; i++ ) 
    {
	index = tmp[i] - 'a';
	if ( !cur->elm[index] )
	    return 0;
	cur = cur->elm[index];
    }

    if ( cur->end )
	return 1;
}
