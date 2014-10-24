#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lru_table.h>

static unsigned int collisionCount = 0;
/*
uint32_t hash_Function (LruTable *table, int key);
void update_LRU (LruTable *table, struct __lru *pos, int value);
void add_LRU_node (LruTable *table, struct __node *node, int value);
void del_LRU_node (LruTable *table, struct __lru *pos);
struct __node *find_collision(struct __tableEntry *tentry, int key);
struct __node *add_new_chainEntry(LruTable *table, struct __tableEntry *tentry, int key, int value);
*/
/* 
 * @param: hash_key_size: Number of bits to be used from Hash Value
 * @return: LruTable *:   Pointer to Hash Table just initialized
 *
 * Notes: 
 * This is the contructor for the Lru Hash Table. It initializes
 * all the data structures for the table and returns a pointer to
 * table. Initialized size of table is 2^hash_key_size.
 */
LruTable* LruTable_Create (int hash_key_size) 
{
    LruTable *table = NULL;

    if ( (hash_key_size < 1) || (hash_key_size > LRU_TABLE_HASH_KEY_SIZE_MAX) )
	return NULL;

    table = malloc(sizeof(LruTable));
    if ( !table )
	return NULL;
    
    table->maxSize =  1 << hash_key_size;
    table->keyBits = (uint32_t) hash_key_size;
    table->currentSize = 0;

    table->htArray = malloc(sizeof(struct __tableEntry) * table->maxSize);
    if ( !table->htArray ) {
	free(table);
	return NULL;
    }
    memset(table->htArray, 0, sizeof(struct __tableEntry) * table->maxSize);

    table->lruList = malloc(sizeof(struct __LruMeta));
    if ( !table->lruList ) {
	free(table->htArray);
	free(table);
	return NULL;
    }
    table->lruList->head = NULL;
    table->lruList->tail = NULL;

    return table;
}


/* 
 * @param: table: Pointer to LRU Hash Table
 * @return: void
 *
 * Notes:
 * This is a destructor for LRU Hash Table implementaion.
 * Takes the pointer to table and frees all memory.
 *
 */
void LruTable_Destroy (LruTable *table)
{
    int i = 0;
    struct __lru *current = NULL;
    struct __lru *tmp = NULL;
    struct __node *chainPt = NULL, *freeChain = NULL;

    if ( !table )
	return;

    for ( i = 0; i < table->maxSize; i++ ) {
	    chainPt = table->htArray[i].chain;
	    while ( chainPt ) {
		freeChain = chainPt->next;
		free(chainPt);
		chainPt = freeChain;
	    }
    }

    free(table->htArray);

    if ( table->lruList ) {
	current = table->lruList->head;
	while ( current ) {
	    tmp = current->next;
	    free(current);
	    current = tmp;
	}
    }

    free(table->lruList);
    free(table);
    memset(table, 0, sizeof(LruTable));
    printf(" COLLISIONS IN THIS RUN: %u:\n", collisionCount);
}


/*

 */
void LruTable_Insert (LruTable *table, int key, int value)
{
    uint32_t index;
    struct __node *collisionPos = NULL, *newNode = NULL;

//    printf("%s:%d\n",__func__, __LINE__);
    if ( !table ) 
	return;

    index = hash_Function(table, key);

//    printf("%s:%d\n",__func__, __LINE__);
    if ( table->htArray[index].chain ) { /* Atleast one Entry present */
	collisionPos = find_collision(&(table->htArray[index]), key);

	if ( collisionPos ) { /* Key Hit - just update value/LRU state */
	    collisionPos->data = value;
	    update_LRU(table, collisionPos->lruPos, value);
	}
	if ( !collisionPos )
	    ++collisionCount;
    }

  //  printf("%s:%d\n",__func__, __LINE__);
    /* Key Miss or add a new node in chain for the collision  */
    if ( !collisionPos || !table->htArray[index].chain ) {
	newNode = add_new_chainEntry(table, &(table->htArray[index]), key, value);
 //   	printf("%s:%d\n",__func__, __LINE__);
	if ( !newNode )
	    return;
	add_LRU_node(table, newNode, value);
    	table->currentSize += 1;   		/* Increase the current size of HT */
    }
}


/*
 */
bool LruTable_Lookup (const LruTable *table, int key, int *out_value)
{
    uint32_t index;
    struct __node *position = NULL;

    if ( !table || !out_value )
	return false;

    index = hash_Function((LruTable *) table, key);

    if ( table->htArray[index].chain ) {
	position = find_collision(&(table->htArray[index]), key);
	if ( position ) {
	    *out_value = position->data;
	    update_LRU((LruTable *) table, position->lruPos, position->data);
	    return true;
	}
    }
    return false;
}

/**/
void LruTable_Remove (LruTable *table, int key)
{
    uint32_t index;
    struct __node *position;

    if ( !table ) 
	return;

    index = hash_Function(table, key);

    if ( table->htArray[index].chain ) {
        printf("%s:%d\n",__func__, __LINE__);
	position = find_collision(&(table->htArray[index]), key);
        printf("%s:%d\n",__func__, __LINE__);
	if ( !position ) {
            printf("%s:%d\n",__func__, __LINE__);
	    return;
	}

        printf("%s:%d\n",__func__, __LINE__);
	del_LRU_node(table, position->lruPos);
        printf("%s:%d\n",__func__, __LINE__);
	if ( position == table->htArray[index].chain ) { /* Node is at head of chain */
	    table->htArray[index].chain = position->next;
            printf("%s:%d:%x:\n",__func__, __LINE__, position);
	    if ( position->next )
		position->next->prev = NULL;
            printf("%s:%d\n",__func__, __LINE__);
	}
	else { /* Not at head */
            printf("%s:%d:%x: %x:\n",__func__, __LINE__, position, table->htArray[index].chain);
	    position->prev->next = position->next;
            printf("%s:%d\n",__func__, __LINE__);
	    if ( position->next )
		position->next->prev = position->prev;
	}
	free(position);
	table->currentSize -= 1;
//	memset(&(table->htArray[index]), 0, sizeof(struct __node));
    }
}

/**/
bool LruTable_RemoveOldest (LruTable *table, int *out_value)
{
    struct __node *position = NULL;
    if ( !table || !out_value ) 
	return false;

    if ( table->lruList ) {
	*out_value = table->lruList->head->data;
	position = table->lruList->head->backPt;
        printf("%s:%d\n",__func__, __LINE__);
	LruTable_Remove(table, position->key);  /* Remove entry in HT & LRU list */
        printf("%s:%d\n",__func__, __LINE__);
	return true;
    }
    return false;
}


/* Hash Function takes a key returns an index 
uint32_t hash_Function (LruTable *table, int key)
{
    uint32_t index;
    uint32_t mask = 0xFFFFFFFF;

    index = key & (mask >> (32 - table->keyBits));
    return index;
}
*/
uint32_t hash_Function ( LruTable *table, int key)
{
    uint32_t hash = 0, mask = 0xFFFFFFFF, ret;
    int i;

    for ( i = 0; i < 5; i++ )
	hash ^= ( hash << 5 ) + ( hash >> 2 ) + key;

    ret = hash & (mask >> (32 - table->keyBits));
    return ret;
}


/* Update the value in the LRU list node and put it at the tail of list.
 * Thereby maintaing LRU node at the head of the LRU list.
 */
void update_LRU (LruTable *table, struct __lru *pos, int value)
{
    struct __lru *tmp = pos;

    if ( !table || !pos )
	return;

    pos->data = value;     /* Updating the new value */

    if ( table->lruList->head == table->lruList->tail 
	    || pos == table->lruList->tail )  /* Only node in the LRU list */
	return;

    if ( pos == table->lruList->head ) {
	table->lruList->head = table->lruList->head->next;
	table->lruList->head->prev = NULL;
	table->lruList->tail->next = tmp;
	tmp->prev = table->lruList->tail;
	tmp->next = NULL;
	table->lruList->tail = tmp;
    }
    else {
	tmp->next->prev = tmp->prev;
	tmp->prev->next = tmp->next;
	tmp->next = NULL;
	tmp->prev = table->lruList->tail;
	table->lruList->tail->next = tmp;
	table->lruList->tail = tmp;
    }
}

/**/
void add_LRU_node (LruTable *table, struct __node *node, int value)
{
    struct __lru *newNode = NULL;

    if ( !table || !node )
	return;

    newNode = malloc(sizeof(struct __lru));
    if ( !newNode )
	return;

    newNode->data = value;
    newNode->backPt = node;
    node->lruPos = newNode;  /* Updated the LRU pointer in node in chain */

    /* Check if LRU list is not initilized */
    if ( !table->lruList->head ) {

	newNode->next = NULL;
	newNode->prev = NULL;
	node->lruPos = newNode;
	table->lruList->head = newNode;
	table->lruList->tail = newNode;
    }
    else { 
	table->lruList->tail->next = newNode;
	newNode->prev = table->lruList->tail;
	newNode->next = NULL;
	table->lruList->tail = newNode;
	node->lruPos = newNode;
    }
}

/* Delete a given node from the LRU list */
void del_LRU_node (LruTable *table, struct __lru *pos)
{
    struct __lru *tmp = pos;

    if ( !table || !pos )
	return;

    if ( table->lruList->head == table->lruList->tail 
	    && table->lruList->head == pos ) { /* Only node in the LRU list */
	free(pos);
	table->lruList->head = NULL;
	table->lruList->tail = NULL;
	return;
    }

    if ( table->lruList->tail == pos ) {
	table->lruList->tail = table->lruList->tail->prev;
	table->lruList->tail->next = NULL;
	free(pos);
	return;
    }

    if ( table->lruList->head == pos ) {
	table->lruList->head = table->lruList->head->next;
	table->lruList->head->prev = NULL;
	free(pos);
	return;
    }

    tmp->next->prev = tmp->prev;
    tmp->prev->next = tmp->next;
    free(tmp);
}


/* Check if a key is already present in Chain return pt to it
  else return NULL
 */
struct __node *find_collision(struct __tableEntry *tentry, int key)
{
    struct __node *current = NULL;

    if ( !tentry || !tentry->chain )
	return NULL;

    current = tentry->chain;

    while ( current ) {
	if ( current->key == key )
	    return current;
	current = current->next;
    }
    return NULL;
}

/* Adds a new entry in the chain - always adds at the head */
struct __node *add_new_chainEntry(LruTable *table, struct __tableEntry *tentry, int key, int value)
{
    struct __node *newNode = NULL;

    if ( !tentry || table->maxSize == table->currentSize )
	return NULL;


    newNode = malloc(sizeof(struct __node));
    if ( !newNode )
	return NULL;

    newNode->data = value;
    newNode->key = key;
    newNode->prev = NULL;

    /* lruPos pointer is initialized in add_LRU_node()*/
    newNode->lruPos = NULL; /* till that call make it NULL */
    if ( !tentry->chain ) { /* Create the chain */
	newNode->next = NULL;
	tentry->chain = newNode;
    }
    else { /* Add at the head */
	newNode->next = tentry->chain;
	tentry->chain = newNode;
    }
    return newNode;
}
