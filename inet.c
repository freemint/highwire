/* @(#)highwire/inet.c
 */
#ifdef USE_INET
# if defined(__GNUC__)
#  define USE_MINT

# elif defined(__PUREC__)
#  define USE_STIK
# endif
#endif /* USE_INET */


#include <stddef.h>
#include <errno.h>

#include "inet.h"


/*******************************************************************************
 *
 * Basic functions
 *
 */

#if defined(USE_MINT)
# include <netdb.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <mintbind.h>


#elif defined(USE_STIK)
# include <stdio.h> /*printf/puts */
# include <string.h>
# include <tos.h>
# include <time.h>
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

#endif /* USE_STIK */


/*============================================================================*/
short
inet_host_addr (const char * name, long * addr)
{
	short ret = -1;

#if defined(USE_MINT)
	struct hostent * host = gethostbyname (name);
	if (host) {
		*addr = *(long*)host->h_addr;
		ret   = 0;
	} else {
		ret   = -errno;
	}

#elif defined(USE_STIK)
	if (init_stick()) {
		if (resolve ((char*)name, NULL, (uint32*)addr, 1) > 0) {
			ret = 0;
		}
	}

#else
	(void)name, addr;
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

#else
	(void)addr, port;
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

#else
	(void)fh, buf, len;
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

#else
	ret = -1;
	(void)fh, buf, len;
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

#else
	return NULL;
#endif
}
