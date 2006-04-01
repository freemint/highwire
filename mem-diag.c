/*******************************************************************************
 *
 * Memory Diagnostics
 *
 *******************************************************************************
*/
#define MD_CHKBND  /* enable memory region boundary check               */
#define MD_SUMMARY /* show orphaned memory blocks after program end     */
#undef MD_KEEPALL /* keeps information about all memory ever allocated */


#ifdef __GNUC__
#include <stdlib.h>
#include <stdio.h>

#include "hw-types.h"


LONG mvalidate (void * mem);


/******************************************************************************/

typedef struct s_mem_item * MEM_ITEM;
struct s_mem_item {
	void * Chunk;
	size_t Size;
	ULONG  Created;
	ULONG  Deleted;
};


typedef struct s_tree_node * TREE_NODE;
typedef struct s_tree_item * TREE_ITEM;

typedef struct {
	TREE_NODE Node;
	TREE_ITEM Item;
} TREE_ITER;

static void * tree_item (TREE_NODE *, TREE_ITEM, long mode);
static void * tree_next (TREE_ITER *);

static TREE_NODE actv_base = NULL;
static TREE_NODE free_base = NULL;


/*----------------------------------------------------------------------------*/
static inline ULONG ptr2offs (void * ptr)
{
	extern void * _base;
	return (ULONG)(((ULONG*)ptr)[-1] - (ULONG)_base -0x100);
}

/*----------------------------------------------------------------------------*/
#ifdef MD_CHKBND
#define CANARY     8
#define CANARY_1   0xA5
#define CANARY_4   0xA5A5A5A5uL
#define CHUNKSIZE(s)   (s+CANARY*2)
#define CHUNK2MEM(c)   ((char*)(c)+CANARY)
#define MEM2CHUNK(m)   ((char*)(m)-CANARY)

/*............................................................................*/
static UWORD canary_chk (MEM_ITEM item)
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

/*............................................................................*/
static const char * canary_str (UWORD patt, size_t size)
{
	static char buff[40];
	char * p   = buff;
	UWORD  bit = 0x8000;
	while(1) {
		if      (bit & 0x8000) *(p++) = '|';
		else if (bit & 0x0808) *(p++) = ':';
		else if (bit & 0x0080) {
			sprintf (p, "|%li|", size);
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

#else /*- !MD_CHKBND ---------------------------------------------------------*/
#define CANARY     0
#define CHUNKSIZE(s)   (s)
#define CHUNK2MEM(c)   ((char*)c)
#define MEM2CHUNK(m)   ((char*)m)
#endif


/*----------------------------------------------------------------------------*/
static MEM_ITEM
mem_find (TREE_NODE base, void * chunk)
{
	TREE_ITER iter = { base, NULL };
	MEM_ITEM  item;
	while ((item = tree_next (&iter))) {
		if ((char*)chunk >= (char*)item->Chunk &&
		    (char*)chunk <  (char*)item->Chunk + item->Size + CANARY *2) {
			break;
		}
	}
	return item;
}

/*----------------------------------------------------------------------------*/
#if defined(MD_SUMMARY) || defined(MD_CHKBND)
static void summarize (void)
{
#ifdef MD_SUMMARY
	size_t    size = 0;
	ULONG     num  = 0;
#endif
	TREE_ITER iter = { actv_base, NULL };
	MEM_ITEM  item;
	while ((item = tree_next (&iter))) {
#ifdef MD_CHKBND
		UWORD patt = canary_chk (item);
		if (patt) {
			printf ("canary violated in %p!  %s created @<%05lX>\r\n",
			        CHUNK2MEM (item->Chunk), canary_str (patt, item->Size),
			        item->Created);
		}
#endif
#ifdef MD_SUMMARY
		size += item->Size;
		num  += 1;
#endif
	}
#ifdef MD_SUMMARY
	puts ("\r\n_Memory_Summary_________________________"
	        "________________________________________");
	while ((item = tree_next (&iter))) if (item->Created) {
		ULONG     crtd = item->Created;
		size_t    sum  = item->Size;
		ULONG     cnt  = 1;
		TREE_ITER subi = iter;
		while ((item = tree_next (&subi))) {
			if (item->Created == crtd) {
				sum     += item->Size;
				cnt     += 1;
				item->Created = 0uL;
			}
		}
		printf ("@<%05lX>: %4li (%li)\r\n", crtd, cnt, sum);
	}
	if (num) {
		puts ("----------------------------------------"
		      "----------------------------------------");
	}
	printf ("%7li bytes in %li blocks.\r\n", size, num);
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
		if ((item = tree_item (&free_base, (TREE_ITEM)&chunk, -1))) {
/*			printf ("malloc(%li) @<%05lX>: %p duplicate %li.\r\n",
			        size, ptr2offs(&size), CHUNK2MEM(chunk), item->Size);
*/			if (!tree_item (&actv_base, (TREE_ITEM)item, 1)) {
				__free (item);
				item = NULL;
			}
		} else {
			item = tree_item (&actv_base, (TREE_ITEM)&chunk,
			                  sizeof (struct s_mem_item));
		}
		if (!item) {
			__free (chunk);
			return NULL;
		} else {
/*			printf ("malloc(%li) @<%05lX>: %p.\r\n",
			         size, ptr2offs (&size), CHUNK2MEM(chunk));
*/		}
	}
#ifdef MD_CHKBND
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
	
#if defined(MD_SUMMARY) || defined(MD_CHKBND)
	{	static BOOL __once = TRUE;
		if (__once) {
#ifdef MD_SUMMARY
			extern BOOL mem_TidyUp;
			mem_TidyUp = TRUE;
#endif
			__once = FALSE;
			atexit (summarize);
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
	MEM_ITEM   item  = tree_item (&actv_base, (TREE_ITEM)&chunk, -1);
	if (item) {
#ifdef MD_CHKBND
		UWORD patt = canary_chk (item);
		if (patt) {
			printf ("free(%p) @<%05lX>: canary violated!\r\n", mem, ptr2offs (&mem));
			printf ("   %s created @<%05lX>.\r\n",
			        canary_str (patt, item->Size), item->Created);
		}
#endif
/*		printf ("free(%p) @<%05lX>: %li.\r\n", mem, ptr2offs (&mem), item->Size);*/
#ifdef MD_KEEPALL
		if (tree_item (&free_base, (TREE_ITEM)item, +1)) {
			item->Deleted = ptr2offs (&mem);
		} else
#endif
		{
			__free (item); /* bad memory */
		}
		__free (chunk);
	} else 
if (ptr2offs (&mem) < 0x3F000)  /* don't show known bugs in libungif */
	{
#ifdef MD_CHKBND
		if ((item = tree_item (&free_base, (TREE_ITEM)&chunk, 0))) {
			printf ("free(%p) @<%05lX>: already deleted!\r\n", mem, ptr2offs (&mem));
			printf ("   %li bytes created @<%05lX>, deleted @<%05lX>.\r\n",
			        item->Size, item->Created, item->Deleted);
		} else
#endif
		{
			printf ("free(%p) @<%05lX>: invalid pointer!\r\n", mem, ptr2offs (&mem));
		}
	}
}

/*============================================================================*/
LONG
mvalidate (void * mem)
{
	BOOL     actv;
	char   * chunk = MEM2CHUNK(mem);
	MEM_ITEM item  = tree_item (&actv_base, (TREE_ITEM)&chunk, 0);
	if (item) {
		/* everything is fine */
		return 0;
	}
	if ((item = tree_item (&free_base, (TREE_ITEM)&chunk, 0))) {
		printf ("mvalidate(%p) @<%05lX>: already deleted!\r\n",
		        mem, ptr2offs (&mem));
		printf ("   %li bytes created @<%05lX>, deleted @<%05lX>.\r\n",
		        item->Size, item->Created, item->Deleted);
		return -2;
	}
	if ((item = mem_find (actv_base, chunk))) {
		actv = TRUE;
	} else {
		item = mem_find (free_base, chunk);
		actv = FALSE;
	}
	if (item) {
		const char * text;
		long n = 0;
		chunk  = item->Chunk;
		if ((n = (chunk + CANARY) - (char*)mem) > 0) {
			text = "before";
		} else if ((n = (char*)mem - (chunk + CANARY + item->Size -1)) > 0) {
			text = "behind";
		} else {
			n    = (char*)mem - (chunk + CANARY);
			text = "inside";
		}
		printf ("mvalidate(%p) @<%05lX>:%s %li bytes %s %p\r\n",
		        mem, ptr2offs (&mem), (actv ? "" : " invalid,"), n, text, chunk);
		printf (actv ? "   %li bytes created @<%05lX>.\r\n"
		             : "   %li bytes created @<%05lX>, deleted @<%05lX>.\r\n",
		        item->Size, item->Created, item->Deleted);
		return (actv ? -3 : -1);
	}
	printf ("mvalidate(%p) @<%05lX>: invalid pointer!\r\n",
	        mem, ptr2offs (&mem));
	return -1;
}


/*******************************************************************************
 *
 * Tree stuff
 *
*/

struct s_tree_item {
	ULONG Hash;
};

typedef union {
	TREE_NODE Node;
   TREE_ITEM Item;
} TREE_SLOT;

struct s_tree_node {
	TREE_NODE Parent;
	UWORD     Mask;
	TREE_SLOT Slot[16];
};

static TREE_NODE node_pool = NULL; /* free list */

/*----------------------------------------------------------------------------*/
static void *
tree_item (TREE_NODE * base, TREE_ITEM item, long mode)
{
	/* mode == 0: search only, returns item or NULL
	 *       < 0: remove item from tree and return it
	 *      >= 4: create new item of size 'mode' in tree
	 *      1..3: insert given item in tree
	*/
	UWORD     dpth = 0;
	ULONG     patt = item->Hash;
	WORD      num  = patt & 0xF;
	TREE_NODE node = *base;
	if (!node) {
		if (mode > 0) {
			if ((node = node_pool)) node_pool = node->Parent;
			else                    node = __malloc (sizeof (struct s_tree_node));
		}
		if (!node) {
			return NULL;
		}
		*base = memset (node, 0, sizeof (struct s_tree_node));
	}
	while (node) {
		WORD        bit  = 1 << num;
		TREE_SLOT * slot = &node->Slot[num];
		
		if (node->Mask & bit) { /*............................. follow the node */
			node =  slot->Node;
			dpth += 1;
		
		} else if (!slot->Item) { /*.......................... empty slot found */
			if (mode <= 0) return NULL;
			else           break;
			
		} else if (slot->Item->Hash == item->Hash) { /*............ match found */
			if (mode >= 0) return slot->Item;
			else           break;
		
		} else if (mode == 0) { /*............................... nothing found */
			return NULL;
		
		} else { /*.......................................... insert a new node */
			TREE_NODE prnt = node;
			if ((node = node_pool)) node_pool = node->Parent;
			else                    node = __malloc (sizeof (struct s_tree_node));
			if (node) {
				memset (node, 0, sizeof (struct s_tree_node));
				dpth += 1;
				num  =  (slot->Item->Hash >> (dpth *4)) & 0xF;
				node->Slot[num].Item = slot->Item;
				node->Parent         = prnt;
				slot->Node =  node;
				prnt->Mask |= bit;
			}
			/* else memory exhausted! */
		}
		patt >>= 4;
		num  =   patt & 0xF;
	}
	if (!node) { /* memory exhausted or implementation failure */
		puts ("KRABUMM!!!");
		return NULL;
	}
	if (mode >= 4) { /* new item to create */
		ULONG hash = item->Hash;
		if ((item = __malloc (mode))) {
			item->Hash = hash;
		} else {
			mode = -1; /* to remove all recently allocated nodes */
		}
	}
	if (mode > 0) {
		node->Slot[num].Item = item;
		
	} else { /* (mode < 0) */
		item = node->Slot[num].Item;
		node->Slot[num].Item = NULL;
		while (node->Parent && !node->Mask) {
			TREE_NODE prnt = node->Parent;
			for (num = 0; num < numberof(node->Slot) && prnt; num++) {
				if (node->Slot[num].Item) prnt = NULL; /* not empty */
			}
			if (!prnt) break;
			for (num = 0; num < numberof(prnt->Slot) && node; num++) {
				if (prnt->Slot[num].Node == node) {
					prnt->Slot[num].Node = NULL;
					prnt->Mask          &= ~(1 << num);
					node->Parent = node_pool;
					node_pool    = node;
					node         = NULL;
				}
			}
			if (node || !(node = prnt)) break;
		}
	}
	return item;
}

/*----------------------------------------------------------------------------*/
static void *
tree_next (TREE_ITER * iter)
{
	TREE_ITEM item;
	TREE_NODE node;
	if (!iter || !iter->Node) {
		return NULL;
	}
	
	item = iter->Item;
	node = iter->Node;
	while(1) {
		WORD num = 0;
		if (item) {
			while (item != node->Slot[num++].Item && num < 16);
		}
		if (num >= 16) {
			item = NULL;
		} else {
			while (!(item = node->Slot[num].Item) && ++num < 16);
		}
		if (!item) {
			if (!node->Parent) {
				break;
			}
			item = (TREE_ITEM)node;
			node = node->Parent;
			
		} else if (node->Mask & (1 << num)) {
			item = NULL;
			node = node->Slot[num].Node;
		
		} else {
			break;
		}
	}
	iter->Item = item;
	iter->Node = node;
	
	return item;
}

#endif __GNUC__
