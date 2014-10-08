//
// Copyright (c) 2013 Nutanix Inc. All rights reserved.
//
// The problem is to implement a file cache in C that implements the defined
// interface given below for struct 'file_cache'. The typical usage is for a
// client to call 'file_cache_pin_files()' to pin a bunch of files in the cache
// and then either read or write to their in-memory contents in the cache.
// Writing to a cache entry makes that entry 'dirty'. Before a dirty entry can
// be evicted from the cache, it must be unpinned and has to be cleaned by
// writing the corresponding data to storage.
//
// All files are assumed to have size 10KB. If a file doesn't exist to begin
// with, it should be created and filled with zeros - the size should be 10KB.
//
// 'file_cache' should be a thread-safe object that can be simultaneously
// accessed by multiple threads. If you are not comfortable with concurrent
// programming, then it may be single-threaded (see alternative in the
// file_cache_pin_files() comment). To implement the problem in its entirety
// may require use of third party libraries and headers. For the sake of
// convenience, it is permissible (although not preferred) to substitute
// external functions with stub implementations, but in doing so, please be
// clear what the intended behavior and side effects would be.
//
// Do not be overly concerned about portability. Pick your platform of choice
// (Nutanix develops on Linux) and be consistent.
//
// The problem has an upper limit of two hours. Note that we'd rather see a
// stable and clean solution that implements a subset of the functionality than
// a complete but-buggy one.
//
// If you have any questions, please email both brian@nutanix.com and
// bnc@nutanix.com. If no reply is received in a timely manner, please make a
// judgement call and state your assumptions in the code or response email.

#ifndef _NUTANIX_FILE_CACHE_H_
#define _NUTANIX_FILE_CACHE_H_

typedef struct file_cache file_cache;

/*-----------------------------Changes Start from Here ------------------------ */

//#define DEBUG
/* Structure definition for struct file_cache. This acts a meta data for the file cache
 * and has all the function pointers and pointer to actual file cache nodes.
 * All the declarations have inline comments describing there use and role in the file cache.
*/

struct file_cache {
    int maxSize;		   /* Max size of file_cache passed to constructor */
    int currentSize;               /* Current Size of the file_cache */
    struct __node_cache *nodeHead; /* Pointer to the head of list of cache nodes */
    struct file_cache **selfRef;   /* This is used to set the static pt in constructor to NULL. */
   /* As we cant change the function signature of the constructor and otherwise if once destroy is called,
      no new instances of file cache can be initialized until a new process calls the constructor because 
      static variable in constructor is still poninting to same heap memory initialized in previous call to constuctor.
      So this is set to NULL before freeing the memory in destroy call */

    /* function pointers */
    void (*file_cache_destroy)(file_cache *cache);

    void (*file_cache_pin_files)(file_cache *cache,
                          const char **files,
                          int num_files);

    void (*file_cache_unpin_files)(file_cache *cache,
                            const char **files,
                            int num_files);

    const char *(*file_cache_file_data)(file_cache *cache, const char *file);

    char *(*file_cache_mutable_file_data)(file_cache *cache, const char *file);


};

/* Definition of struct node_cache. See inline commints for each member role. */

struct __node_cache {
    int refCount;       /* Reference Count for each cache data - Cant Unpin until > 0 */
    char dirty;         /* Dirty Byte. If set cache should be flushed to Disk before Unpining  */
    char *name;         /* Name of the file as specified in Pin API call assuming to be a absolute path */
    char *cache;        /* Pointer to 10Kb char buffer. */
}; 

/* Simple definition of a variadic debug printf function for debugging purpose */
# ifndef DEBUG			
# define DEBUG_T 0

#else
# define DEBUG_T 1 	
#endif

/******************* MACRO declaration the debug print: **********************************/

#define dbug_p(fmt,...) \
	do { if(DEBUG_T) fprintf(stderr, "%s:%d:%s():" fmt,__FILE__,\
				__LINE__,__func__,##__VA_ARGS__); } while(0)

/*********************************** END HERE ****************************************************************/


// Constructor. 'max_cache_entries' is the maximum number of files that can
// be cached at any time.
struct file_cache *file_cache_construct(int max_cache_entries);

// Destructor. Flushes all dirty buffers.
void file_cache_destroy(file_cache *cache);

// Pins the given files in array 'files' with size 'num_files' in the cache.
// If any of these files are not already cached, they are first read from the
// local filesystem. If the cache is full, then some existing cache entries may
// be evicted. If no entries can be evicted (e.g., if they are all pinned, or
// dirty), then this method will block until a suitable number of cache
// entries becomes available. It is OK for more than one thread to pin the
// same file, however the file should not become unpinned until both pins
// have been removed.
//
// Is is the application's responsibility to ensure that the files may
// eventually be pinned. For example, if 'max_cache_entries' is 5, an
// irresponsible client may try to pin 4 files, and then an additional 2
// files without unpinning any, resulting in the client deadlocking. The
// implementation *does not* have to handle this.
//
// If you are not comfortable with multi-threaded programming or
// synchronization, this function may be modified to return a boolean if
// the requested files cannot be pinned due to the cache being full. However,
// note that entries in 'files' may already be pinned and therefore even a
// full cache may add additional pins to files.
void file_cache_pin_files(file_cache *cache,
                          const char **files,
                          int num_files);

// Unpin one or more files that were previously pinned. It is ok to unpin
// only a subset of the files that were previously pinned using
// file_cache_pin_files(). It is undefined behavior to unpin a file that wasn't
// pinned.
void file_cache_unpin_files(file_cache *cache,
                            const char **files,
                            int num_files);

// Provide read-only access to a pinned file's data in the cache.
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file isn't pinned.
const char *file_cache_file_data(file_cache *cache, const char *file);

// Provide write access to a pinned file's data in the cache. This call marks
// the file's data as 'dirty'. The caller may update the contents of the file
// by writing to the memory pointed by the returned value.
//
// Multiple clients may have access to the data, however the cache *does not*
// have to worry about synchronizing the clients' accesses (you may assume
// the application does this correctly).
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file is not pinned.
char *file_cache_mutable_file_data(file_cache *cache, const char *file);

#endif  // _NUTANIX_FILE_CACHE_H_
