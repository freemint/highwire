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


typedef BOOL (*SCHED_FUNC) (void *, long);

typedef struct s_SCHED {
	struct s_SCHED * Next;
	SCHED_FUNC       func;
	void           * Value;
	long             Hash;
} * SCHED;

static SCHED __beg = NULL, __end = NULL;
static ULONG __cnt = 0;

static void (*on_off) (long msec);


/*==============================================================================
 */
void
sched_init (void (*func) (long msec))
{
	on_off = func;
	
	if (on_off && __beg) (*on_off) (0);
}


/*==============================================================================
 */
void
sched_insert (SCHED_FUNC func, void * value, long hash)
{
	SCHED sched = malloc (sizeof (struct s_SCHED));
	sched->Next  = NULL;
	sched->func  = func;
	sched->Value = value;
	sched->Hash  = hash;
	if (__end) __end->Next = sched;
	else       __beg       = sched;
	__end = sched;
	__cnt++;
	
	if (on_off) (*on_off) (1);
}

/*==============================================================================
 */
ULONG
sched_remove (SCHED_FUNC func, void * value)
{
	SCHED sched = __beg, prev = NULL;
	ULONG num   = 0;
	
	while (sched) {
		SCHED next = sched->Next;
		if (sched->Value == value && (!func || sched->func == func)) {
			if (prev) {
				if ((prev->Next = next) == NULL)
					__end = prev;
			} else if ((__beg = next) == NULL) {
				__end = NULL;
				if (on_off) (*on_off) (-1);
			}
			free (sched);
			__cnt--;
			num++;
		} else {
			prev = sched;
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
	SCHED sched = __beg, prev = NULL;
	ULONG num   = 0;
	
	while (sched) {
		SCHED next = sched->Next;
		if (sched->Hash == hash) {
			(*sched->func)(sched->Value, hash);
			if (prev) {
				if ((prev->Next = next) == NULL)
					__end = prev;
			} else if ((__beg = next) == NULL) {
				__end = NULL;
				if (on_off) (*on_off) (-1);
			}
			free (sched);
			__cnt--;
			num++;
		} else {
			prev  = sched;
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
			SCHED sched = __beg;
			__beg       = sched->Next;
			if (!__beg) __end = NULL;
			__cnt--;
			
			if ((*sched->func)(sched->Value, 0)) {
				sched->Next = NULL;
				if (__end) __end->Next = sched;
				else       __beg       = sched;
				__end = sched;
				__cnt++;
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
