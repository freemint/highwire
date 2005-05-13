#include <stdlib.h>

#if defined (__PUREC__)
# include <tos.h>

#elif defined (LATTICE)
# include <dos.h>
# include <mintbind.h>

#elif defined (__GNUC__)
# include <mintbind.h>
#endif

#include <errno.h>


#define OVL_MODULE

#include "../../hw-types.h"
#include "../../ovl_sys.h"

#include "../../inet.h"
#include "../../inet.c"


#if defined(USE_MINT)
OVL_DECL (OF_CHLDPRC,FTAB_NETWORK);
#else
OVL_DECL (OF_SIMPLE,FTAB_NETWORK);
#endif
struct ovl_info_t ovl_data = {
#if defined(USE_MINT)
	"MiNTnet.OVL",
#elif defined(USE_ICNN)
	"Iconnect.OVL",
#elif defined(USE_STNG)
	"Sting.OVL",
#elif defined(USE_STIK)
	"STiK2.OVL",
#else
	"NETWORK.OVL",
#endif
	"0001", __DATE__ ", " __TIME__, "AltF4@freemint.de"
};


/*============================================================================*/
long __CDECL ovl_init(void)
{
	return INET_VERSION;
}


/*============================================================================*/
struct ovl_info_t *__CDECL ovl_version(void)
{
	return((struct ovl_info_t *)&ovl_data);
}

/* ----------------------------------------------------------------- *
 * ovl_getftab - Returns function table for OVL                      *
 * ----------------------------------------------------------------- */

void * __CDECL ovl_getftab(void)
{
	static INET_FTAB inet = {
		inet_host_addr,
		inet_connect,
		inet_send,
		inet_recv,
		inet_close,
		inet_instat,
		inet_select,
		inet_info
	};

	return &inet;
}



/*============================================================================*/
long __CDECL ovl_free(void)
{
	return(0);
}


/*============================================================================*/
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
