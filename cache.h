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
LOCATION cache_assign (LOCATION, void *, size_t,
                       const char * type, long date, long expires);
