#include <stddef.h>
#include <stdlib.h>

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
	void   (*dtor)(void*);
} * CACHEITEM;
static CACHEITEM __cache     = NULL;
static size_t    __cache_num = 0;
static size_t    __cache_mem = 0;


/*============================================================================*/
CACHED
cache_insert (LOCATION loc, long hash,
              CACHEOBJ * object, size_t size, void (*dtor)(void*))
{
	CACHEITEM citem = malloc (sizeof (struct s_cache_item));
	citem->Location = location_share (loc);
	citem->Hash     = hash;
	citem->Object   = *object;
	citem->Size     = size;
	citem->dtor     = dtor;
	citem->Reffs    = 1;
	citem->Next     = __cache;
	__cache          = citem;
	__cache_num++;
	__cache_mem += size;
	
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
			} else if (opt_found) {
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
				info->Size   = citem->Size;
				info->Used   = citem->Reffs;
				info->Hash   = citem->Hash;
				info->File   = citem->Location->File;
				info->Object = citem->Object;
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
