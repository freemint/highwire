#ifndef __CACHE_H__
#define __CACHE_H__


typedef void       * CACHEOBJ;
typedef const void * CACHED;

CACHED   cache_insert  (LOCATION, long ident,
                        CACHEOBJ *, size_t size, void (*dtor)(void*));
CACHED   cache_lookup  (LOCATION, long ident, long * opt_found);
CACHED   cache_bound   (CACHED, LOCATION * exchange);
CACHEOBJ cache_release (CACHED *, BOOL erase);
size_t   cache_clear   (CACHED this_n_all);

BOOL     cache_setup  (const char * dir);

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

typedef struct s_cache_info {
	LOCATION Source;
	long     Ident;
	size_t   Used;
	LOCATION Local;
	long     Date;
	long     Expires;
	CACHED   Object;
	size_t   Size;
} * CACHEINF;

size_t cache_info (size_t * size, CACHEINF *);


#endif /*__CACHE_H__*/
