/*
 * schedule.c -- Job control.
 *
 * AltF4 - Jan 31, 2002
 *
 */
#include <stddef.h>
#include <stdlib.h>

#include "hw-types.h"
#include "schedule.h"


typedef struct s_SCHED {
	struct s_SCHED * Next;
	struct s_SCHED * Prev;
	long             Priority;
	SCHED_FUNC       func;
	void           * Value;
	long             Hash;
} * SCHED;

static SCHED __beg = NULL, __end = NULL;
static ULONG __cnt = 0;

static void (*on_off) (long msec) = (void(*)(long))NULL;


/*==============================================================================
 */
void
sched_init (void (*func) (long msec))
{
	on_off = func;
	
	if (on_off && __beg) (*on_off) (0);
}


/*----------------------------------------------------------------------------*/
static void
sort_in (SCHED sched)
{
	if (!__beg) {
		sched->Prev = sched->Next = NULL;
		__beg       = __end       = sched;
	
	} else if (__beg->Priority < sched->Priority) {
		sched->Next = __beg;
		sched->Prev = NULL;
		__beg->Prev = sched;
		__beg       = sched;
	
	} else {
		SCHED queue = __end;
		while (queue->Priority < sched->Priority) {
			queue = queue->Prev;
		}
		sched->Prev = queue;
		if ((sched->Next = queue->Next) != NULL) queue->Next->Prev = sched;
		else                                     __end             = sched;
		queue->Next = sched;
	}
	__cnt++;
}

/*----------------------------------------------------------------------------*/
static SCHED
move_out (SCHED sched)
{
	if (sched->Prev) sched->Prev->Next = sched->Next;
	else             __beg             = sched->Next;
	if (sched->Next) sched->Next->Prev = sched->Prev;
	else             __end             = sched->Prev;
	__cnt--;
	
	return sched;
}


/*==============================================================================
 */
BOOL
sched_insert (SCHED_FUNC func, void * value, long hash, int prio)
{
	SCHED sched = malloc (sizeof (struct s_SCHED));
	if (sched) {
		sched->Priority = prio;
		sched->func     = func;
		sched->Value    = value;
		sched->Hash     = hash;
		sort_in (sched);
		
		if (on_off) (*on_off) (1);
		
		return TRUE;
	}
	
	return FALSE; /* memory exhausted */
}

/*==============================================================================
 */
ULONG
sched_remove (SCHED_FUNC func, void * value)
{
	SCHED sched = __beg;
	ULONG num   = 0;
	
	while (sched) {
		SCHED next = sched->Next;
		if (sched->Value == value && (!func || sched->func == func)) {
			free (move_out (sched));
			num++;
			if (on_off && !__beg) (*on_off) (-1);
		}
		sched = next;
	}
	return num;
}

/*==============================================================================
 */
ULONG
sched_clear (long hash)
{
	SCHED sched = __beg;
	ULONG num   = 0;
	
	while (sched) {
		SCHED next = sched->Next;
		if (sched->Hash == hash) {
			(*sched->func)(sched->Value, hash);
			free (move_out (sched));
			num++;
			if (on_off && !__beg) (*on_off) (-1);
		}
		sched = next;
	}
	return num;
}


/*==============================================================================
 */
LONG
schedule (int max_do)
{
	static int _recursive = 0;
	
	if (!_recursive++) {
		
		if (max_do < 0) max_do = (int)__cnt;
		
		while (__beg) {
			SCHED sched = move_out (__beg);
			if ((*sched->func)(sched->Value, 0)) {
				sort_in (sched);
			} else {
				free (sched);
			}
			if (!--max_do) break;
		}
		
		if (on_off && !__beg) (*on_off) (0);
	}
	_recursive--;
	
	return (__beg ? 1 : 0);
}
