/* OVL_SYS.C
*
* version 0.1
* Dan Ackerman
* aka baldrick@netset.com
*
* starting guts of OVL loading and handling system
*
*/
/*#include <aes.h>*/
#include <stdio.h>
#include <string.h>

#if defined (__PUREC__)
# include <tos.h>

#elif defined (LATTICE)
# include <dos.h>
# include <basepage.h>
# define BASPAG BASEPAGE

#elif defined (__GNUC__)
# include <osbind.h>
# include <basepage.h>
# define BASPAG BASEPAGE
#endif

#include "ovl_sys.h"

/* structs in use to hold OVL values*/

OVL_METH *ovl_methods;

BASPAG *ovl_basepage;

/* ----------------------------------------------------------------- *
 *  Load OVL - does the Pexecs and scans the OVL for the functions   *
 * ----------------------------------------------------------------- */
static int
load_ovl(char *ovl_name)
{
	long i;
	long newsize;

	ovl_basepage = (BASPAG *)Pexec(3,ovl_name,NULL,NULL);
	if ((long)ovl_basepage < 0L) {
		puts ("Pexec() failed");
		return 0;
	}
/*	Pexec(4,NULL,ovl_basepage,NULL);*/

	/* this calculation is based off of the Pexec() cookbook
	 * newsize = basepage + text + data + bss + heap + stack
	 * 
	 * some values are taken directly from there.  Also this is
	 * probably fairly PureC specific
	 */
	 
	newsize = 0x100 + ovl_basepage->p_tlen + ovl_basepage->p_dlen
			 + ovl_basepage->p_blen + 0x800 + 0x800;
			 
#ifdef __PUREC__
	Mshrink(0,ovl_basepage,newsize);

#else /* LATTICE && GCC */
	Mshrink(ovl_basepage,newsize);
#endif

	for (i = 0; i < ovl_basepage->p_dlen; i++)
	{
		/* Search for OVL Magic in DATA area */
		if(strncmp((char *)ovl_basepage->p_dbase + i,OVL_MAGIC,OVL_HDR_LEN)==0)
		{
			/* set function pointer structure */
			ovl_methods = (OVL_METH *)((char*)ovl_basepage->p_dbase + i + OVL_HDR_LEN);
			
			return 1;
		}
	 }
 
	return 0;
}

/* ----------------------------------------------------------------- *
 * ovl_infos - just grabbing and displaying the return from the      *
 *             ovl_version() call.                                   *
 * ----------------------------------------------------------------- */
static int
display_ovl_infos(void)
{
	struct ovl_info_t *ovl_data;

	ovl_data = ovl_version();

	printf("%s\r\n",ovl_data->name);
	printf("%s\r\n",ovl_data->version);
	printf("%s\r\n",ovl_data->date);
	printf("%s\r\n",ovl_data->author);
	
	return(1);
}

int load_sampleovl(void)
{
	if(load_ovl("modules\\sample.ovl") == 0)
		printf("No OVL MAGIC found\r\n");
	else
	{
		/* If we are here we found the OVL_MAGIC hidden in array[] */

		/* let OVL do any init work it needs to do */
		ovl_init();
		
		/* Grab and show OVL version data */
		display_ovl_infos();

		/* Close the OVL */
		ovl_free();
	}

  return (1);
}
