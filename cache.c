#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined (__GNUC__)
# include <fcntl.h>
# include <unistd.h>
#endif

#include "defs.h"
#include "Location.h"
#include "cache.h"


typedef struct s_cache_item {
	struct s_cache_item * Next;
	size_t                Reffs;
	LOCATION Location;
	long     Hash;
	CACHEOBJ Object;
	size_t   Size;
	long     Date;
	long     Expires;
	void   (*dtor)(void*);
} * CACHEITEM;
static CACHEITEM __cache     = NULL;
static size_t    __cache_num = 0;
static size_t    __cache_mem = 0;
static LOCATION  __cache_dir = NULL;


/*----------------------------------------------------------------------------*/
static CACHEITEM
create_item (LOCATION loc, CACHEOBJ object, size_t size, void (*dtor)(void*))
{
	CACHEITEM citem = malloc (sizeof (struct s_cache_item));
	citem->Location = location_share (loc);
	citem->Hash     = 0;
	citem->Object   = object;
	citem->Size     = size;
	citem->Date     = 0;
	citem->Expires  = 0;
	citem->dtor     = dtor;
	citem->Reffs    = 1;
	citem->Next     = __cache;
	__cache          = citem;
	__cache_num++;
	__cache_mem += size;
	
	return citem;
}


/*============================================================================*/
CACHED
cache_insert (LOCATION loc, long hash,
              CACHEOBJ * object, size_t size, void (*dtor)(void*))
{
	CACHEITEM citem = create_item (loc, *object, size, dtor);
	citem->Hash     = hash;
	
	*object = NULL;
	
	return citem->Object;
}


/*============================================================================*/
CACHED
cache_lookup (LOCATION loc, long hash, long * opt_found)
{
	CACHEITEM * p_cache = &__cache;
	CACHEITEM * p_found = NULL;
	CACHEITEM   citem;
	
	while ((citem = *p_cache) != NULL) {
		if (location_equal (loc, citem->Location)) {
			if (hash == citem->Hash) {
				p_found = p_cache;
				break;
			} else if (opt_found && citem->Hash) {
				p_found = p_cache;
			}
		}
		p_cache = &citem->Next;
	}
	if (p_found) {
		citem = *p_found;
		if (p_found != &__cache) {
			*p_found    = citem->Next;
			citem->Next = __cache;
			__cache     = citem;
		}
		if (opt_found) {
			*opt_found = citem->Hash;
		}
		return citem->Object;
	}
	
	if (opt_found) {
		*opt_found = 0;
	}
	return NULL;
}


/*============================================================================*/
CACHED
cache_bound (CACHED cached, LOCATION * exchange)
{
	CACHEITEM citem = __cache;
	
	while (citem) {
		if (cached == citem->Object) {
			citem->Reffs++;
			if (exchange && *exchange != citem->Location) {
				free_location (exchange);
				*exchange = location_share (citem->Location);
			}
			return citem->Object;
		}
		citem = citem->Next;
	}
	return NULL;
}


/*============================================================================*/
CACHEOBJ
cache_release (CACHED * p_object, BOOL erase)
{
	CACHEOBJ object = NULL;
	CACHED   cached = *p_object;
	if (cached) {
		CACHEITEM * p_cache = &__cache;
		CACHEITEM   citem;
		
		while ((citem = *p_cache) != NULL) {
			if (citem->Object == cached) {
				if ((!citem->Reffs || !--citem->Reffs) && erase) {
					if (citem->dtor) {
						(*citem->dtor)(citem->Object);
					} else {
						object = citem->Object;
					}
					free_location (&citem->Location);
					__cache_num--;
					__cache_mem -= citem->Size;
					*p_cache = citem->Next;
					free (citem);
				}
				*p_object = NULL;
				break;
			}
			p_cache = &citem->Next;
		}
	}
	return object;
}


/*============================================================================*/
size_t
cache_clear (CACHED this_n_all)
{
	size_t num = 0;
	CACHEITEM * p_cache = &__cache;
	CACHEITEM   citem;
	
	while ((citem = *p_cache) != NULL) {
		if (!citem->Reffs && citem->dtor
		    && (!this_n_all || this_n_all == citem->Object)) {
			(*citem->dtor)(citem->Object);
			free_location (&citem->Location);
			__cache_num--;
			__cache_mem -= citem->Size;
			*p_cache = citem->Next;
			free (citem);
			num++;
			if (this_n_all) break;
		} else {
			p_cache = &citem->Next;
		}
	}
	
	
	return num;
}


/*============================================================================*/
size_t
cache_info (size_t * size, CACHEINF * p_info)
{
	if (p_info) {
		CACHEINF info = (__cache_num
		                 ? malloc (sizeof (struct s_cache_info) * __cache_num)
		                 : NULL);
		*p_info = info;
		if (info) {
			size_t    num   = __cache_num;
			CACHEITEM citem = __cache;
			do {
				LOCATION local = (citem->Hash ? citem->Location : citem->Object);
				info->Size    = citem->Size;
				info->Used    = citem->Reffs;
				info->Hash    = citem->Hash;
				info->Source  = citem->Location;
				info->Date    = citem->Date;
				info->Expires = citem->Expires;
				info->File    = local->File;
				info->Object  = citem->Object;
				citem = citem->Next;
				info++;
			} while (--num);
		}
	}
	if (size) {
		*size = __cache_mem;
	}
	return __cache_num;
}


/*============================================================================*/
BOOL
cache_setup (const char * dir)
{
	char buf[1024], * p = strchr (strcpy (buf, dir), '\0');
	LOCATION loc;
	if (p[-1] != '/' && p[-1] != '\\') {
		*(p++) = (strchr (buf, '/') ? '/' : '\\');
	}
	strcpy (p, "cache.idx");
	if ((loc = new_location (buf, NULL)) != NULL) {
		int fh;
		location_FullName (loc, buf, sizeof(buf));
		if ((fh = open (buf, O_RDWR|O_CREAT, 0666)) >= 0) {
			close (fh);
			__cache_dir = loc;
		} else {
			free_location (&loc);
		}
	}
	return (__cache_dir != NULL);
}


/*----------------------------------------------------------------------------*/
static void
file_dtor (void * arg)
{
	LOCATION loc = arg;
	char buf[1024];
	location_FullName (loc, buf, sizeof(buf));
	unlink (buf);
	free_location (&loc);
}

/*============================================================================*/
LOCATION
cache_assign (LOCATION src, void * data, size_t size,
              const char * type, long date, long expires)
{
	static long num = 0;
	LOCATION    loc = NULL;
	if (__cache_dir) {
		const char * dot;
		char buf[1024];
		int  fh;
		if (!type || !*type) {
			type = "";
			dot  = "";
		} else {
			dot  = ".";
		}
		sprintf (buf, "%08lX%s%s", num, dot, type);
		loc = new_location (buf, __cache_dir);
		location_FullName (loc, buf, sizeof(buf));
		if ((fh = open (buf, O_RDWR|O_CREAT, 0666)) >= 0) {
			CACHEITEM citem;
			write (fh, data, size);
			close (fh);
			num++;
			citem = create_item (src, location_share (loc), size, file_dtor);
			citem->Date    = date;
			citem->Expires = expires;
			citem->Reffs--;
		} else {
			free_location (&loc);
		}
	}
	return loc;
}
