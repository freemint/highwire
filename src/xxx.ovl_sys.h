/* OVL_SYS.H
* 
* This will be the programming API for the ovl system.
*
*/
#ifndef __OVL_API__H__
#define __OVL_API__H__


#define OVL_REVISION 0x20030206ul


/* unique header identification */
/* Use which ever version you feel more comfortable with,
   or more appropriately which ever version your compiler is 
   more comfortable with
*/
#define OVL_MAGIC	0x57695265L		/* "WiRe" 	 */
	
/*--- Module functions used by the client ---*/
typedef struct ovl_methods_t
{
	long magic;    /* must be "WiRe" */
	long revision; /* interface revision date in ISO yyyymmdd  BCD */
	long flags;    /* startup basic information: */
	#define OF_SIMPLE  0x0000uL /* nothing special to do */
	#define OF_STARTUP 0x0001uL /* needs to get main() started, Pexec(4) */
	#define OF_CHLDPRC 0x0002uL /* needs to be started as process, Pexec(104) */
	#define OF_THREAD  0x0003uL
	long ftabtype; /* Function Table mapping reference */
	#define FTAB_SIMPLE  0x0000uL /* standard FTAB interface   */
	#define FTAB_NETWORK 0x0001uL /* networking FTAB interface */
	long               __CDECL (*ovl_init)   (void);
	struct ovl_info_t *__CDECL (*ovl_version)(void);
	void              *__CDECL (*ovl_getftab)(void);
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

#define _ovl_head OVL_MAGIC, OVL_REVISION
#define _ovl_tail ovl_init, ovl_version, ovl_getftab, ovl_free, &ovl_methods.magic

/* shorthand for the OVL function table definition */
#define OVL_DECL(flags,ft_type) OVL_METH ovl_methods = { _ovl_head, flags, ft_type, _ovl_tail }

/* set of functions which are always defined in an OVL */
long               __CDECL ovl_init   (void);
struct ovl_info_t *__CDECL ovl_version(void);
void              *__CDECL ovl_getftab(void);
long               __CDECL ovl_free   (void);


#else /* ! OVL_MODULE*/

OVL_METH * load_ovl (const char * ovl_name, void (*handler)(void*));
void       kill_ovl (void * this_N_all);

typedef struct s_module_info {
	const char * File;
	OVL_METH   * Meth;
} * MODULINF;

size_t module_info (MODULINF*);


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
 * ovl_getftab - Returns function table for OVL                      *
 * ----------------------------------------------------------------- */

#define ovl_getftab()          (*ovl_methods->ovl_getftab)()		

/* ----------------------------------------------------------------- *
 * ovl_free - De-initialization of ovl                               *
 *                  (freeing memory, closing files, etc...)          *
 *  returns 0 on success, <0 if an error has occured                 *
 * ----------------------------------------------------------------- */

#define ovl_free()             (*ovl_methods->ovl_free)()

#endif /* !OVL_MODULE */

#endif /* !__OVL_API__H__ */
