#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ds.h"

int main()
{
    Trie *root = NULL;
    int i;
    char keys[][8] = {"she", "sells", "sea", "shore", "the", "by", "sheer"};
    char *ck = "abhirooop";

    root = malloc(sizeof(Trie));
    if ( !root )
	return 1;

    for (i = 0; i<26; i++ )
    {
	root->elm[i] = NULL;
    }
    root->end = 1; /* End of NULL string */

    for ( i = 0; i<7; i++ )
    {
	trie_insert(root, keys[i]);
    }
    
    if ( trie_search(root, keys[0]) )
	printf("Key %s is present\n", keys[0] );
    else
	printf("Key %s is NOT present\n", keys[0] );

    if ( trie_search(root, ck) )
	printf("Key %s is present\n", ck );
    else
	printf("Key %s is NOT present\n", ck );

    i = 0;
    i = trie_del(root, keys[0], strlen(keys[0]), 0);
    printf("%d\n", i);

    if ( trie_search(root, keys[0]) )
	printf("Key %s is present\n", keys[0] );
    else
	printf("Key %s is NOT present\n", keys[0] );

   if ( trie_search(root, keys[6]) )
	printf("Key %s is present\n", keys[6] );
    else
	printf("Key %s is NOT present\n", keys[6] );

    return 0;
}




   




