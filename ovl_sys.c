/* OVL_SYS.C
*
* version 0.1
* Dan Ackerman
* aka baldrick@netset.com
*
* starting guts of OVL loading and handling system
*
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined (__PUREC__)
# include <tos.h>

#elif defined (LATTICE)
# include <dos.h>
# include <mintbind.h>
# include <basepage.h>
# define BASPAG BASEPAGE

#elif defined (__GNUC__)
# include <mintbind.h>
# include <basepage.h>
# define BASPAG BASEPAGE
#endif

#ifndef EINVFN
# define EINVFN -32
#endif

#include "hw-types.h"
#include "ovl_sys.h"


typedef struct {
	BASPAG   * basepage;
	OVL_METH * methods;
	short      pid;
	void     (*handler)(void*);
}      OVL_SLOT;
static OVL_SLOT slot[8] = {
	{NULL,NULL,0,NULL},{NULL,NULL,0,NULL},{NULL,NULL,0,NULL},{NULL,NULL,0,NULL},
	{NULL,NULL,0,NULL},{NULL,NULL,0,NULL},{NULL,NULL,0,NULL},{NULL,NULL,0,NULL}
};


/*------------------------------------------------------------------------------
 * signal handler for child process exited (flags >= OF_CHLDPRC)
*/
static void sig_chld (long sig)
{
	long  l   = Pwaitpid (-1, 0, NULL);
	short pid = l >>16;
	short i;
	printf ("SIGCHLD: %i,%04lX \n", pid, l & 0xFFFF);
	for (i = 0; i < numberof(slot); i++) {
		if (slot[i].pid == pid) {
			if (slot[i].handler) {
				(*slot[i].handler)(slot[i].methods);
				slot[i].handler = NULL;
			}
			free (slot[i].basepage);
			slot[i].basepage = NULL;
			slot[i].methods  = NULL;
			slot[i].pid = 0;
			break;
		}
	}
	(void)sig;
}


/* ----------------------------------------------------------------- *
 *  Load OVL - does the Pexecs and scans the OVL for the functions   *
 * ----------------------------------------------------------------- */
OVL_METH *
load_ovl (const char * ovl_name, void (*handler)(void*))
{
	BASPAG   * ovl_basepage = (BASPAG *)Pexec(3,ovl_name,NULL,NULL);
	OVL_METH * ovl_methods  = NULL;
	short      pid          = 0;
	
	if ((long)ovl_basepage <= 0L) {
		puts ("Pexec() failed");
		return NULL;
	
	} else {
		union { char c[4]; long l; } magic = { { OVL_MAGIC } };
		OVL_METH * p   = (OVL_METH*)ovl_basepage->p_dbase;
		OVL_METH * end = p + ovl_basepage->p_dlen - sizeof(OVL_METH);
		do if (p->magic.l == magic.l) {
			long r = p->revision & 0x88888888uL;
			if (p->revision & ((r >>1) | (r >>2))) {        /* check for BCD */
				printf ("OVL: infalid revision %08lX\n", p->revision);
			} else if (p->revision < OVL_REVISION) {        /* compare date */
				printf ("OVL: too old %08lX\n", p->revision);
			} else if (&p->magic.l != p->magic_l) {         /* additional check */
				printf ("OVL: invalid format (0x%lX/0x%lX)\n",
				        (long)&p->magic.l, (long)p->magic_l);
			} else {
				ovl_methods = p;
			}
			break;
		} while (++p < end);
	}
printf("ovl: bp=%p ft=%p \n", ovl_basepage, ovl_methods);
	
	if (ovl_methods) {
		if (ovl_methods->flags >= OF_THREAD) { /* not supported yet */
			ovl_methods = NULL;
		
		} else if (!ovl_methods->flags) { /* OF_SIMPLE */
			/* this calculation is based off of the Pexec() cookbook
			 * newsize = basepage + text + data + bss + heap + stack
			 * 
			 * some values are taken directly from there.  Also this is
			 * probably fairly PureC specific
			 */
			size_t newsize = 0x100 + ovl_basepage->p_tlen + ovl_basepage->p_dlen
			               + ovl_basepage->p_blen + 0x800 + 0x800;
		#ifdef __PUREC__
			Mshrink(0,ovl_basepage,newsize);
		#else /* LATTICE && GCC */
			Mshrink(ovl_basepage,newsize);
		#endif
		
		} else {
			if (ovl_methods->flags == OF_CHLDPRC) {
				pid = Pexec (104, NULL, ovl_basepage, NULL);
				if (pid > 0) {
					Psignal (20/*SIGCHLD*/, (long)sig_chld);
				} else if (pid == -32) { /* EINVFN */
					pid = 0;
				} else {
					ovl_methods = NULL;
				}
			}
			if (!pid) { /* OF_STARTUP or 104 not available */
				if (Pexec (4, NULL, ovl_basepage, NULL) < 0) {
					ovl_methods = NULL;
				}
			}
		}
	}
	
	if (!ovl_methods) {
		free (ovl_basepage);
	
	} else {
		short i;
		for (i = 0; i < numberof(slot); i++) {
			if (!slot[i].methods) {
				slot[i].basepage = ovl_basepage;
				slot[i].methods  = ovl_methods;
				slot[i].pid      = pid;
				slot[i].handler  = handler;
				break;
			}
		}
	}
	
	return ovl_methods;
}


/* ----------------------------------------------------------------- *
 * ----------------------------------------------------------------- */
void
kill_ovl (void * this_N_all)
{
	long  mask = Psigblock (1uL << 20/*SIGCHLD*/);
	short i;
	
	for (i = 0; i < numberof(slot); i++) {
		if (slot[i].methods && (!this_N_all || this_N_all == slot[i].methods)) {
			if (slot[i].pid > 0) {
				long r = Pkill (slot[i].pid,  19/*SIGCONT*/);
				printf("kill(%i,SIGCONT) = %li\n", slot[i].pid, r);
				
				if ((r = Pwaitpid (slot[i].pid, 1, NULL)) != 0) {
					slot[i].pid = 0;
				}
				printf("waitpid(%i) = %i,%04lX \n",
					    (short)(r >>16), (short)(r >>16), r & 0xFFFF);
			}
		}
	}
	for (i = 0; i < numberof(slot); i++) {
		if (slot[i].methods && (!this_N_all || this_N_all == slot[i].methods)) {
			if (slot[i].pid > 0) {
				long r = Pkill (slot[i].pid,  1/*SIGHUP*/);
				printf("kill(%i,SIGHUP) = %li\n", slot[i].pid, r);
				
				r = Pwaitpid (slot[i].pid, 0, NULL);
				printf("waitpid() = %i,%04lX \n", (short)(r >>16), r & 0xFFFF);
			
			}
			if (slot[i].handler) {
				(*slot[i].handler)(slot[i].methods);
				slot[i].handler = NULL;
			}
			free (slot[i].basepage);
			slot[i].basepage = NULL;
			slot[i].methods  = NULL;
			slot[i].pid = 0;
		}
	}
	Psigsetmask (mask);
}



/* ----------------------------------------------------------------- *
 * ovl_infos - just grabbing and displaying the return from the      *
 *             ovl_version() call.                                   *
 * ----------------------------------------------------------------- */
static int
display_ovl_infos (OVL_METH * ovl_methods)
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
	OVL_METH * ovl_methods = load_ovl ("modules\\sample.ovl", (void(*)(void*))0);
	if (!ovl_methods) {
		printf("No OVL MAGIC found\r\n");
	
	} else { /* If we are here we found the OVL_MAGIC hidden in array[] */

		/* let OVL do any init work it needs to do */
		ovl_init();
		
		/* Grab and show OVL version data */
		display_ovl_infos (ovl_methods);

		/* Close the OVL */
		ovl_free();
	}

  return (1);
}
