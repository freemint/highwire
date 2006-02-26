/*******************************************************************************
 *
 * Memory Diagnostics
 *
 *******************************************************************************
*/
#ifdef __GNUC__
#include <stdlib.h>
#include <stdio.h>

#include "hw-types.h"

#define CHKBND /* enable memory region boundary check           */
#undef SUMARY /* show orphaned memory blocks after program end */


typedef struct s_mem_item * MEM_ITEM;
struct s_mem_item {
	MEM_ITEM Next;
	void   * Chunk;
	size_t   Size;
	ULONG    Created;
	ULONG    Deleted;
};
static MEM_ITEM actv_lst = NULL;
static MEM_ITEM free_lst = NULL;


/*----------------------------------------------------------------------------*/
static inline ULONG ptr2offs (void * ptr)
{
	extern void * _base;
	return (ULONG)(((ULONG*)ptr)[-1] - (ULONG)_base -0x100);
}

/*----------------------------------------------------------------------------*/
static MEM_ITEM *
mem_item (MEM_ITEM * plist, void * chunk)
{
	while (*plist && (*plist)->Chunk != chunk) {
		plist = &(*plist)->Next;
	}
	return plist;
}

/*----------------------------------------------------------------------------*/
#ifdef CHKBND
#define CANARY     8
#define CANARY_1   0xA5
#define CANARY_4   0xA5A5A5A5uL
#define CHUNKSIZE(s)   (s+CANARY*2)
#define CHUNK2MEM(c)   ((char*)(c)+CANARY)
#define MEM2CHUNK(m)   ((char*)(m)-CANARY)

static UWORD canary (MEM_ITEM item)
{
	UWORD  patt = 0x0000;
	char * can  = item->Chunk;
	if (((ULONG*)can)[0] != CANARY_4 || ((ULONG*)can)[1] != CANARY_4) {
		UWORD bit = 0x8000;
		short i;
		for (i = 0; i < CANARY; i++, bit >>= 1) {
			if (can[i] != CANARY_1) patt |= bit;
		}
	}
	can += CANARY + item->Size;
	if ((item->Size & 3) ||
	    ((ULONG*)can)[0] != CANARY_4 || ((ULONG*)can)[1] != CANARY_4) {
		UWORD bit = 0x0080;
		short i;
		for (i = 0; i < CANARY; i++, bit >>= 1) {
			if (can[i] != CANARY_1) patt |= bit;
		}
	}
	return patt;
}

static const char * canary_str (UWORD patt, size_t size)
{
	static char buff[40];
	char * p   = buff;
	UWORD  bit = 0x8000;
	while(1) {
		if      (bit & 0x8000) *(p++) = '|';
		else if (bit & 0x0808) *(p++) = ':';
		else if (bit & 0x0080) {
			sprintf (p, "|%li %04X|", size, patt);
			while (*(++p));
		}
		*(p++) = (patt & bit ? '*' : '.');
		if (!(bit >>= 1)) {
			*(p++) = '|';
			*(p++) = '\0';
			break;
		}
	}
	return buff;
}

#else /*- !CHKBND ------------------------------------------------------------*/
#define CHUNKSIZE(s)   (s)
#define CHUNK2MEM(c)   ((char*)c)
#define MEM2CHUNK(m)   ((char*)m)
#endif

/*----------------------------------------------------------------------------*/
#if defined(SUMARY) || defined(CHKBND)
static void sumary (void)
{
	size_t   size = 0;
	ULONG    num  = 0;
	MEM_ITEM item = actv_lst;
	while (item) {
#ifdef CHKBND
		UWORD  patt = canary (item);
		if (patt) {
			printf ("canary detected in %p!  %s created @<%05lX>\n",
			        CHUNK2MEM (item->Chunk), canary_str (patt, item->Size),
			        item->Created);
		}
#endif
		size += item->Size;
		num  += 1;
		item =  item->Next;
	}
#ifdef SUMARY
	puts ("\n_Memory_Sumery__________________________"
	        "________________________________________");
	while (actv_lst) {
		MEM_ITEM * pitem = &actv_lst;
		ULONG      crtd  = actv_lst->Created;
		size_t     sum   = 0;
		ULONG      cnt   = 0;
		while ((item = *pitem)) {
			if (item->Created == crtd) {
				sum   += item->Size;
				cnt   += 1;
				*pitem = item->Next;
			} else {
				pitem  = &item->Next;
			}
		}
		printf ("@<%05lX>: %4li (%li)\n", crtd, cnt, sum);
	}
	if (num) {
		puts ("----------------------------------------"
		      "----------------------------------------");
	}
	printf ("%7li bytes in %li blocks.\n", size, num);
#endif
}
#endif

/*============================================================================*/
void *
malloc (size_t size) 
{
	extern void * __malloc(size_t);
	
	MEM_ITEM item;
	void   * chunk = __malloc (CHUNKSIZE(size));
	if (!chunk) {
		return NULL;
	
	} else {
		MEM_ITEM * pitem = mem_item (&free_lst, chunk);	
		if (*pitem) {
			item = *pitem;
/*			printf ("malloc(%li): %p duplette %li @<%05lX>.\n",
			         size, CHUNK2MEM(chunk), item->Size,  ptr2offs (&size));
*/			*pitem = item->Next;
		} else if (!(item = __malloc (sizeof (struct s_mem_item)))) {
			__free (chunk);
			return NULL;
		} else {
/*			printf ("malloc(%li): %p @<%05lX>.\n",
			         size, CHUNK2MEM(chunk), ptr2offs (&size));
*/		}
	}
#ifdef CHKBND
	((ULONG*)chunk)[0] = CANARY_4;
	((ULONG*)chunk)[1] = CANARY_4;
	if (!(size & 3)) {
		char * can = CHUNK2MEM(chunk) + size;
		((ULONG*)can)[0] = CANARY_4;
		((ULONG*)can)[1] = CANARY_4;
	} else {
		char * can = CHUNK2MEM(chunk) + size;
		short  i;
		for (i = 0; i < CANARY; can[i++] = CANARY_1);
	}
#endif
	item->Chunk   = chunk;
	item->Size    = size;
	item->Created = ptr2offs (&size);
	item->Deleted = 0uL;
	item->Next    = actv_lst;
	actv_lst      = item;
	
#if defined(SUMARY) || defined(CHKBND)
	{	static BOOL __once = TRUE;
		if (__once) {
			__once = FALSE;
			atexit (sumary);
		}
	}
#endif
	
	return CHUNK2MEM(chunk);
}

/*============================================================================*/
void
free (void * mem)
{
	extern void __free(void*);
	
	void     * chunk = MEM2CHUNK(mem);
	MEM_ITEM * pitem = mem_item (&actv_lst, chunk);
	MEM_ITEM   item  = *pitem;
	if (item) {
#ifdef CHKBND
		UWORD  patt = canary (item);
		if (patt) {
			printf ("free(%p): canary detected! @<%05lX>\n", mem, ptr2offs (&mem));
			printf ("   %s created @<%05lX>\n",
			        canary_str (patt, item->Size), item->Created);
		}
#endif
/*		printf ("free(%p): %li @<%05lX>\n", mem, item->Size, ptr2offs (&mem));*/
		*pitem        = item->Next;
		item->Next    = free_lst;
		free_lst      = item;
		item->Deleted = ptr2offs (&mem);
		__free (mem);
	} else {
		pitem = mem_item (&free_lst, chunk);
		item  = *pitem;
		if (item) {
			printf ("free(%p): already deleted! @<%05lX>\n", mem, ptr2offs (&mem));
			printf ("   %li bytes created  @<%05lX>, deleted @<%05lX>\n",
			        item->Size, item->Created, item->Deleted);
		} else {
if (ptr2offs (&mem) < 0x3F000)  /* don't show known bugs in libungif */
			printf ("free(%p): invalid pointer! @<%05lX>\n", mem, ptr2offs (&mem));
		}
	}
}

#endif __GNUC__
