#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined (__GNUC__)
# include <fcntl.h>
# include <unistd.h>

#elif defined (LATTICE)
# include <fcntl.h>
#endif

#include "defs.h"
#include "Location.h"
#include "cache.h"


typedef struct s_cache_item * CACHEITEM;
typedef struct s_cache_node * CACHENODE;

struct s_cache_item {
	CACHENODE Node;
	ULONG     NodeHash;
	CACHEITEM NodeNext;
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

struct s_cache_node {
	CACHENODE BackNode;
	UWORD     NodeMask;
	unsigned  Level4x :8;
	unsigned  Filled  :8;
	union {
		CACHENODE Node;
		CACHEITEM Item;
	}         Array[16];
};

static struct s_cache_node __tree_base = {
	NULL, 0x0000, 0, 0,
	{	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
		{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL} }
};


/*----------------------------------------------------------------------------*/
#if 0
static long __stats (CACHENODE node)
{
	static int level = 0;
	long  sum = 0;
	UWORD mask, idx;
	for (idx = 0, mask = 0x0001, ++level; idx < 16; idx++, mask <<= 1) {
		if (node->Array[idx].Node) {
			sum += (node->NodeMask & mask ? __stats (node->Array[idx].Node)
			                              : level);
		}
	}
	level--;
	return sum;
}
static void exit_stats (void)
{
	long res   = __stats (&__tree_base);
	int  level = 1;
	size_t n = __cache_num;
	while (n > 16) {
		level++;
		n >>= 4;
	}
	printf ("quality(%lu): %li\n", __cache_num, res - (__cache_num * level));
}
#define EXIT_STATS
#endif


/*----------------------------------------------------------------------------*/
static CACHENODE
tree_node (ULONG hash)
{
	CACHENODE node = &__tree_base;
	UWORD     idx  = hash & 0xF;
	while (node->NodeMask & (1 << idx)) {
		node = node->Array[idx].Node;
		idx  = (hash >>= 4) & 0xF;
	}
	return node;
}

/*----------------------------------------------------------------------------*/
static CACHEITEM *
tree_slot (LOCATION loc)
{
	ULONG     hash  = location_Hash (loc);
	CACHENODE node  = tree_node (hash);
	UWORD     idx   = (hash >> node->Level4x) & 0xF;
	return &node->Array[idx].Item;
}

/*----------------------------------------------------------------------------*/
static CACHEITEM
tree_item (LOCATION loc)
{
	ULONG     hash  = location_Hash (loc);
	CACHENODE node  = tree_node (hash);
	UWORD     idx   = (hash >> node->Level4x) & 0xF;
	CACHEITEM citem = node->Array[idx].Item;
	while (citem && (citem->Ident || !location_equal (loc, citem->Location))) {
		citem = citem->NodeNext;
	}
	return citem;
}


/*----------------------------------------------------------------------------*/
static CACHEITEM
create_item (LOCATION loc, CACHEOBJ object, size_t size, void (*dtor)(void*))
{
	ULONG     hash  = location_Hash (loc);
	CACHENODE node  = tree_node (hash);
	UWORD     idx   = (hash >> node->Level4x) & 0xF;
	CACHEITEM nitem = node->Array[idx].Item;
	CACHEITEM citem = NULL;
#ifdef EXIT_STATS
	static BOOL __once = TRUE;
	if (__once) {
		__once = FALSE;
		atexit (exit_stats);
	}
#endif

	if (nitem && nitem->NodeHash != hash) {
		do {
			CACHENODE back = node;
			if ((node = malloc (sizeof(struct s_cache_node))) != NULL) {
				back->Array[idx].Node = node;
				back->NodeMask       |= (1 << idx);
				node->BackNode = back;
				node->NodeMask = 0x0000;
				node->Level4x  = back->Level4x +4;
				node->Filled   = 1;
				memset (node->Array, 0, sizeof(node->Array));
				node->Array[(nitem->NodeHash >> node->Level4x) & 0xF].Item = nitem;
				citem = nitem;
				do {
					citem->Node = node;
				} while ((citem = citem->NodeNext) != NULL);
				idx = (hash >> node->Level4x) & 0xF;
			}
		} while (node->Array[idx].Item);
	}
	if (node && (citem = malloc (sizeof (struct s_cache_item))) != NULL) {
		citem->Node           = node;
		citem->NodeHash       = hash;
		if ((citem->NodeNext  = node->Array[idx].Item) == NULL) {
			node->Filled++;
		}
		node->Array[idx].Item = citem;
		
		citem->Location = location_share (loc);
		citem->Ident    = 0;
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
	}
	return citem;
}

/*----------------------------------------------------------------------------*/
static void
destroy_item (CACHEITEM citem)
{
	CACHENODE node = citem->Node;
	ULONG     hash = citem->NodeHash;
	UWORD     idx  = (hash >> node->Level4x) & 0xF;

	if (node->Array[idx].Item != citem) {
		CACHEITEM * nptr = &node->Array[idx].Item;
		while (*nptr) {
			if (*nptr == citem) {
				*nptr = citem->NodeNext;
				break;
			}
			nptr = &(*nptr)->NodeNext;
		}

	} else if ((node->Array[idx].Item = citem->NodeNext) == NULL) {
		node->Filled--;
		while (!node->Filled && node->BackNode) {
			CACHENODE back = node->BackNode;
			free (node);
			node = back;
			idx  = (hash >> node->Level4x) & 0xF;
			node->Array[idx].Node = NULL;
			node->NodeMask       &= ~(1 << idx);
			node->Filled--;
		}
	}

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
	ULONG       hash    = location_Hash (loc);
	CACHENODE   node    = tree_node (hash);
	UWORD       idx     = (hash >> node->Level4x) & 0xF;
	CACHEITEM * p_cache = &node->Array[idx].Item;
	CACHEITEM * p_found = NULL;
	CACHEITEM   citem;

	while ((citem = *p_cache) != NULL) {
		if (location_equal (loc, citem->Location)) {
			if (ident == citem->Ident) {
				p_found = p_cache;
				break;
			} else if (opt_found && citem->Ident) {
				p_found = p_cache;
			}
		}
		p_cache = &citem->NodeNext;
	}
	if (p_found) {
		citem = *p_found;
		if (p_found != &node->Array[idx].Item) {
			*p_found              = citem->NodeNext;
			citem->NodeNext       = node->Array[idx].Item;
			node->Array[idx].Item = citem;
		}
		if (citem->PrevItem) {
			citem->PrevItem->NextItem = citem->NextItem;
			if (citem->NextItem) citem->NextItem->PrevItem = citem->PrevItem;
			else                 __cache_end               = citem->PrevItem;
			__cache_beg->PrevItem = citem;
			citem->PrevItem       = NULL;
			citem->NextItem       = __cache_beg;
			__cache_beg           = citem;
		}
		if (opt_found) {
			*opt_found = citem->Ident;
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
				info->Source  = citem->Location;
				info->Ident   = citem->Ident;
				info->Used    = citem->Reffs;
				info->Local   = (citem->Ident ? NULL : citem->Object);
				info->Date    = citem->Date;
				info->Expires = citem->Expires;
				info->Object  = citem->Object;
				info->Size    = citem->Size;
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


/*============================================================================*/
CRESULT
cache_exclusive (LOCATION loc)
{
	CACHEITEM citem = tree_item (loc);
	CRESULT   res;
	
	if (citem) {
		res = (!citem->Object ? CR_BUSY : CR_LOCAL);
	/*	printf ("cache_exclusive(%s): %s\n",
		        loc->FullName, (res == CR_BUSY ? "busy" : "found"));*/
	} else {
		create_item (loc, NULL, 0uL, (void(*)(void*))0);
		res = CR_NONE;
	/*	printf ("cache_exclusive(%s) set\n", loc->FullName);*/
	}
	return res;
}

/*============================================================================*/
void
cache_abort (LOCATION loc)
{
	CACHEITEM citem = tree_item (loc);
	
	if (!citem) {
		printf ("cache_abort(%s): not found!\n", loc->FullName);
	
	} else {
	/*	printf ("cache_abort(%s)\n", loc->FullName);*/
		destroy_item (citem);
	}
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
	static long _file_id = 0;
	
	LOCATION  loc   = NULL;
	CACHEITEM citem = tree_item (src);
	
	if (!citem) {
		printf ("cache_assign(%s): not found!\n", src->FullName);
	
	} else if (citem->Object) {
		printf ("cache_assign(%s): already in use!\n", src->FullName);
	
	} else {
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
			sprintf (buf, "%08lX%s%s", _file_id, dot, type);
			loc = new_location (buf, __cache_dir);
			location_FullName (loc, buf, sizeof(buf));
			if ((fh = open (buf, O_RDWR|O_CREAT|O_TRUNC, 0666)) >= 0) {
				write (fh, data, size);
				close (fh);
				_file_id++;
				citem->Object  = location_share (loc);
				citem->dtor    = file_dtor;
				citem->Size    = size;
				citem->Date    = date;
				citem->Expires = expires;
				citem->Reffs--;
			} else {
				free_location (&loc);
			}
		}
		if (!loc) { /* either no cache dir set or file coudn't be written */
			destroy_item (citem);
		}
	}
	return loc;
}


/*============================================================================*/
CRESULT
cache_query (LOCATION loc, long ident, CACHEINF info)
{
	CACHEITEM citem = *tree_slot (loc);
	CRESULT   res_d = CR_NONE, res_m = CR_NONE;
	
	info->Source  = NULL;
	info->Ident   = 0;
	info->Used    = 0;
	info->Local   = NULL;
	info->Date    = 0;
	info->Expires = 0;
	info->Object  = NULL;
	info->Size    = 0;
	
	while (citem) {
		if (location_equal (loc, citem->Location)) {
			if (!citem->Ident) {
				if (citem->Object) {
					info->Local   = citem->Object;
					info->Date    = citem->Date;
					info->Expires = citem->Expires;
					res_d         = CR_LOCAL;
				} else {
					res_d         = CR_BUSY;
				}
				if (!ident || !info->Source) {
					info->Source = citem->Location;
				}
				if (!ident || (res_m == CR_MATCH)) break;
			
			} else if (ident == citem->Ident || !info->Object) {
				info->Object = citem->Object;
				info->Ident  = citem->Ident;
				info->Size   = citem->Size;
				if (ident != citem->Ident) {
					if (!ident || !info->Source) {
						info->Source = citem->Location;
					}
					res_m     = CR_FOUND;
				} else {
					info->Source = citem->Location;
					res_m     = CR_MATCH;
					if (res_d) break;
				}
			}
		}
		citem = citem->NodeNext;
	}
	
	return (res_d | res_m);
}
