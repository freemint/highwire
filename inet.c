/* @(#)highwire/inet.c
 */
#if defined (USE_INET) && !defined(USE_OVL)
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


typedef struct {
	short __CDECL (*host_addr) (const char * host, long * addr);
	long  __CDECL (*connect) (long addr, long port);
	long  __CDECL (*send)    (long fh, char * buf, size_t len);
	long  __CDECL (*recv)    (long fh, char * buf, size_t len);
	void  __CDECL (*close)   (long fh);
	const char * __CDECL (*info) (void);
} INET_FTAB;


#if defined(USE_OVL) /*********************************************************/
# include "ovl_sys.h"

static OVL_METH  * inet_ovl  = NULL;
static INET_FTAB * inet_ftab = NULL;

/*----------------------------------------------------------------------------*/
static void ovl_invalid (void * ignore)
{
	(void)ignore;
	inet_ovl  = NULL;
	inet_ftab = NULL;
}

/*----------------------------------------------------------------------------*/
static BOOL ovl_load(void)
{
	if (inet_ftab) {
		return TRUE;
	}
	if ((inet_ovl = load_ovl ("network.ovl", ovl_invalid)) == NULL) {
		return FALSE; /* no OVL found */
	}
	if ((*inet_ovl->ovl_init)() < 0 ||
	    (inet_ftab = (*inet_ovl->ovl_getftab)()) == NULL) {
		kill_ovl (inet_ovl);
		return FALSE;    /* wrong OVL */
	}
	return TRUE;
}

/* endif defined(USE_OVL) */

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
short
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

#elif defined(USE_OVL)
	if (ovl_load()) {
		ret = (*inet_ftab->host_addr)(name, addr);
	}

#else
	(void)name; (void)addr;
#endif

	return ret;
}


/*============================================================================*/
long
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
	} else if (connect (fh, (struct sockaddr *)&s_in, sizeof (s_in)) < 0) {
		fh = -errno;
		close (fh);
	}

#elif defined(USE_STIK)
	if (!init_stick()) {
		puts ("No STiK");
	} else {
		if ((fh = TCP_open (addr, (short)port, 0, 2048)) < 0) {
			fh = -1;
		}
	}

#elif defined(USE_OVL)
	if (ovl_load()) {
		fh = (*inet_ftab->connect)(addr, port);
	}

#else
	(void)addr; (void)port;
#endif

	return fh;
}


/*============================================================================*/
long
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

#elif defined(USE_OVL)
	if (inet_ftab) {
		fh = (*inet_ftab->send)(fh, buf, len);
	}

#else
	(void)fh; (void)buf; (void)len;
#endif

	return ret;
}


/*============================================================================*/
long
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
			if (!ret) ret = -1;
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
			if (!ret) ret = -1;
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

#elif defined(USE_OVL)
	if (inet_ftab) {
		ret = (*inet_ftab->recv)(fh, buf, len);
	}

#else
	ret = -1;
	(void)fh; (void)buf; (void)len;
#endif

	return ret;
}


/*============================================================================*/
void
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
	
	#elif defined(USE_OVL)
		if (inet_ftab) {
			(*inet_ftab->close)(fh);
		}
	#endif
	}
}


/*============================================================================*/
const char *
inet_info (void)
{
#if defined(USE_MINT)
	return "MiNTnet";

#elif defined(USE_STIK)
	return "STiK2";

#elif defined(USE_OVL)
	if (inet_ftab) {
		return (*inet_ftab->info)();
	} else {
		return NULL;
	}

#else
	return NULL;
#endif
}
