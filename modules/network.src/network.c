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


#ifdef __PUREC__
# define inet_host_addr(h,a) __CDECL inet_host_addr(h,a)
# define inet_connect(a,p)   __CDECL inet_connect(a,p)
# define inet_send(f,b,l)    __CDECL inet_send(f,b,l)
# define inet_recv(f,b,l)    __CDECL inet_recv(f,b,l)
# define inet_close(f)       __CDECL inet_close(f)
# define inet_info(_)        __CDECL inet_info(_)
#endif
#include "../../inet.h"
#include "../../inet.c"


/*--- Prototypes ---*/
long               __CDECL ovl_init   (void);
struct ovl_info_t *__CDECL ovl_version(void);
long               __CDECL ovl_free   (void);

#if defined(USE_MINT)
OVL_DECL (OF_CHLDPRC);
#else
OVL_DECL (OF_SIMPLE);
#endif
struct ovl_info_t ovl_data = {
#if defined(USE_MINT)
	"MiNTnet.OVL",
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
	static INET_FTAB inet = {
		inet_host_addr,
		inet_connect,
		inet_send,
		inet_recv,
		inet_close,
		inet_info
	};

	return (long)&inet;
}


/*============================================================================*/
struct ovl_info_t *__CDECL ovl_version(void)
{
	return((struct ovl_info_t *)&ovl_data);
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
		exit (EINVAL); /* mode yet not supported */
	
	} else if (ovl_methods.flags == OF_CHLDPRC) {
		if (Psigsetmask (0) == EINVFN) {
			exit (1); /* no signal handling, notify about that */
		}
		Pause(); /* else suspend */
	}
	
	/* else OF_STARTUP, simply quit */
	return 0;
}