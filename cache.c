#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef __PUREC__
# include <tos.h>

#else /* LATTICE || __GNUC__ */
# include <mintbind.h>
# include <fcntl.h>

# if defined (__GNUC__)
#  include <unistd.h>
# endif
#endif

#include "defs.h"
#include "Location.h"
#include "cache.h"


typedef struct s_cache_item * CACHEITEM;
typedef struct s_cache_node * CACHENODE;

static BOOL cache_flush  (CACHEITEM, LOCATION);
static BOOL cache_remove (long num, long use);


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
	char      Cached[10];    /* enough for 'ABCD.xyz' */
};
#define item_isMem( citem )   ((citem)->dtor != NULL)

static CACHEITEM __cache_beg = NULL;
static CACHEITEM __cache_end = NULL;
#define            CACHE_MAX ((size_t)100 *1024)
static size_t    __cache_mem_use = 0;
static size_t    __cache_mem_max = 0;
static size_t    __cache_mem_num = 0;
static size_t    __cache_dsk_use = 0;
static size_t    __cache_dsk_max = 0;
static size_t    __cache_dsk_num = 0;
static size_t    __cache_dsk_lim = 0;
static LOCATION  __cache_dir  = NULL;
static char    * __cache_path = NULL;
static char    * __cache_file = NULL;
static long      __cache_fid = 1;

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
	while (citem && (item_isMem (citem) ||
	                 !location_equal (loc, citem->Location))) {
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
	
	if (dtor) {
		if (!object) {
			puts ("create_item(): mem item without object!");
			return NULL;
		}
	} else {
		if (object) {
			puts ("create_item(): disk item with object!");
			return NULL;
		}
	}
	
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
		citem->Reffs    = 1;
		citem->Ident    = 0;
		citem->Object   = object;
		citem->Size     = size;
		citem->Date     = 0;
		citem->Expires  = 0;
		citem->dtor     = dtor;
		if (item_isMem (citem)) {
			__cache_mem_num++;
			__cache_mem_use += size;
		} else {
			__cache_dsk_num++;
			__cache_dsk_use += size;
		}
		if (__cache_beg) __cache_beg->PrevItem = citem;
		else             __cache_end           = citem;
		citem->PrevItem = NULL;
		citem->NextItem = __cache_beg;
		__cache_beg     = citem;
		
		citem->Cached[0] = '\0';
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
	
	if (item_isMem (citem)) {
		if (citem->Object) {
			(*citem->dtor)(citem->Object);
		}
		__cache_mem_num--;
		__cache_mem_use -= citem->Size;
	
	} else { /* citem is (probably) on disk */
		if (citem->Cached[0]) {
			strcpy (__cache_file, citem->Cached);
			unlink (__cache_path);
		}
		free_location ((LOCATION*)&citem->Object);
		__cache_dsk_num--;
		__cache_dsk_use -= citem->Size;
	}
	free_location (&citem->Location);
	free (citem);
}


/*----------------------------------------------------------------------------*/
static BOOL
cache_throw (long size)
{
	CACHEITEM citem = __cache_end;
	/*BOOL      single;
	if (size > 0) {
		printf ("cache_throw(%li):\n", size);
		single = FALSE;
	} else {
		single = TRUE;
	}*/
	while (citem) {
		CACHEITEM prev = citem->PrevItem;
		if (!citem->Reffs && item_isMem (citem)) {
			/*if (single) {
				printf ("cache_throw(): %li '%s'\n",
				        citem->Size, citem->Location->File);
			} else {
				printf ("%7lu '%s'\n", citem->Size, citem->Location->File);
			}*/
			size -= citem->Size;
			destroy_item (citem);
			if (size <= 0) return TRUE;
		}
		citem = prev;
	}
	/*if (single) {
		puts ("cache_throw(): giving up");
	} else {
		puts ("    ... giving up");
	}*/
	return FALSE;
}


/*============================================================================*/
CACHED
cache_insert (LOCATION loc, long ident,
              CACHEOBJ * object, size_t size, void (*dtor)(void*))
{
	CACHEITEM citem;
	
	if (!__cache_mem_max) {
		cache_setup (NULL,0,0,0);
	}
	if (__cache_mem_use + size > __cache_mem_max) {
		cache_throw (__cache_mem_use + size - __cache_mem_max);
	}
	
	if ((citem = create_item (loc, *object, size, dtor)) == NULL) {
		return NULL;
	}
	citem->Ident = ident;
	*object      = NULL;
	
	return citem->Object;
}


/*----------------------------------------------------------------------------*/
static LOCATION
cache_location (CACHEITEM citem)
{
	if (!citem->Object && citem->Cached[0]) {
		citem->Object = new_location (citem->Cached, __cache_dir);
	}
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
		return (ident ? citem->Object : (CACHED)cache_location (citem));
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
					if (item_isMem (citem)) {
						destroy_item (citem);
					} else if (!citem->Cached[0]) {
						puts ("cache_release(): item is busy!");
					} else {
						destroy_item (citem);
						cache_flush (NULL, __cache_dir);
					}
				}
				*p_object = NULL;
				break;
			}
			citem = citem->NextItem;
		}
	}
	if (__cache_mem_use > __cache_mem_max) {
		cache_throw (__cache_mem_use - __cache_mem_max);
	}
	return object;
}


/*============================================================================*/
size_t
cache_clear (CACHED this_n_all)
{
	size_t    num   = 0;
	size_t    dsk   = 0;
	CACHEITEM citem = __cache_beg;

	while (citem) {
		CACHEITEM next = citem->NextItem;
		if (!citem->Reffs && (item_isMem (citem) || citem->Cached[0])
		    && (!this_n_all || this_n_all == citem->Object)) {
			if (!item_isMem (citem)) {
				if (!citem->Cached[0]) {
					citem = next;
					continue;   /* skip this, busy item */
				}
				dsk++;
			}
			destroy_item (citem);
			num++;
			if (this_n_all) break;
		}
		citem = next;
	}
	if (dsk) {
		cache_flush (NULL, __cache_dir);
	}
	return num;
}


/*============================================================================*/
size_t
cache_info (size_t * size, CACHEINF * p_info)
{
	size_t num = __cache_mem_num + __cache_dsk_num;
	if (p_info) {
		CACHEINF info = (num ? malloc (sizeof(struct s_cache_info) * num) : NULL);
		if ((*p_info = info) != NULL) {
			size_t    n     = num;
			CACHEITEM citem = __cache_beg;
			do {
				info->Source  = citem->Location;
				info->Ident   = citem->Ident;
				info->Used    = citem->Reffs;
				info->Cached  = (citem->Cached[0] ? citem->Cached : NULL);
				info->Local   = (item_isMem (citem) ? NULL : citem->Object);
				info->Date    = citem->Date;
				info->Expires = citem->Expires;
				info->Object  = citem->Object;
				info->Size    = citem->Size;
				citem = citem->NextItem;
				info++;
			} while (--n);
		}
	}
	if (size) {
		*size = __cache_mem_use /*+ __cache_dsk_use*/;
	}
	return num;
}


/*----------------------------------------------------------------------------*/
static BOOL
cache_flush (CACHEITEM citem, LOCATION loc)
{
	FILE * file;
	BOOL   single;
	
	if (citem) {
		file   = fopen (loc->FullName, "rb+");
		single = TRUE;
	} else {
		citem  = __cache_end;
		file   = fopen (loc->FullName, "wb");
		single = FALSE;
	}
	if (!file) {
		puts ("cache_flush(): open failed.");
		return FALSE;
	}
	if (!__cache_dsk_num) {
		__cache_fid = 1;
	}
	fprintf (file, "%08lX\n", __cache_fid);
	if (single) {
		fseek (file, 0, SEEK_END);
	} else {
		location_wrIdx (NULL, NULL); /* reset location database */
	}
	while (citem) {
		if (!item_isMem (citem) && citem->Cached[0]) {
			if (location_wrIdx (file, citem->Location)) {
				fprintf (file, "$%08lX$%08lX$%08lX$/%s\n",
				         citem->Size, citem->Date, citem->Expires, citem->Cached);
				if (single) break;
			}
		} else if (single) {
			puts ("cache_flush(): invalid item.");
			break;
		}
		citem = citem->PrevItem;
	}
	fclose (file);
	
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static void
exit_flush (void)
{
	CACHEITEM citem = __cache_beg;
	BOOL      flush = FALSE;
	time_t    locl  = time (NULL);
	while (citem && citem->Object) {
		CACHEITEM next = citem->NextItem;
		if (!item_isMem(citem) && citem->Expires && citem->Expires < locl) {
			destroy_item (citem);
			flush = TRUE;
		}
		citem = next;
	}
	if (flush) {
		cache_flush (NULL, __cache_dir);
	}
}

/*----------------------------------------------------------------------------*/
static long
read_hex (char ** ptr)
{
	char * p = (**ptr == '$' ? *ptr + 1 : NULL);
	long   n = (p ? strtol (p, &p, 16) : -2);
	if (p > *ptr +1 && *p == '$') *ptr = p;
	return n;
}

/*============================================================================*/
void
cache_setup (const char * dir, size_t mem_max, size_t dsk_max, size_t dsk_lim)
{
	if (!dir && !mem_max && !dsk_max && !dsk_lim) {
		puts ("Setting cache defaults:");
		if (!__cache_mem_max) {
			mem_max = CACHE_MAX;
			printf ("  Memory: %lu bytes\n", mem_max);
		}
		if (!__cache_dir) {
			puts (" Disk: (diabled)");
		} else if (!__cache_dsk_max) {
			dsk_max = 2L*1024*1024;
			dsk_lim = 200;
			printf ("  Disk: %lu bytes, %lu files.\n", dsk_max, dsk_lim);
		}
	}
	
	if (mem_max) {
		if ((long)(__cache_mem_max = (long)Malloc (-1) /2) < 0) {
			__cache_mem_max = 0;
		} else if (__cache_mem_max > mem_max) {
			__cache_mem_max = mem_max;
		}
/*		printf ("cache mem %lu\n", __cache_mem_max);*/
	}
	
	if (dsk_max) {
		__cache_dsk_max = dsk_max;
		__cache_dsk_lim = (dsk_lim ? dsk_lim : 500);
	}
	
	if (dir && *dir && !__cache_dir) {
		LOCATION loc  = NULL;
		FILE   * file = NULL;
		size_t   plen = strlen (dir);
		if ((__cache_path = malloc (plen +12)) != NULL) {
			memcpy (__cache_path, dir, plen);
			if (__cache_path[plen-1] != '/' && __cache_path[plen-1] != '\\') {
				__cache_path[plen++] = (strchr (__cache_path, '/') ? '/' : '\\');
			}
			__cache_file = __cache_path + plen;
			strcpy (__cache_file, "cache.idx");
			loc = new_location (__cache_path, NULL);
		}
		if (loc) {
			char hdr[16];
			if ((file = fopen (__cache_path, "rb")) != NULL
			    && fgets (hdr, (int)sizeof(hdr) -1, file)) {
				char * p;
				long num = strtol (hdr, &p, 16);
				if ((num > 0) && (p == hdr +8) && ((*p == '\n') || (*p == '\r'))) {
					__cache_fid = num;
					__cache_dir = loc;
				} else {
					fclose (file);
					file = NULL;
				}
			} else if (cache_flush (NULL, loc)) {
				__cache_dir = loc;
			} else {
				free_location (&loc);
			}
		}
		if (file) {
			time_t locl = time (NULL);
			long   ndel = 0;
			while (!feof (file)) {
				char buf[1024];
				loc = location_rdIdx (file);
				if (!fgets (buf, (int)sizeof(buf) -1, file)) {
					if (loc) {
						puts ("cache_setup(): idx truncated.");
						free_location (&loc);
					}
				} else {
					char * ptr  = buf;
					long   size = read_hex (&ptr);
					long   date = read_hex (&ptr);
					long   expr = read_hex (&ptr);
					size_t len  = strlen   (++ptr);
					while (len && isspace (ptr[len-1])) ptr[--len] = '\0';
					if (size < 0 || date < 0 || expr < -1) {
						puts ("cache_setup(): idx corrupted.");
						free_location (&loc);
						break;
					} else if (!loc) { /* outdated or invalid */
						strcpy (ptr +1, __cache_file);
						unlink (__cache_path);
						ndel++;
					} else if (expr && expr <= locl) { /* Expired */
						free_location (&loc);
						strcpy (ptr +1, __cache_file);
						unlink (__cache_path);
						ndel++;
					} else {
						CACHEITEM item = create_item (loc, NULL, size,
						                              (void(*)(void*))0);
						if (!item) {
							puts ("cache_setup(): create failed.");
							free_location (&loc);
							break;
						} else {
							strcpy (item->Cached, ptr +1);
							item->Date    = date;
							item->Expires = expr;
							item->Reffs--;
						}
					}
				}
			}
			fclose (file);
			if (ndel) {
				cache_flush (NULL, __cache_dir);
			}
			atexit (exit_flush);
		}
	}
	
	if (__cache_dir && __cache_dsk_max) {
		if ((__cache_dsk_num > __cache_dsk_lim ||
		     __cache_dsk_use > __cache_dsk_max) 
		    && cache_remove (__cache_dsk_num - __cache_dsk_lim,
			                  __cache_dsk_use - __cache_dsk_max)) {
			cache_flush (NULL, __cache_dir);
		}
	}
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
	
/*>>>>>>>>>> DEBUG */
	} else if (citem->Object || citem->Cached[0]) {
		printf ("cache_abort(%s): not busy!\n", loc->FullName);
	
	} else if (citem->Size) {
		printf ("cache_abort(%s): has size %lu!\n", loc->FullName, citem->Size);
	
/*<<<<<<<<<< DEBUG */
	} else {
	/*	printf ("cache_abort(%s)\n", loc->FullName);*/
		destroy_item (citem);
	}
}

/*----------------------------------------------------------------------------*/
static BOOL
cache_remove (long num, long use)
{
	long      cnt   = 0;
	CACHEITEM citem = __cache_end;
	while (citem) {
		CACHEITEM prev = citem->PrevItem;
		if (!item_isMem (citem) && !citem->Reffs && citem->Cached[0]) {
			use -= citem->Size;
			num--;
			destroy_item (citem);
			cnt++;
			if (num <= 0 && use <= 0) break;
		}
		citem = prev;
	}
	return (cnt > 0);
}

/*============================================================================*/
LOCATION
cache_assign (LOCATION src, void * data, size_t size,
              const char * type, long date, long expires)
{
	LOCATION  loc   = NULL;
	CACHEITEM citem = tree_item (src);
	
	if (!citem) {
		printf ("cache_assign(%s): not found!\n", src->FullName);
	
	} else if (citem->Object) {
		printf ("cache_assign(%s): already in use!\n", src->FullName);
	
	} else {
		if (__cache_dir) {
			char   buf[1024];
			int    fh;
			char * p = citem->Cached;
			short  n = 15; /* 5bit * (4 -1) = one million possible files, */
			do {           /* should be enough                            */
				static const char * base32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
				*(p++) = base32[(__cache_fid >>n) & 0x1F];
			} while ((n -= 5) >= 0);
			if (type && *type) {
				*(p++) = '.';
				strcpy (p, type);
			} else {
				*p = '\0';
			}
			if (!__cache_dsk_max) {
				cache_setup (NULL,0,0,0);
			}
			if (__cache_dsk_num >= __cache_dsk_lim ||
			    __cache_dsk_use + size > __cache_dsk_max) {
				cache_remove (__cache_dsk_num +1 - __cache_dsk_lim,
				              __cache_dsk_use + size - __cache_dsk_max);
				n = TRUE;
			} else {
				n = FALSE;
			}
			loc = new_location (citem->Cached, __cache_dir);
			location_FullName (loc, buf, sizeof(buf));
			if ((fh = open (buf, O_RDWR|O_CREAT|O_TRUNC, 0666)) >= 0) {
				write (fh, data, size);
				close (fh);
				__cache_fid++;
				citem->Object  = location_share (loc);
				citem->Size    = size;
				citem->Date    = date;
				citem->Expires = expires;
				citem->Reffs--;
				__cache_dsk_use += size;
				cache_flush ((n ? NULL : citem), __cache_dir);
			} else {
				free_location (&loc);
			}
		}
		if (!loc) { /* either no cache dir set or file couldn't be written */
			citem->Cached[0] = '\0';
			destroy_item (citem);
		}
	}
	return loc;
}

/*============================================================================*/
void
cache_expires (LOCATION loc, long date)
{
	CACHEITEM citem = tree_item (loc);
	if (!citem) {
		printf ("cache_expires(%s): not found!\n", loc->FullName);
	} else if (date > 0 || !citem->Expires) {
		citem->Expires = date;
		cache_flush (NULL, __cache_dir);
	}
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
	info->Cached  = NULL;
	info->Local   = NULL;
	info->Date    = 0;
	info->Expires = 0;
	info->Object  = NULL;
	info->Size    = 0;
	
	while (citem) {
		if (location_equal (loc, citem->Location)) {
			if (!item_isMem (citem)) {
				if (citem->Cached[0]) {
					info->Cached  = citem->Cached;
					info->Local   = cache_location (citem);
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
