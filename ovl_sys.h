/* OVL_SYS.H
* 
* This will be the programming API for the ovl system.
*
* version 0.1
* Dan Ackerman
* aka baldrick@netset.com
*
*/
#ifndef __OVL_API__H__
#define __OVL_API__H__


#define OVL_REVISION 0x20030120ul


/* unique header identification */
/* Use which ever version you feel more comfortable with,
   or more appropriately which ever version your compiler is 
   more comfortable with
*/
#define OVL_MAGIC		"WiRe"


/*--- Module functions used by the client ---*/
typedef struct ovl_methods_t
{
	union { char c[4]; long l;
	}    magic;
	long revision; /* interface revision date in ISO yyyymmdd  BCD */
	long flags;    /* same basic information: */
	#define OF_SIMPLE  0x0000uL /* nothing special to do */
	#define OF_STARTUP 0x0001uL /* needs to get main() started, Pexec(4) */
	#define OF_CHLDPRC 0x0002uL /* needs to be started as process, Pexec(104) */
	#define OF_THREAD  0x0003uL
	long               __CDECL (*ovl_init)   (void);
	struct ovl_info_t *__CDECL (*ovl_version)(void);
	long               __CDECL (*ovl_free)   (void);
	long * magic_l;
} OVL_METH;

/*--- OVL information ---*/
struct ovl_info_t
{
	char *name;
	char *version;
	char *date;
	char *author;
};


/* define OVL_MODULE when you are building an OVL */
#ifdef OVL_MODULE

#define _ovl_head { OVL_MAGIC }, OVL_REVISION
#define _ovl_tail ovl_init, ovl_version, ovl_free, &ovl_methods.magic.l

/* shorthand for the OVL function table definition */
#define OVL_DECL(flags) OVL_METH ovl_methods = { _ovl_head, flags, _ovl_tail }


#else /* ! OVL_MODULE*/

OVL_METH * load_ovl (const char * ovl_name, void (*handler)(void*));
void       kill_ovl (void * this_N_all);


/* ----------------------------------------------------------------- *
 *  ovl_init() called by client to initialize ovl					 *
 * ----------------------------------------------------------------- */

#define ovl_init()             (*ovl_methods->ovl_init)()

/* ----------------------------------------------------------------- *
 * ovl_version - Returns infos about module                          *
 * returns a struct ovl_info_t * for the module                      *
 * ----------------------------------------------------------------- */

#define ovl_version()          (*ovl_methods->ovl_version)()		

/* ----------------------------------------------------------------- *
 * ovl_free - De-initialization of ovl                               *
 *                  (freeing memory, closing files, etc...)          *
 *  returns 0 on success, <0 if an error has occured                 *
 * ----------------------------------------------------------------- */

#define ovl_free()             (*ovl_methods->ovl_free)()

#endif /* !OVL_MODULE */

#endif /* !__OVL_API__H__ */
