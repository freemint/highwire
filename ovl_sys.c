/* OVL_SYS.C
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

#include "defs.h"
#include "ovl_sys.h"


typedef struct {
	BASPAG   * basepage;
	OVL_METH * methods;
	char     * file;
	short      pid;
	void     (*handler)(void*);
}      OVL_SLOT;
static OVL_SLOT ovl_slot[16];
static short    num_slot = -1;


/*============================================================================*/
size_t
module_info (MODULINF * p_info)
{
	if (num_slot <= 0) {
		if (p_info) *p_info = NULL;
		return 0;
	
	} else if (p_info) {
		MODULINF info = malloc (sizeof (struct s_module_info) * num_slot);
		if ((*p_info = info) != NULL) {
			OVL_SLOT * slot = ovl_slot;
			size_t     num  = num_slot;
			short      i;
			for (i = 0; i < numberof(ovl_slot); i++) {
				if (slot->methods) {
					info->File = slot->file;
					info->Meth = slot->methods;
					if (!--num) break;
					info++;
				}
				slot++;
			}
		}
	}
	return num_slot;
}


/*----------------------------------------------------------------------------*/
static void
clear_slot (OVL_SLOT * slot)
{
printf("clear %p\n", slot->methods);
	if (slot->handler) {
		(*slot->handler)(slot->methods);
		slot->handler = NULL;
	}
	if (slot->basepage) {
		if (slot->basepage->p_env) {
			Mfree (slot->basepage->p_env);
		}
		Mfree (slot->basepage);
		slot->basepage = NULL;
	}
	if (slot->file) {
		free (slot->file);
		slot->file = NULL;
	}
	slot->methods  = NULL;
	slot->pid = 0;
	num_slot--;
}


/*------------------------------------------------------------------------------
 * signal handler for child process exited (flags >= OF_CHLDPRC)
*/
static void sig_chld (long sig)
{
	long  l = Pwaitpid (-1, 0, NULL);
	(void)sig;
	printf ("SIGCHLD: %i,%04lX \n", (short)(l >>16), l & 0xFFFF);
	if (l > 0 && num_slot > 0) {
		OVL_SLOT * slot = ovl_slot;
		short      pid  = l >>16;
		short      i;
		for (i = 0; i < numberof(ovl_slot); i++) {
			if (slot->pid == pid) {
				clear_slot (slot);
				break;
			}
			slot++;
		}
	}
}


/* ----------------------------------------------------------------- *
 *  Load OVL - does the Pexecs and scans the OVL for the functions   *
 * ----------------------------------------------------------------- */
OVL_METH *
load_ovl (const char * ovl_name, void (*handler)(void*))
{
	char       file_path[HW_PATH_MAX];
	BASPAG   * ovl_basepage;
	OVL_METH * ovl_methods  = NULL;
	short      pid          = 0;
	
	if (num_slot < 0) {
		memset (ovl_slot, 0, sizeof(ovl_slot));
		num_slot = 0;
	}
	
	strcat (strcpy (file_path, "modules\\"), ovl_name);
	ovl_basepage = (BASPAG *)Pexec(3,file_path,NULL,NULL);
	if ((long)ovl_basepage <= 0L) {
		puts ("Pexec() failed");
		return NULL;
	
	} else {
		WORD * ptr = (WORD*)ovl_basepage->p_dbase;
		WORD * end = ptr + (ovl_basepage->p_dlen - sizeof(OVL_METH)) / 2;
		do if (*(long*)ptr == OVL_MAGIC) {
			OVL_METH * m = (OVL_METH*)ptr;
			long       r = m->revision & 0x88888888uL;
			if (m->revision & ((r >>1) | (r >>2))) {      /* check for valid BCD */
				printf ("OVL: infalid revision %08lX\n", m->revision);
			} else if (m->revision < OVL_REVISION) {             /* compare date */
				printf ("OVL: too old %08lX\n", m->revision);
			} else if (&m->magic != m->magic_l) {            /* additional check */
				printf ("OVL: invalid format (0x%lX/0x%lX)\n",
				        (long)&m->magic, (long)m->magic_l);
			} else {
				ovl_methods = m;
			}
			break;
		} while (++ptr < end);
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
				pid = Pexec (104, ovl_name, ovl_basepage, NULL);
				if (pid > 0) {
					Psignal (20/*SIGCHLD*/, (long)sig_chld);
				} else if (pid == EINVFN) {
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
		if (ovl_basepage->p_env) {
			Mfree (ovl_basepage->p_env);
		}
		Mfree (ovl_basepage);
	
	} else {
		char     * file = strrchr (ovl_name, '\\');
		OVL_SLOT * slot = ovl_slot;
		short      i;
		for (i = 0; i < numberof(ovl_slot); i++) {
			if (!slot->methods) {
				slot->basepage = ovl_basepage;
				slot->methods  = ovl_methods;
				slot->file     = strdup (file ? file +1 : ovl_name);
				slot->pid      = pid;
				slot->handler  = handler;
				num_slot++;
				break;
			}
			slot++;
		}
	}
	
	return ovl_methods;
}


/* ----------------------------------------------------------------- *
 * ----------------------------------------------------------------- */
void
kill_ovl (void * this_N_all)
{
	if (num_slot > 0) {
		long       mask;
		OVL_SLOT * slot;
		short      i, n = (this_N_all ? 1 : num_slot);
		
		slot = ovl_slot;
		for (i = 0; i < numberof(ovl_slot); i++) {
			if (slot->methods && (!this_N_all || this_N_all == slot->methods)) {
				if (slot->pid > 0) {
					long r = Pkill (slot->pid,  19/*SIGCONT*/);
					printf("kill(%i,SIGCONT) = %li\n", slot->pid, r);
					
					r = Pwaitpid (slot->pid, 1, NULL);
					printf("waitpid(%i) = %i,%04lX \n",
						    slot->pid, (short)(r >>16), r & 0xFFFF);
					if (r != 0) {
						slot->pid = 0;
					}
				}
				if (!--n) break;
			}
			slot++;
		}
		if (num_slot <= 0) {
			return;
		}
		mask = Psigblock (1uL << 20/*SIGCHLD*/);
		slot = ovl_slot;
		for (i = 0; i < numberof(ovl_slot); i++) {
			if (slot->methods && (!this_N_all || this_N_all == slot->methods)) {
				if (slot->pid > 0) {
					long r = Pkill (slot->pid,  1/*SIGHUP*/);
					printf("kill(%i,SIGHUP) = %li\n", slot->pid, r);
					
					r = Pwaitpid (slot->pid, 0, NULL);
					printf("waitpid() = %i,%04lX \n", (short)(r >>16), r & 0xFFFF);
				}
				clear_slot (slot);
				if (!num_slot) break;
			}
			slot++;
		}
		Psigsetmask (mask);
	}
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
	OVL_METH * ovl_methods = load_ovl ("sample.ovl", (void(*)(void*))0);
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
