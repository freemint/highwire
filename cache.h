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

typedef struct s_cache_info {
	size_t       Size;
	size_t       Used;
	long         Ident;
	LOCATION     Source;
	long         Date;
	long         Expires;
	const char * File;
	CACHED       Object;
} * CACHEINF;

size_t cache_info (size_t * size, CACHEINF *);


BOOL     cache_setup  (const char * dir);

typedef enum {
	CQ_NONE  = 0,
	CQ_BUSY  = 0x01, CQ_LOCAL = 0x02,
	CQ_FOUND = 0x10, CQ_MATCH = 0x20
} CQRESULT;
#define CQresultDsk(r)   (r & 0x0F)
#define CQresultMem(r)   (r & 0xF0)

CQRESULT cache_exclusive (LOCATION);
void     cache_abort     (LOCATION);
LOCATION cache_assign    (LOCATION, void *, size_t,
                          const char * type, long date, long expires);


#endif /*__CACHE_H__*/
