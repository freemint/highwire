#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "file_sys.h"
#include "global.h"
#include "Location.h"
#include "cache.h"


#define MAGIC_NUM 0x20040824l /* needs to get updated in case the format of *
                               * the cache.idx file changes                 */

static const char base32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";


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
static WORD      __cache_changed = FALSE;
static LOCATION  __cache_dir  = NULL;
static char    * __cache_path = NULL;
static char    * __cache_file = NULL;
static long      __cache_fid = 1;


/* functions for cache directory handling */
static BOOL cache_flush  (CACHEITEM);
static void cache_build  (void);
static BOOL cache_remove (long num, long use);

/* search tree handling */
static CACHEITEM * tree_slot (LOCATION);
static CACHEITEM   tree_item (LOCATION);
static CACHENODE tree_insert (CACHEITEM, LOCATION);
static void      tree_remove (CACHEITEM);


/*******************************************************************************
 *
 *   General setup and information.
*/

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
			puts (" Disk: (disabled)");
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
		size_t plen = strlen (dir);
		if ((__cache_path = malloc (plen +14)) != NULL) {
			memcpy (__cache_path, dir, plen);
			if (__cache_path[plen-1] != '/' && __cache_path[plen-1] != '\\') {
				__cache_path[plen++] = (strchr (__cache_path, '/') ? '/' : '\\');
			}
			__cache_file = __cache_path + plen;
			strcpy (__cache_file, "cache.idx");
			if ((__cache_dir = new_location (__cache_path, NULL)) != NULL) {
				cache_build();
			}
		}
	}
	
	if (__cache_dir && __cache_dsk_max
	    && (__cache_dsk_num > __cache_dsk_lim ||
	        __cache_dsk_use > __cache_dsk_max)) {
		BOOL update = cache_remove (__cache_dsk_num - __cache_dsk_lim,
		                            __cache_dsk_use - __cache_dsk_max);
		if (update) {
			cache_flush (NULL);
		}
	}
}

/*============================================================================*/
const char *
cache_DirInfo(void)
{
	return (__cache_dir ? location_Path (__cache_dir, NULL) : NULL);
}


/******************************************************************************/


/*----------------------------------------------------------------------------*/
static CACHEITEM
create_item (LOCATION loc, CACHEOBJ object, size_t size, void (*dtor)(void*))
{
	CACHEITEM citem;
	
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
	
	citem = malloc (sizeof (struct s_cache_item));
	if (citem && tree_insert (citem, loc)) {
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
	
	} else if (citem) {   /* memory exhausted */
		free (citem);
		citem = NULL;
	}
	return citem;
}

/*----------------------------------------------------------------------------*/
static void
destroy_item (CACHEITEM citem)
{
	
	tree_remove (citem);
	
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
			__cache_changed = TRUE;
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
item_reorder (CACHEITEM citem)
{
	if (!citem->PrevItem) {
		return FALSE; /* already at the begin */
	}
	citem->PrevItem->NextItem = citem->NextItem;
	if (citem->NextItem) citem->NextItem->PrevItem = citem->PrevItem;
	else                 __cache_end               = citem->PrevItem;
	__cache_beg->PrevItem = citem;
	citem->PrevItem       = NULL;
	citem->NextItem       = __cache_beg;
	__cache_beg           = citem;
	
	return (__cache_changed = TRUE);
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
cache_insert (LOCATION loc, long ident, long lc_ident,
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
	if (lc_ident) {
		CACHEITEM nitem = citem->NodeNext;
		while (nitem) {
			if (nitem->Cached[0]) {
				if (nitem->Ident) {
					printf ("cache_insert(%s): ident already set.\n", loc->FullName);
				}
				nitem->Ident    = lc_ident;
				__cache_changed = TRUE;
				break;
			}
			nitem = nitem->NodeNext;
		}
		if (!nitem) {
			printf ("cache_insert(%s): local not found.\n", loc->FullName);
		}
	}
	
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
	CACHEITEM * p_array = tree_slot (loc);
	CACHEITEM * p_cache = p_array;
	CACHEITEM * p_found = NULL;
	CACHEITEM   citem;

	if (opt_found) {
		*opt_found = 0;
	}

	while ((citem = *p_cache) != NULL) {
		if (location_equal (loc, citem->Location)) {
			if (ident ? ident == citem->Ident : citem->Cached[0] != '\0') {
				p_found = p_cache;
				break;
			} else if (opt_found /*&& citem->Ident*/) {
				if (citem->Cached[0] == '\0') {
					p_found = p_cache;
				 } else {
				 	*opt_found = citem->Ident;
				 }
			}
		}
		p_cache = &citem->NodeNext;
	}
	if (p_found) {
		citem = *p_found;
		if (p_found != p_array) {
			*p_found        = citem->NodeNext;
			citem->NodeNext = *p_array;
			*p_array        = citem;
		}
		item_reorder (citem);
		if (opt_found) {
			*opt_found = citem->Ident;
		}
		return (ident ? citem->Object : (CACHED)cache_location (citem));
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
						cache_flush (NULL);
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
		cache_flush (NULL);
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
				info->Ident   = (item_isMem (citem) ?citem->Ident :  0ul);
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
			}
			loc = new_location (citem->Cached, __cache_dir);
			location_FullName (loc, buf, sizeof(buf));
			if ((fh = open (buf, O_RDWR|O_CREAT|O_TRUNC|O_RAW, 0666)) >= 0) {
				write (fh, data, size);
				close (fh);
				__cache_fid++;
				citem->Object  = location_share (loc);
				citem->Size    = size;
				citem->Date    = date;
				citem->Expires = expires;
				citem->Reffs--;
				__cache_dsk_use += size;
				cache_flush (citem);
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
		__cache_changed = TRUE;
	}
}

/*============================================================================*/
CRESULT
cache_query (LOCATION loc, long ident, CACHEINF info)
{
	CACHEITEM citem = *tree_slot (loc);
	CACHEITEM found = NULL;
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
					found         = citem;
					info->Cached  = citem->Cached;
					info->Local   = cache_location (citem);
					info->Date    = citem->Date;
					info->Expires = citem->Expires;
					res_d         = CR_LOCAL;
				} else {
					res_d         = CR_BUSY;
				}
				if (!info->Ident) {
					info->Ident  = citem->Ident;
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
	if (found) {
		item_reorder (found);
	}
	return (res_d | res_m);
}


/*******************************************************************************
 *
 *   Functions for cache directory handling.
*/

/*------------------------------------------------------------------------------
 * Write the index.cfg file.
*/
static BOOL
cache_flush (CACHEITEM citem)
{
	FILE * file;
	BOOL   single;
	
	if (citem && !__cache_changed) {
		file   = fopen (__cache_dir->FullName, "rb+");
		single = TRUE;
	} else {
		citem  = __cache_end;
		file   = fopen (__cache_dir->FullName, "wb");
		single = FALSE;
	}
	if (!file) {
		puts ("cache_flush(): open failed.");
		return FALSE;
	}
	if (!__cache_dsk_num) {
		__cache_fid = 1;
	}
	fprintf (file, "%08lX:%08lX\n", MAGIC_NUM, __cache_fid);
	if (single) {
		fseek (file, 0, SEEK_END);
	} else {
		location_wrIdx (NULL, NULL); /* reset location database */
	}
	while (citem) {
		if (!item_isMem (citem) && citem->Cached[0]) {
			if (location_wrIdx (file, citem->Location)) {
				fprintf (file, "$%08lX$%08lX$%08lX$%08lX$/%s\n", citem->Ident,
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
	__cache_changed = FALSE;
	
	return TRUE;
}

/*------------------------------------------------------------------------------
 * Exit handler
*/
static void
_exit_flush (void)
{
	CACHEITEM citem = __cache_beg;
	time_t    locl  = time (NULL);
	while (citem && citem->Object) {
		CACHEITEM next = citem->NextItem;
		if (!item_isMem(citem) && citem->Expires && citem->Expires < locl) {
			destroy_item (citem);
		}
		citem = next;
	}
	if (__cache_changed == TRUE) {
		cache_flush (NULL);
	}
}

/*----------------------------------------------------------------------------*/
static long
read_hex (char ** ptr)
{
	char * p = (**ptr == '$' ? *ptr + 1 : NULL);
	long   n = (p ? strtoul (p, &p, 16) : -2);
	if (p > *ptr +1 && *p == '$') *ptr = p;
	return n;
}

/*----------------------------------------------------------------------------*/
static void
clear_dir (void)
{
	DTA * old = Fgetdta(), dta;
	Fsetdta (&dta);
	strcpy (__cache_file, "*.*");
	if (Fsfirst (__cache_path, 0x0000) == E_OK) do {
		if ((strchr (base32, toupper (dta.d_fname[0])) &&
		     strchr (base32, toupper (dta.d_fname[1])) &&
		     strchr (base32, toupper (dta.d_fname[2])) &&
		     strchr (base32, toupper (dta.d_fname[3])) &&
		     (!dta.d_fname[4] || (dta.d_fname[4] == '.')))
		    || stricmp (dta.d_fname, "cache.idx") == 0) {
			strcpy  (__cache_file,  dta.d_fname);
			Fdelete (__cache_path);
		}
	} while (Fsnext() == E_OK);
	Fsetdta (old);
}

/*------------------------------------------------------------------------------
 * Read the index.cfg file and create all cache structures.
*/
static void
cache_build (void)
{
	FILE * file = fopen (__cache_path, "rb");
	if (file) {
		char hdr[20] = "", * p = hdr;
		if (!(fgets (hdr, (int)sizeof(hdr) -1, file)
		      && strtoul (p, &p, 16) == MAGIC_NUM && *(p++) == ':'
		      && (__cache_fid = strtoul (p, &p, 16)) > 0 && p == hdr +17
		      && (hdr[17] == '\n' || hdr[17] == '\r'))) {
			__cache_fid = 0l;
			fclose (file);
			file = NULL;
			if (hdr[0]) {
				hwUi_info ("cache::build()", " \n"
				           "The cache is out of date and will be cleared now.\n"
				           "This might need some time.\n");
			}
			clear_dir();
		}
	}
	
	if (file) {
		time_t locl = time (NULL);
		while (!feof (file)) {
			char buf[1024];
			LOCATION loc = location_rdIdx (file);
			if (!fgets (buf, (int)sizeof(buf) -1, file)) {
				if (loc) {
					puts ("cache_setup(): idx truncated.");
					free_location (&loc);
				}
			} else {
				char * ptr  = buf;
				long  ident = read_hex (&ptr);
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
					strcpy (__cache_file, ptr +1);
					unlink (__cache_path);
					__cache_changed = TRUE;
				
				} else if (expr && expr <= locl) { /* Expired */
					free_location (&loc);
					strcpy (__cache_file, ptr +1);
					unlink (__cache_path);
					__cache_changed = TRUE;
				
				} else {
					CACHEITEM item = create_item (loc, NULL, size,(void(*)(void*))0);
					if (!item) {
						puts ("cache_setup(): create failed.");
						free_location (&loc);
						break;
					} else {
						strcpy (item->Cached, ptr +1);
						item->Ident   = ident;
						item->Date    = date;
						item->Expires = expr;
						item->Reffs--;
					}
				}
			}
		}
		fclose (file);
	
	} else { /* !file */
		__cache_changed = TRUE;
	}
	
	if (__cache_changed) {
		if (!cache_flush (NULL)) {
			free_location (&__cache_dir);
			__cache_changed = FALSE;
		} else {
			atexit (_exit_flush);
		}
	}
}

/*------------------------------------------------------------------------------
 * Delete files to meet the cache limits.
*/
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


/*******************************************************************************
 *
 *   Search tree handling (sorted by LOCATION)
*/

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

/* root node, take care that ALL elements are zero-ed at startup! */
static struct s_cache_node __tree_base = {
	NULL, 0x0000, 0, 0,
	{	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
		{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL} }
};

/*----------------------------------------------------------------------------*/
static CACHENODE
tree_node (ULONG hash, UWORD * p_idx)
{
	CACHENODE node = &__tree_base;
	UWORD     idx  = hash & 0xF;
	while (node->NodeMask & (1 << idx)) { /* go through the tree */
		node = node->Array[idx].Node;
		idx  = (hash >>= 4) & 0xF;
	}
	*p_idx = idx;
	return node;
}

/*------------------------------------------------------------------------------
 * Return the pointer to the linked list where the location belongs to.
*/
static CACHEITEM *
tree_slot (LOCATION loc)
{
	UWORD     idx;
	CACHENODE node = tree_node (location_Hash (loc), &idx);
	return &node->Array[idx].Item;
}

/*------------------------------------------------------------------------------
 * Returns the cache item fro the tree.
*/
static CACHEITEM
tree_item (LOCATION loc)
{
	UWORD     idx;
	CACHENODE node  = tree_node (location_Hash (loc), &idx);
	CACHEITEM citem = node->Array[idx].Item;
	while (citem && (item_isMem (citem) ||
	                 !location_equal (loc, citem->Location))) {
		citem = citem->NodeNext;
	}
	return citem;
}

/*------------------------------------------------------------------------------
 * Insert the cache item in the search tree and expand the tree if necessary.
*/
static CACHENODE
tree_insert (CACHEITEM citem, LOCATION loc)
{
	UWORD     idx;
	ULONG     hash  = location_Hash (loc);
	CACHENODE node  = tree_node (hash, &idx);
	CACHEITEM nitem = node->Array[idx].Item;
	
	if (nitem && nitem->NodeHash != hash) {
		do {
			CACHENODE back = node;
			CACHEITEM temp;
			if ((node = malloc (sizeof(struct s_cache_node))) != NULL) {
				back->Array[idx].Node = node;
				back->NodeMask       |= (1 << idx);
				node->BackNode = back;
				node->NodeMask = 0x0000;
				node->Level4x  = back->Level4x +4;
				node->Filled   = 1;
				memset (node->Array, 0, sizeof(node->Array));
				node->Array[(nitem->NodeHash >> node->Level4x) & 0xF].Item = nitem;
				temp = nitem;
				do {
					temp->Node = node;
				} while ((temp = temp->NodeNext) != NULL);
				idx = (hash >> node->Level4x) & 0xF;
			}
		} while (node->Array[idx].Item);
	}
	if (node) {
		citem->Node          = node;
		citem->NodeHash      = hash;
		if ((citem->NodeNext = node->Array[idx].Item) == NULL) {
			node->Filled++;
		}
		node->Array[idx].Item = citem;
	}
	return node;
}

/*------------------------------------------------------------------------------
 * Remove cache item from the search tree and remove empty nodes.
*/
static void
tree_remove (CACHEITEM citem)
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
}
