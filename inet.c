/* @(#)highwire/inet.c
 */
#if defined(USE_OVL)
# undef USE_OVL
# define _USE_OVL_

#elif defined (USE_INET)
# if defined(__GNUC__)
#  define USE_MINT

# elif defined(__PUREC__)
#  define USE_STIK
# endif
#endif /* USE_INET */


#include <stddef.h>
#include <errno.h>

#ifndef __HW_INET_H__
# include "hw-types.h"
# include "inet.h"
#endif


#if defined(_USE_OVL_) /*******************************************************/
# include "ovl_sys.h"

static OVL_METH  * inet_ovl  = NULL;
static short __CDECL demand_host_addr (const char * host, long * addr);
static long  __CDECL demand_connect   (long addr, long port);
INET_FTAB inet_ftab = {
	demand_host_addr,
	demand_connect,
	inet_send,
	inet_recv,
	inet_close,
	inet_info
}, backup;

/*----------------------------------------------------------------------------*/
static void ovl_invalid (void * ignore)
{
	(void)ignore;
	if (inet_ovl) {
		inet_ovl  = NULL;
		inet_ftab = backup;
	}
}

/*----------------------------------------------------------------------------*/
static BOOL ovl_load(void)
{
	INET_FTAB * ovl_ftab = NULL;
	
	if (inet_ovl) {
		return TRUE;
	
	} else if ((inet_ovl = load_ovl ("network.ovl", ovl_invalid)) == NULL) {
		return FALSE; /* no OVL found */
	
	} else if (inet_ovl->ftabtype != FTAB_NETWORK ||
	           (*inet_ovl->ovl_init)() < 0 ||
	           (ovl_ftab = (*inet_ovl->ovl_getftab)()) == NULL) {
		inet_ovl = NULL;
		kill_ovl (inet_ovl);
		return FALSE;    /* wrong OVL */
	}
	backup    = inet_ftab;
	inet_ftab = *ovl_ftab;
	
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static short __CDECL demand_host_addr (const char * host, long * addr)
{
	return (ovl_load() ? (*inet_ftab.host_addr)(host, addr)
	                   : inet_host_addr (host, addr));
}

/*----------------------------------------------------------------------------*/
static long  __CDECL demand_connect (long addr, long port)
{
	return (ovl_load() ? (*inet_ftab.connect)(addr, port)
	                   : inet_connect (addr, port));
}

/* endif defined(_USE_OVL_) */

#elif defined(USE_MINT) /******************************************************/
# include <netdb.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <mintbind.h>


#elif defined(USE_STIK) /******************************************************/
# include <stdio.h> /*printf/puts */
# include <string.h>
# include <tos.h>
# include <time.h>
# undef min
# undef max
# include <transprt.h>


static TPL * tpl = NULL;


/*----------------------------------------------------------------------------*/
static DRV_LIST * find_drivers(void)
{
	struct {
		long cktag;
		long ckvalue;
	}  * jar = (void*)Setexc (0x5A0 /4, (void (*)())-1);
	long i   = 0;
	long tag = 'STiK';

	while (jar[i].cktag) {
		if (jar[i].cktag == tag) {
			return (DRV_LIST*)jar[i].ckvalue;
		}
		++i;
	}
	return NULL;  /* Pointless return value... */
}


/*----------------------------------------------------------------------------*/
static int init_stick (void)
{
	if (!tpl) {
		DRV_LIST * drivers = find_drivers();
		if (drivers && strcmp (MAGIC, drivers->magic) == 0) {
			tpl = (TPL*)get_dftab(TRANSPORT_DRIVER);
		}
	}
	return (tpl != NULL);
}

#endif /* USE_STIK ************************************************************/


/*============================================================================*/
short __CDECL
inet_host_addr (const char * name, long * addr)
{
	short ret = -1;

#if defined(USE_MINT)
	struct hostent * host = gethostbyname (name);
	if (host) {
		*addr = *(long*)host->h_addr;
		ret   = E_OK;
	} else {
		ret   = -errno;
	}

#elif defined(USE_STIK)
	if (init_stick()) {
		if (resolve ((char*)name, NULL, (uint32*)addr, 1) > 0) {
			ret = E_OK;
		}
	}

#else
	(void)name; (void)addr;
#endif

	return ret;
}


/*----------------------------------------------------------------------------*/
#if defined(USE_MINT) || defined(USE_STIK)
static BOOL timeout = FALSE;

static void sig_alrm (long sig)
{
	(void)sig;
	timeout = TRUE;
}
#endif

/*============================================================================*/
long __CDECL
inet_connect (long addr, long port)
{
	long fh = -1;

#if defined(USE_MINT)
	struct sockaddr_in s_in;
	s_in.sin_family = AF_INET;
	s_in.sin_port   = htons ((short)port);
	s_in.sin_addr   = *(struct in_addr *)&addr;
	if ((fh = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
		fh = -errno;
	} else {
		long alrm = Psignal (14/*SIGALRM*/, (long)sig_alrm);
		if (alrm >= 0) {
			timeout = FALSE;
			Talarm (3);
		}
		if (connect (fh, (struct sockaddr *)&s_in, sizeof (s_in)) < 0) {
			close (fh);
			fh = -(timeout && errno == EINTR ? ETIMEDOUT : errno);
		}
		if (alrm >= 0) {
			Talarm (0);
			Psignal (14/*SIGALRM*/, alrm);
		}
	}

#elif defined(USE_STIK)
	if (!init_stick()) {
		puts ("No STiK");
	} else {
		long alrm = Psignal (14/*SIGALRM*/, (long)sig_alrm);
		if (alrm >= 0) {
			timeout = FALSE;
			Talarm (3);
		}
		if ((fh = TCP_open (addr, (short)port, 0, 2048)) < 0) {
			fh = -(fh == -1001L ? ETIMEDOUT : 1);
		}
		if (alrm >= 0) {
			Talarm (0);
			Psignal (14/*SIGALRM*/, alrm);
		}
	}

#else
	(void)addr; (void)port;
#endif

	return fh;
}


/*============================================================================*/
long __CDECL
inet_send (long fh, char * buf, size_t len)
{
	long ret = -1;

#if defined(USE_MINT)
	ret = Fwrite (fh, len, buf);

#elif defined(USE_STIK)
	if (!tpl) {
		puts ("No STiK");
	} else {
		ret = TCP_send ((int)fh, buf, (int)len);
	}

#else
	(void)fh; (void)buf; (void)len;
#endif

	return ret;
}


/*============================================================================*/
long __CDECL
inet_recv (long fh, char * buf, size_t len)
{
	long ret = 0;

#if defined(USE_MINT)
	while (len) {
		long n = Finstat (fh);
		if (n < 0) {
			if (!ret) ret = n;
			break;
		} else if (n == 0x7FFFFFFF) { /* connection closed */
			if (!ret) ret = -ECONNRESET;
			break;
		} else if (n && (n = Fread (fh, (n < len ? n : len), buf)) < 0) {
			if (!ret) ret = -errno;
			break;
		} else if (n) {
			ret += n;
			buf += n;
			len -= n;
		} else { /* no data available yet */
			break;
		}
	}

#elif defined(USE_STIK)
	if (!tpl) {
		puts ("No STiK");
		ret = -1;
	} else while (len) {
		short n = CNbyte_count ((int)fh);
		if (n < E_NODATA) {
			if (!ret) ret = (n == E_EOF || n == E_RRESET ? -ECONNRESET : -1);
			break;
		} else if (n > 0) {
			if (n > len) n = len;
			if ((n = CNget_block ((int)fh, buf, n)) < 0) {
				if (!ret) ret = -1;
				break;
			} else {
				ret += n;
				buf += n;
				len -= n;
			}
		} else { /* no data available yet */
			break;
		}
	}

#else
	ret = -1;
	(void)fh; (void)buf; (void)len;
#endif

	return ret;
}


/*============================================================================*/
void __CDECL
inet_close (long fh)
{
	if (fh >= 0) {

	#if defined(USE_MINT)
		close (fh);

	#elif defined(USE_STIK)
		if (!tpl) {
			puts ("No STiK");
		} else {
			TCP_close ((int)fh, 0);
		}
	
	#endif
	}
}


/*============================================================================*/
const char * __CDECL
inet_info (void)
{
#if defined(USE_MINT)
	return "MiNTnet";

#elif defined(USE_STIK)
	return "STiK2";

#else
	return NULL;
#endif
}
