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


typedef struct s_cache_item * CACHEITEM;

struct s_cache_item {
	LOCATION  Location;
	CACHEITEM NextItem;
	CACHEITEM PrevItem;
	size_t    Reffs;
	long      Ident;
	CACHEOBJ  Object;
	size_t    Size;
	long      Date;
	long      Expires;
	void    (*dtor)(void*);
};
static CACHEITEM __cache_beg = NULL;
static CACHEITEM __cache_end = NULL;
static size_t    __cache_num = 0;
static size_t    __cache_mem = 0;
static LOCATION  __cache_dir = NULL;


/*----------------------------------------------------------------------------*/
static CACHEITEM
create_item (LOCATION loc, CACHEOBJ object, size_t size, void (*dtor)(void*))
{
	CACHEITEM citem = malloc (sizeof (struct s_cache_item));
	citem->Location = location_share (loc);
	citem->Ident     = 0;
	citem->Object   = object;
	citem->Size     = size;
	citem->Date     = 0;
	citem->Expires  = 0;
	citem->dtor     = dtor;
	citem->Reffs    = 1;
	
	if (__cache_beg) __cache_beg->PrevItem = citem;
	else             __cache_end           = citem;
	citem->PrevItem = NULL;
	citem->NextItem = __cache_beg;
	__cache_beg     = citem;
	__cache_num++;
	__cache_mem += size;
	
	return citem;
}

/*----------------------------------------------------------------------------*/
static void
destroy_item (CACHEITEM citem)
{
	if (citem->PrevItem) citem->PrevItem->NextItem = citem->NextItem;
	else                 __cache_beg               = citem->NextItem;
	if (citem->NextItem) citem->NextItem->PrevItem = citem->PrevItem;
	else                 __cache_end               = citem->PrevItem;
	__cache_num--;
	__cache_mem -= citem->Size;
	free_location (&citem->Location);
	free (citem);
}


/*============================================================================*/
CACHED
cache_insert (LOCATION loc, long ident,
              CACHEOBJ * object, size_t size, void (*dtor)(void*))
{
	CACHEITEM citem = create_item (loc, *object, size, dtor);
	citem->Ident    = ident;
	
	*object = NULL;
	
	return citem->Object;
}


/*============================================================================*/
CACHED
cache_lookup (LOCATION loc, long ident, long * opt_found)
{
	CACHEITEM found = NULL;
	CACHEITEM citem = __cache_beg;
	
	while (citem) {
		if (location_equal (loc, citem->Location)) {
			if (ident == citem->Ident) {
				found = citem;
				break;
			} else if (opt_found && citem->Ident) {
				found = citem;
			}
		}
		citem = citem->NextItem;
	}
	if (found) {
		if (found->PrevItem) {
			found->PrevItem->NextItem = found->NextItem;
			if (found->NextItem) found->NextItem->PrevItem = found->PrevItem;
			else                 __cache_end               = found->PrevItem;
			__cache_beg->PrevItem = found;
			found->PrevItem       = NULL;
			found->NextItem       = __cache_beg;
			__cache_beg           = found;
		}
		if (opt_found) {
			*opt_found = found->Ident;
		}
		return found->Object;
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
	CACHEITEM citem = __cache_beg;
	
	while (citem) {
		if (cached == citem->Object) {
			citem->Reffs++;
			if (exchange && *exchange != citem->Location) {
				free_location (exchange);
				*exchange = location_share (citem->Location);
			}
			return citem->Object;
		}
		citem = citem->NextItem;
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
		CACHEITEM citem = __cache_beg;
		while (citem) {
			if (citem->Object == cached) {
				if ((!citem->Reffs || !--citem->Reffs) && erase) {
					if (citem->dtor) {
						(*citem->dtor)(citem->Object);
					} else {
						object = citem->Object;
					}
					destroy_item (citem);
				}
				*p_object = NULL;
				break;
			}
			citem = citem->NextItem;
		}
	}
	return object;
}


/*============================================================================*/
size_t
cache_clear (CACHED this_n_all)
{
	size_t    num   = 0;
	CACHEITEM citem = __cache_beg;
	
	while (citem) {
		CACHEITEM next = citem->NextItem;
		if (!citem->Reffs && citem->dtor
		    && (!this_n_all || this_n_all == citem->Object)) {
			(*citem->dtor)(citem->Object);
			destroy_item (citem);
			num++;
			if (this_n_all) break;
		}
		citem = next;
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
		if ((*p_info = info) != NULL) {
			size_t    num   = __cache_num;
			CACHEITEM citem = __cache_beg;
			do {
				LOCATION local = (citem->Ident ? citem->Location : citem->Object);
				info->Size    = citem->Size;
				info->Used    = citem->Reffs;
				info->Ident   = citem->Ident;
				info->Source  = citem->Location;
				info->Date    = citem->Date;
				info->Expires = citem->Expires;
				info->File    = local->File;
				info->Object  = citem->Object;
				citem = citem->NextItem;
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
		if ((fh = open (buf, O_RDWR|O_CREAT|O_TRUNC, 0666)) >= 0) {
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
