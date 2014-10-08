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
 *
 *
 * The file_cache uses 2 mutex variables 'metaLock' & 'pinLock' and a condition variable 'slotcv' to synchronise
 * file cache creation and the pin and unpinning of files in the file cache respectively.
 * metaLock allows only one thread to enter the constructor and desctructor at a time, therby enforcing singelton pattern.
 * pinLock synchronises the access to pin and unpin access to file cache, if there a no empty slot the thread will block on 
 * slotcv condition variable which is signaled in unpin function whenever an empty slot opens up.
 *
 * Developed & tested on Ubuntu 32bit with gcc 4.4.3
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

pthread_mutex_t metaLock = PTHREAD_MUTEX_INITIALIZER;    /* Mutex used in constructor to facilitate singelton instance */
pthread_mutex_t pinLock;  				 /* Mutex used in pin & unpin to searially access file_cache DS */
pthread_cond_t slotcv;	     			         /* Condition variable to signal if cache has empty slot */

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

    pthread_mutex_lock(&metaLock);

    if ( !fileCachePt ) {
	fileCachePt = malloc(sizeof(struct file_cache));
	if ( !fileCachePt )
	    return NULL;

	memset(fileCachePt, 0, sizeof(struct file_cache));
	fileCachePt->maxSize = max_cache_entries;
	fileCachePt->currentSize = 0;
	fileCachePt->selfRef = &fileCachePt;

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

	pthread_mutex_init(&pinLock, NULL);
	pthread_cond_init (&slotcv, NULL);
    }
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
    FILE *filePt;

    if ( !cache )
	return;

    pthread_mutex_lock(&metaLock);

    size = cache->maxSize;
    i = 0;

    while ( i < size ) {  /* Free the cache and name in each nodeCache */
	if ( cache->nodeHead[i].dirty ) { /* Flush back to Disk */ 
	    filePt = fopen(cache->nodeHead[i].name, "w");
	    if ( !filePt ) {  /* Can't Open file to write to,error out without modifying any metadata */
		pthread_mutex_unlock(&metaLock);
		return;
	    }
	    fwrite(cache->nodeHead[i].cache, 1, CACHE_SIZE, filePt);
	    fclose(filePt);
	}

	free(cache->nodeHead[i].name);
	free(cache->nodeHead[i].cache);
	i++;
    }

    /* Now free nodeCache itself */
    free(cache->nodeHead);

    /* Setting the static pointer in construct call to NULL */
    *(cache->selfRef) = NULL;

    /* Just to make sure that all dangling refrences will seg fault after this */
    memset(cache, 0, sizeof(struct file_cache));

    /* Now free file_cache structure */
    free(cache);

    pthread_mutex_unlock(&metaLock);

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
    dbug_p("Entering PINING:\n");
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
		dbug_p("CACHE HIT for :%s: RefCount:%d:\n", fName, cache->nodeHead[j].refCount);
		break;
	    }
	}
	if ( 0 == hit ) { /* Cache Miss */
	    dbug_p("CACHE MISS:%d\n", cache->currentSize);
	    while ( cache->currentSize == cache->maxSize ) /* Cache is full, wait for a slot to open */ {
		dbug_p("WAITING ....\n"); //ABHI
		pthread_cond_wait(&slotcv, &pinLock);
	    }
	    dbug_p("CACHE MISS-UNLOCK:%d\n", cache->currentSize);

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
		dbug_p("PINNING:%s:\n", cache->nodeHead[freeIndex].name); //ABHI
	    }
	    else { /* File not present. Create on Disk with 10 Kb '\0' */

		filePt = fopen(fName, "w+");
		if ( !filePt ) {
  		    pthread_mutex_unlock(&pinLock);
		    return;
		}
		fclose(filePt); 
		ret = truncate(fName, CACHE_SIZE);
		dbug_p(" RET FROM TRUCN:%d\n", ret); // ABHI
	    }

	}
    }
    dbug_p("Leaving PINNING:\n");
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
    dbug_p("Entering UNPINING:\n");
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
		    dbug_p("UNPINNING:%s:\n", cache->nodeHead[j].name); //ABHI
		    free(cache->nodeHead[j].cache);
		    free(cache->nodeHead[j].name);
		    memset(&(cache->nodeHead[j]), 0, sizeof(struct __node_cache));

		    if ( cache->currentSize == cache->maxSize ) {
			dbug_p("SIZE ARE SAME:%d:%d:\n", cache->currentSize, cache->maxSize);
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
    dbug_p("LEAVINF UNPIN:\n");
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



/* Below are some unit test cases (very basic) and some code I wrote to test the multithreaded behavior of file cache */
/* To turn ON the debug printfs -dbug_p() either uncomment define DEBUG in file_cache.h or just compile with -DDEBUG flag */

/**************************************************** TESTING ************************************/


struct payload {
    struct file_cache *c;
    const char **name;
    int num;
    int tid;
};


/* Used by pthreads to unload the arguments and call file_cache function
 * Called from main().
 */
void *test_fc(void *payload)
{
    sleep(2);
    printf("ENTERING TEST_FC---------------------\n");
    struct payload *pt = (struct payload *) payload;
    printf("TID:%d:\n", pt->tid);
    if ( pt->tid == 1) {

	printf("Pinning- PID:%d:File-%s:\n", pt->tid, *pt->name);
	sleep(5);
    	pt->c->file_cache_pin_files(pt->c, pt->name, pt->num);
    }
    else if ( pt->tid == 0) {
	printf("Pinning- PID:%d:File-%s:\n",pt->tid, *pt->name);
    	pt->c->file_cache_pin_files(pt->c, pt->name, pt->num);
    }
    if ( pt->tid == 2) {

	printf("UNPinning- PID:%d:File-%s:\n", pt->tid, *pt->name);
	sleep(5);
    	pt->c->file_cache_unpin_files(pt->c, pt->name, pt->num);
    }
 
}


/*
 * Some unit test case for the file cache implementation.
*/

void test_case()
{
    struct file_cache *pt1 = NULL;
    struct file_cache *pt2 = NULL;

    const char *rPt;		/* Read pointer returned from */
    char *wPt;                  /* Write pointer returned from */
    const char *fileList;
    int total = 7, passed = 0, maxcount = 0, currentcount = 0, i, flag = 0;
    const char *fn [4];

    fn[0] = "bt.c";
    fn[1] = "ad.out";
    fn[2] = "check.out";
    fn[3] = "qw.out";


    printf("Executing Some Basic Test Case.\n");

    /* Test to check if only a singleton instance is returned 
       from constructor even on multiple invocation */

    pt1 = file_cache_construct(4);
    pt2 = file_cache_construct(9);
    if ( pt1 == pt2 ) {
	++passed;
	printf("Singleton test passed.\n");
    }
    else
	printf("Singleton test failed.\n");
    

    /* Test to check if Pining of File Cache working */

    maxcount = pt1->maxSize;
    currentcount = pt1->currentSize;
    if ( maxcount == 4 && currentcount == 0) {
	++passed;
	printf("File Cache initialaized to correct counts.\n");
    }
    else
	printf("File Cache *NOT* initialaized to correct counts.%d:%d\n", maxcount, currentcount);


    pt1->file_cache_pin_files(pt1, fn, 4);
    maxcount = 0, currentcount = 0;  /* Foe this test the files in fn should be present */
    maxcount = pt1->maxSize;
    currentcount = pt1->currentSize;
    if ( maxcount == 4 && currentcount == 4) {
	++passed;
	printf("File Cache PIN success.\n");
    }
    else
	printf("File Cache PIN failure.\n");


    pt1->file_cache_unpin_files(pt1, &fn[3], 1);
    currentcount = pt1->currentSize;

    /* Normal Unpin test case */
    if ( currentcount == 3 ) {
	++passed;
	printf("Normal Unpin works fine.\n");
    }
    else
	printf("Normal Unpin failed.\n");

    /* Test to check pinnin of alreasy pinned file  */

    pt1->file_cache_pin_files(pt1, &fn[1], 1);
    currentcount = pt1->currentSize;
    if ( currentcount == 3 ) {
	++passed;
	printf("Repin, already pinned file pass.\n");
    }
    else
	printf("Repin, already pinned file FAILED.\n");

    /* Testing Reading File Cache */
    /* File no 3 is already UNpined so should 
       not return valid pointer for that
     */

    printf("PASSED:%d:\n", passed);
    for ( i = 0; i < 4; i++) {
        rPt = pt1->file_cache_file_data(pt1, fn[i]);
	if ( i == 3 ) {
	    if ( rPt == NULL ) {
	    ++passed;
	    printf("Normal read test(-reading unpinned file) PASS.\n");
	    }
	    else
		printf("Normal read test(-reading unpinned file) FAIL.\n");
	}
	else if ( rPt != NULL ) {
	    printf("Normal read test PASS. File no:%d:\n", i);
	}
	else {
	    printf("Normal read test FAIL. File No:%d:\n", i);
	    flag = 1;
	}
    }
    if ( !flag )
	++passed;


    printf("Total Test Case executed: %d: Passed: %d: Failed: %d\n",
	    total, passed, (total -passed) );

}

/*
int main()
{
    
    const char *rPt;
    char *wPt;
    const char *nm;
    const char *nm1, *nm2;
    pthread_t threads[4];
    pthread_attr_t attr;
    int i, rc, t;
    void *status;

    struct file_cache *ch = file_cache_construct(4);
    printf("0x%x\n", (unsigned int ) ch);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    const char *fn [4];
    nm = "gh.out";
    fn[0] = "bt.c";
    fn[1] = "ad.out";
    fn[2] = "check.out";
    fn[3] = "qw.out";

    struct payload payload;
    payload.c = ch;
    payload.name = &nm;
    payload.num = 1;
    payload.tid = 0;


    printf("Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);
    printf("0x%x\n", (unsigned int) file_cache_construct(10) );
    printf("Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);
    ch->file_cache_pin_files(ch, fn, 4);
    printf("Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);

    ch->file_cache_pin_files(ch, &fn[1], 1);
    printf(" 2nd Pin Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);

    printf("Creating 1 thread:\n");

    rc = pthread_create(&threads[0], &attr, test_fc, (void *) &payload);
    if (rc)
	printf("ERROR; return code from pthread_create() is %d\n", rc);

    printf("Sleeping\n");
    sleep(20);
    struct payload payload1;
    nm1 = "bt.c";
    payload1.c = ch;
    payload1.name = &nm1;
    payload1.num = 1;
    payload1.tid = 1;

    printf("Creating 2nd thread:\n");

    rc = pthread_create(&threads[1], &attr, test_fc, (void *) &payload1);
    if (rc)
	printf("ERROR; return code from pthread_create() is %d\n", rc);


    printf("Sleeping-2\n");
    sleep(20);
    struct payload payload2;
    nm2 = "qw.out";
    payload2.c = ch;
    payload2.name = &nm2;
    payload2.num = 1;
    payload2.tid = 2;

    printf("Creating 3rd thread:\n");

    rc = pthread_create(&threads[2], &attr, test_fc, (void *) &payload2);
    if (rc)
	printf("ERROR; return code from pthread_create() is %d\n", rc);


    for ( i = 0; i < 4; i++) {
	rPt = ch->file_cache_file_data(ch, fn[i]);
	printf("%#x\n", (unsigned int) rPt);
    }

    wPt = ch->file_cache_mutable_file_data(ch, fn[0]);
    sprintf(wPt, "%s", "ABHIROOP DABRAL");
//    ch->file_cache_unpin_files(ch, &fn[3], 1);
//    printf("After Unpin-Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);


    pthread_attr_destroy(&attr);
    for(t=0; t<3; t++) {
	rc = pthread_join(threads[t], &status);
	if (rc) {
	    printf("ERROR; return code from pthread_join() is %d\n", rc);
	    exit(-1);
	}
	printf("Main: completed join with thread %d having a status of %ld\n",t,(long)status);
    }

    printf("Max Size:%d: CurrentSize:%d\n", ch->maxSize, ch->currentSize);
    printf("Destroying file cache before exit.\n");

    ch->file_cache_destroy(ch);

//    printf("File Cache Should give seg fault on access:\n");
//    printf("MAX COUNT:%s:\n", ch->nodeHead[2].name);

    test_case();
//    pthread_exit(NULL);
    return 0;
} 
*/
