/* sample.c
*
* version 0.1
* Dan Ackerman
* aka baldrick@netset.com
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

#elif defined (__GNUC__)
# include <unistd.h> /* for write() */
# include <osbind.h>

extern long _PgmSize; /* is this correct ??? */
#endif

/* we are building a module so we need to define OVL_MODULE
 * to avoid conflicts
 */
#define OVL_MODULE

#include "ovl_sys.h"


/*--- Prototypes ---*/
long ___CDECL ovl_init(void);
struct ovl_info_t *___CDECL ovl_version(void);
long ___CDECL ovl_free(void);

/* structs in use to hold OVL values*/

char filemagic[4] = OVL_MAGIC;
OVL_METH ovl_methods = {ovl_init,ovl_version,ovl_free};
struct ovl_info_t ovl_data;

int ovl_id;
int parent_id;

/* ----------------------------------------------------------------- *
 *  ovl_init() called by client to initialize ovl					 *
 * ----------------------------------------------------------------- */
long ___CDECL ovl_init(void)
{
	/* Do any initialization that the OVL may need to do,
	 * getting access to external functions etc here
	 */

	/* First fill out the OVL information structure 
	 * You could do this like icq_methods above, I personally
	 * prefer doing something like this so that it's easier to
	 * edit later.
	 */

	ovl_data.name	 = "SAMPLE.OVL";	 
	ovl_data.version = "0001";
	ovl_data.date 	 = "010903";
	ovl_data.author	 = "Dan Ackerman";
	
	return(0);
}

/* ----------------------------------------------------------------- *
 * ovl_version - Returns infos about module                          *
 * returns an ovl_info_t pointer
 * ----------------------------------------------------------------- */
struct ovl_info_t *___CDECL ovl_version(void)
{
	return((struct ovl_info_t *)&ovl_data);
}


/* ----------------------------------------------------------------- *
 * ovl_free - De-initialization of ovl                               *
 *                  (freeing memory, closing files, etc...)          *
 *  returns 0 on success, <0 if an error has occured                 *
 * ----------------------------------------------------------------- */
long ___CDECL ovl_free(void)
{
	return(0);
}


/* ----------------------------------------------------------------- *
 * main - Appl_init OVL and Ptermres                                 *
 * ----------------------------------------------------------------- */
int main(void)
{
  static void *array[] = {
  	(void *)OVL_MAGIC,
  	(OVL_METH *)&ovl_methods
  	};
  	
  /* Don't actually write anything, just keep array from being swallowed */
  write(1, array[0], 0);
  
  Ptermres(_PgmSize,0);		/* sit into memory */
  
  return (1);
}
