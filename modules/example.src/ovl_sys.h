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

/*--- Macro for prototyping ---*/
#ifdef __PUREC__
#  define ___CDECL cdecl
#else
#  define ___CDECL
#endif

/* unique header identification */
/* Use which ever version you feel more comfortable with,
   or more appropriately which ever version your compiler is 
   more comfortable with
*/
#define OVL_MAGIC		"WiRe"

/* header identification length */
#define OVL_HDR_LEN		4

/*--- OVL information ---*/
struct ovl_info_t
{
	char *name;
	char *version;
	char *date;
	char *author;
};

/*--- Module functions used by the client ---*/
typedef struct ovl_methods_t
{
  long ___CDECL (*ovl_init)(void);
  struct ovl_info_t *___CDECL (*ovl_version)(void);
  long ___CDECL (*ovl_free)(void);
} OVL_METH;



/* define OVL_MODULE when you are building an OVL */
#ifndef OVL_MODULE

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
