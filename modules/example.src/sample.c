/* sample.c
*
* This is a bare bones sample OVL skeleton for HighWire Project
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined (__PUREC__)
# include <tos.h>

#elif defined (LATTICE)
# include <dos.h>
# include <mintbind.h>

#elif defined (__GNUC__)
# include <mintbind.h>
#endif

/* we are building a module so we need to define OVL_MODULE
 * to avoid conflicts
 */
#define OVL_MODULE

#include "../../hw-types.h"
#include "../../ovl_sys.h"


/* structs in use to hold OVL values*/

OVL_DECL (OF_SIMPLE,FTAB_SIMPLE);

struct ovl_info_t ovl_data;


/* ----------------------------------------------------------------- *
 *  ovl_init() called by client to initialize ovl					 *
 * ----------------------------------------------------------------- */
long __CDECL ovl_init(void)
{
	/* Do any initialization that the OVL may need to do,
	 * getting access to external functions etc here
	 */

	/* First fill out the OVL information structure 
	 * You could do this like icq_methods above, I personally
	 * prefer doing something like this so that it's easier to
	 * edit later.
	 */

	ovl_data.name    = "SAMPLE.OVL";	 
	ovl_data.version = "0003";
	ovl_data.date    = "012903";
	ovl_data.author  = "HighWire Dev Team";
	
	return(0);
}

/* ----------------------------------------------------------------- *
 * ovl_version - Returns infos about module                          *
 * returns an ovl_info_t pointer
 * ----------------------------------------------------------------- */
struct ovl_info_t *__CDECL ovl_version(void)
{
	return((struct ovl_info_t *)&ovl_data);
}

/* ----------------------------------------------------------------- *
 * ovl_getftab - Returns function table for OVL                      *
 * ----------------------------------------------------------------- */

void * __CDECL ovl_getftab(void)
{
	/* This OVL has no functions currently */
	
	return NULL;
}


/* ----------------------------------------------------------------- *
 * ovl_free - De-initialization of ovl                               *
 *                  (freeing memory, closing files, etc...)          *
 *  returns 0 on success, <0 if an error has occured                 *
 * ----------------------------------------------------------------- */
long __CDECL ovl_free(void)
{
	return(0);
}


/* ----------------------------------------------------------------- *
 * main - Appl_init OVL and Ptermres                                 *
 * ----------------------------------------------------------------- */
int main(void)
{
	if (!ovl_methods.flags) { /* OF_SIMPLE */
		exit (1); /* this shouldn't happen, but not fatal */
	
	} else if (ovl_methods.flags >= OF_THREAD) {
		exit (-EINVAL); /* mode yet not supported */
	
	} else if (ovl_methods.flags == OF_CHLDPRC) {
		if (Psigsetmask (0) == -EINVFN) {
			exit (1); /* no signal handling, notify about that */
		}
		Pause(); /* else suspend */
	}
	
	/* else OF_STARTUP, simply quit */
	return 0;
}
