#ifndef __CACHE_H__
#define __CACHE_H__


/*--- general setup and information ------------------------------------------*/

void         cache_setup   (const char * dir, size_t mem_max,
                            size_t dsk_max, size_t dsk_num);
                /* Sets the cache directory and limits.  Arguments might be null
                 * to stay unchanged.
                 * dir:     absolute path where cache files get stored.
                 * dsk_max: maximum amount of bytes used on disk.
                 * dsk_num: maximum number of files to be held in disk cache.
                 * mem_max: maximum for objects held in memory (normally
                 *          images), affects only objects not in use.
                */
const char * cache_DirInfo(void);
                /* Returns the absolute path of the cache directory (or NULL) as
                 * set by cache_setup().
                */

/*--- - - - - - - - - - - ---*/


typedef void       * CACHEOBJ;
typedef const void * CACHED;

CACHED   cache_insert  (LOCATION, long ident, long lc_ident,
                        CACHEOBJ *, size_t size, void (*dtor)(void*));
CACHED   cache_lookup  (LOCATION, long ident, long * opt_found);
CACHED   cache_bound   (CACHED, LOCATION * exchange);
CACHEOBJ cache_release (CACHED *, BOOL erase);
size_t   cache_clear   (CACHED this_n_all);

typedef enum {
	CR_NONE  = 0,
	CR_BUSY  = 0x01, CR_LOCAL = 0x02,
	CR_FOUND = 0x10, CR_MATCH = 0x20
} CRESULT;
#define CResultDsk(r)   (r & 0x0F)
#define CResultMem(r)   (r & 0xF0)

CRESULT  cache_exclusive (LOCATION);
void     cache_abort     (LOCATION);
LOCATION cache_assign    (LOCATION, void *, size_t,
                          const char * type, long date, long expires);
void     cache_expires   (LOCATION, long date);

typedef struct s_cache_info {
	LOCATION     Source;
	long         Ident;
	size_t       Used;
	const char * Cached;
	LOCATION     Local;
	long         Date;
	long         Expires;
	CACHED       Object;
	size_t       Size;
} * CACHEINF;

CRESULT cache_query (LOCATION, long ident, CACHEINF);

size_t  cache_info  (size_t * size, CACHEINF *);


#endif /*__CACHE_H__*/
