/**
 * This file contains the implementaion for the functions declared in file_cache.h 
 * @Abhiroop Dabral - adabral@cs.stonybrook.edu
 **/

/* Design of User level File Cache:
 *
 * This file cahe is implemented at User level and each process calling the constructor
 * will have its own instance of file_cache. Though within a process all threads shares
 * the same file_cache. 
 *
 * The constructor initializes a structure file_cache which has all the meta data for the 
 * file cache. This consructor hold a poniter to memory on heap of type struct __node_cache
 * which is an array of structures of size max_cache_entries. Each structure in this array 
 * contains a char * pointer to the actual cache of size 10Kb allocated on heap along with
 * house keeping variables such as refrenceCount and dirty which keeps track of no of threads
 * which have 'pined' the cache in the memory and if cache is written to since it was read into 
 * memory and a char *name poninter which points to the name of the file cached into that cache node.
 * 
 * Diagram below shows only important structure members in file cache. See declaration for details in file_cache.h
 *
 * struct file_cache
 * _________________ 
 * |int maxSize     |  array of structure type (struct __node_cache) size max_cache_entries
 * |int currentSize |      |--------------------|
 * |*nodeHead	    |----> |int refCount        |
 * |func ptrs ...___|      |char dirty          |
 *			   |char *name          |---->[pointer to heap mem of size of file name char string]
 *                         |char *cache         |---->[poniter to heap mem of size 10Kb]
 *			   |____________________|
 *
 */                            

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include "file_cache.h"

#define CACHE_SIZE 10240     /* 10 Kb = 10*1024 Bytes */

pthread_mutex_t metaLock;    /* Mutex used in constructor to facilitate singelton instance */
pthread_mutex_t pinLock;     /* Mutex used in pin & unpin to searially access file_cache DS */
pthread_cond_t slotcv;	     /* Condition variable to signal if cache has empty slot */

/* @param: int max_cache_entries: Maximum entries in the file cache.
 * @ret: file_cache* poniter to file_cache structure. 
 * Notes:
 * This the constructor for the user level file cache. 
 * It initializes the file_cache structure which contains the metadata for the file cache.
 * There will be only one instance of the file_cache at a time in process address space.
 * Client are expected to use the function pointers perform operation on the file cache.
 */
file_cache *file_cache_construct(int max_cache_entries)
{
    static file_cache *fileCachePt = NULL;

    pthread_mutex_init (&metaLock, NULL);
    pthread_mutex_lock(&metaLock);

    if ( !fileCachePt ) {
	fileCachePt = malloc(sizeof(struct file_cache));
	if ( !fileCachePt )
	    return NULL;

	memset(fileCachePt, 0, sizeof(struct file_cache));
	fileCachePt->maxSize = max_cache_entries;
	fileCachePt->currentSize = 0;

	fileCachePt->nodeHead = malloc(max_cache_entries *(sizeof(struct __node_cache)));
	if ( !fileCachePt->nodeHead ) {
	    free(fileCachePt);
	    fileCachePt = NULL;
    	    pthread_mutex_unlock(&metaLock);
	    return NULL;
	}

	memset(fileCachePt->nodeHead,  0, max_cache_entries *(sizeof(struct __node_cache)));
	fileCachePt->file_cache_destroy = file_cache_destroy; 
	fileCachePt->file_cache_pin_files = file_cache_pin_files;
	fileCachePt->file_cache_unpin_files = file_cache_unpin_files;
	fileCachePt->file_cache_file_data = file_cache_file_data;
	fileCachePt->file_cache_mutable_file_data = file_cache_mutable_file_data;
    }
    pthread_mutex_init(&pinLock, NULL);
    pthread_cond_init (&slotcv, NULL);

    pthread_mutex_unlock(&metaLock);
    return fileCachePt;
}
/* @param: file_cache* cache: pointer to file cache structure.
 * @ret: void
 * Notes: 
 *   This is the destructor for the file_cache and frees all allocated
 * memory on heap starting from the bottom i.e. 10Kb cache pages, then
 * name then the array of structure (struct __node_cache) and finally 
 * file_cache structure itself.
 * Also desstroys the mutex and condition variables initialized.
 *
 */

void file_cache_destroy(file_cache *cache)
{
    int size, i;
    if ( !cache )
	return;

    pthread_mutex_lock(&metaLock);

    size = cache->maxSize;
    i = 0;

    while ( i < size ) {  /* Free the cache and name in each nodeCache */
	free(cache->nodeHead[i].name);
	free(cache->nodeHead[i].cache);
	i++;
    }

    /* Now free nodeCache itself */
    free(cache->nodeHead);

    /* Just to make sure that all dangling refrences will seg fault after this */
    memset(cache, 0, sizeof(struct file_cache));

    /* Now free file_cache structure */
    free(cache);

    pthread_mutex_unlock(&metaLock);
    pthread_mutex_destroy(&metaLock);
    pthread_mutex_destroy(&pinLock);
    pthread_cond_destroy(&slotcv);
}

/* @param:
 *  *cache: poniter to file_cache structure (meta data)
 *  **files: poniter to array of char strings containing names of files to be pinned.
 *  num_files: Number of files to be pinned.
 * @ret: void
 *
 * Notes:
 * This function tries to pin(read) files into file_cache.
 * If there is a cache hit i.e. file is already pinned then we just increase the refCount of the file
 * If there is a cache miss i.e. file is not in cache, there are two scenarios:
 *
 *     a. File is present on the secondary storage, it is read into an empty cache slot and corresponding refcount
 *	  and currentSize of cache are incremented.
 *     b. If file in not present on secondary storage a new file is created of size 10Kb and written with '\0'.
 *        In this case it is *NOT* read into cache.
 *
 * If there are no empty slots in the file_cache i.e. maxSize == currentSize in the case of a cache miss,
 * the thread blocks on a condition variable 'slotcv' which is signaled from file_cache_unpin_files() 
 * if a slot is unpined and is ready to be used.
 * At a time only one thread can enter the critical section in this function and can therby modify the file_cache DS.
 *
 */

void file_cache_pin_files(file_cache *cache, const char **files, int num_files)
{
    const char *fName = NULL;
    int i, j, nameLen, hit, freeIndex, ret;
    FILE *filePt;

    if ( !cache || !files || 0 == num_files )
	return;

    pthread_mutex_lock(&pinLock);                 /* take the lock before modifying file_cache */

    for ( i = 0; i < num_files; i++ ) {
	fName = files[i];
	hit = 0;

	for ( j = 0; j < cache->currentSize; j++ ) {

	    if ( 0 == cache->nodeHead[j].refCount )	/* Empty slot we don't need to check this. */
		continue;

	    if ( 0 == strcmp(fName, cache->nodeHead[j].name) ) { /* Cache Hit */ 
		cache->nodeHead[j].refCount++;			
		hit = 1;
		break;
	    }
	}
	if ( 0 == hit ) { /* Cache Miss */
	    while ( cache->currentSize == cache->maxSize ) /* Cache is full, wait for a slot to open */
		pthread_cond_wait(&slotcv, &pinLock);

	    /* Get a free index in the file_cache */
	    for ( freeIndex = 0; freeIndex < cache->maxSize; freeIndex++ ) {
		if ( 0 == cache->nodeHead[freeIndex].refCount )
		    break;
	    }
	    
	    nameLen = strlen(fName) + 1;
	    filePt = fopen(fName, "r");

	    if ( filePt ) { /* Read and Map in cache-  Case a. as mentioned above in Notes  */

		cache->nodeHead[freeIndex].name = malloc(sizeof(char) * nameLen);
		if ( !(cache->nodeHead[freeIndex].name) ) { /* Cant allocate memory, error out */
		    fclose(filePt);
    		    pthread_mutex_unlock(&pinLock);
		    return;
		}
    		memset(cache->nodeHead[freeIndex].name, 0, nameLen);
		strncpy(cache->nodeHead[freeIndex].name, fName, nameLen-1);

		cache->nodeHead[freeIndex].cache = malloc(CACHE_SIZE); /* allocate memory of actual file cache */
		if ( !(cache->nodeHead[freeIndex].cache) ) {/* Cant allocate memory, error out */
		    free(cache->nodeHead[freeIndex].name);
		    fclose(filePt);
    		    pthread_mutex_unlock(&pinLock);
		    return;
		}
    		memset(cache->nodeHead[freeIndex].cache, 0, CACHE_SIZE);
		fread(cache->nodeHead[freeIndex].cache, 1, CACHE_SIZE, filePt); /* Read file into the cache */
		fclose(filePt);
		cache->nodeHead[freeIndex].refCount += 1;
		cache->currentSize += 1;
	    }
	    else { /* File not present. Create on Disk with 10 Kb '\0' */

		filePt = fopen(fName, "w+");
		if ( !filePt ) {
  		    pthread_mutex_unlock(&pinLock);
		    return;
		}
		fclose(filePt); 
		ret = truncate(fName, CACHE_SIZE);
		printf(" RET FROM TRUCN:%d\n", ret); // ABHI
	    }

	}
    }
    pthread_mutex_unlock(&pinLock); /* release the lock before returning */
}

/* 
 * @param: *cache: poniter to file_cache structure (meta data)
 *   **file: poniter to char strings containing names of files to be UNpinned.
 *   num_files: Number of files to be UNpinned.
 * @ret: void
 *
 * Notes:
 * This function UNpins the files if already pinned in the cache and release the memory in cache.
 * if there is a cache hit i.e. file is there in cache, there are two scenarios:
 *  1. refCount of the cache is 1:
 *      - Flush the cache into the file if present on disk and release memory else dont do anything.
 *  2. If refCount is greater then 1, just decrement the refCount by 1 and return.
 *  After releasing the memory check if currentSize == maxSize, signal the process waiting for a free slot.
 *  Decrement the currentSize before signaling.
 *
 */

void file_cache_unpin_files(file_cache *cache, const char **files, int num_files)
{
    const char *fName = NULL;
    int i, j, loc;
    FILE *filePt;

    if ( !cache || !files || 0 == num_files )
	return;

    pthread_mutex_lock(&pinLock);

    for ( i = 0; i < num_files; i++ ) {
	fName = files[i];

	for ( j = 0; j < cache->currentSize; j++ ) {

	    if ( 0 == cache->nodeHead[j].refCount )
		continue;

	    if ( 0 == strcmp(fName, cache->nodeHead[j].name) ) { /* Cache Hit */
		loc = cache->nodeHead[j].refCount - 1; 
		if ( 0 == loc ) { /* Can free the cach node */
		    if ( cache->nodeHead[j].dirty ) { /* Flush back to Disk */ 
			filePt = fopen(fName, "w");
			if ( !filePt ) {  /* Can't Open file to write to,error out without modifying any metadata */
			    pthread_mutex_unlock(&pinLock);
			    return;
			}
			fwrite(cache->nodeHead[j].cache, 1, CACHE_SIZE, filePt);
			fclose(filePt);
		    }
		    free(cache->nodeHead[j].cache);
		    free(cache->nodeHead[j].name);
		    memset(&(cache->nodeHead[j]), 0, sizeof(struct __node_cache));

		    if ( cache->currentSize == cache->maxSize ) {
			printf("SIZE ARE SAME:%d:%d:\n", cache->currentSize, cache->maxSize);
			cache->currentSize -= 1;      /* Decrease the currentSize of file_cache */
			pthread_cond_signal(&slotcv); /* Signal to any thread blocking for a free slot */
		    }
		    else
			cache->currentSize -= 1;      /* Decrease the currentSize of file_cache */
		}
		else /* else just decrease the refcount */
		    cache->nodeHead[j].refCount -= 1;
	    }
	}
    }
    pthread_mutex_unlock(&pinLock);
}
/* 
 * @param: *cache: pointer to file_cache structure (meta data).
 *    *file: const char * pointer to file name to read from cache.
 * @ret: const char * pointer to 10Kb in memory cache else NULL if not present.
 *
 * Notes:
 * This functions returnes a const char pointer to the 10Kb cache to the client if present in cache.
 * It is the responsibility of the client to synchronize the reads and writes to the file cache.
 * 
 */

const char *file_cache_file_data(file_cache *cache, const char *file)
{
    char *ret_val = NULL;
    int i;

    if ( !cache || !file )
	return;

    for ( i = 0; i < cache->currentSize; i++ ) {

	if ( 0 == cache->nodeHead[i].refCount )
	    continue;
	if ( 0 == strcmp(file, cache->nodeHead[i].name) ) {
		ret_val = cache->nodeHead[i].cache;
		break;
	}
    }
    return (const char *) ret_val;
}

/* 
 * @param: *cache: pointer to file_cache structure (meta data).
 *    *file: const char * pointer to file name to write to in cache.
 * @ret: const char * pointer to 10Kb in memory cache else NULL if not present.
 *
 * Notes:
 * This functions returnes a const char pointer to the 10Kb cache to the client if present in cache.
 * It is the responsibility of the client to synchronize the reads and writes to the file cache.
 * 
 */



char *file_cache_mutable_file_data(file_cache *cache, const char *file)
{
    char *ret_val = NULL;
    int i;
    
    if ( !cache || !file )
	return NULL;
    
    for ( i = 0; i < cache->currentSize; i++ ) {

	if ( 0 == cache->nodeHead[i].refCount )
	    continue;
	if ( 0 == strcmp(file, cache->nodeHead[i].name) ) {
	    cache->nodeHead[i].dirty = 1;
	    ret_val = cache->nodeHead[i].cache;
	    break;
	}
    }
    return ret_val;
}



void test_fc()
{

}


int main()
{
    const char *rPt;
    char *wPt;
    pthread_t threads[4];
    int i;

    struct file_cache *ch = file_cache_construct(4);
    printf("0x%x\n", (unsigned int ) ch);

    struct payload {
	struct file_cache *c;
	const char **name;
	int num;
    }payload;

    const char *fn [4];
//    fn[0] = "gh.out";
    fn[0] = "bt.c";
    fn[1] = "ad.out";
    fn[2] = "check.out";
    fn[3] = "qw.out";

    payload.c = ch;
    payload.name = &fn;
    payload.num = 4;

    printf("%s:\n", payload.name[0]);
    printf("%s:\n", payload.name[1]);
    printf("%s:\n", payload.name[2]);

    pthread_create(&threads[0], NULL, ch->file_cache_pin_files, (void *) &payload);

    printf("Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);
    printf("0x%x\n", (unsigned int) file_cache_construct(10) );
    printf("Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);
    const char *nm = "ad.out";
//    ch->file_cache_pin_files(ch, fn, 4);
//    printf("Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);
    for ( i = 0; i < 4; i++) {
	rPt = file_cache_file_data(ch, fn[i]);
	printf("%x\n", rPt);
    }

    ch->file_cache_pin_files(ch, &fn[1], 1);
    printf(" 2nd Pin Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);


    wPt = ch->file_cache_mutable_file_data(ch, fn[3]);
    sprintf(wPt, "%s", "ABHIROOP DABRAL");
    ch->file_cache_unpin_files(ch, &fn[3], 1);
    printf("After Unpin-Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);

//    pthread_exit(NULL);
    return 0;
}
