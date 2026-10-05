
#ifndef CLK_TCK
#   define CLK_TCK     CLOCKS_PER_SEC
#endif

#include <time.h>

#include "include/iconnect/sockinit.h"
#include "include/iconnect/netdb.h"
#include "include/iconnect/types.h"
#include "include/iconnect/socket.h"
#include "include/iconnect/in.h"
#include "include/iconnect/sfcntl.h"
#include "include/iconnect/sockios.h"
#include "include/iconnect/types.h"

/*----------------------------------------------------------------------------*/
static BOOL init_iconnect (void)
{
	static int flag = -1;
	if (flag < 0) {
		flag = (sock_init() == E_OK ? 1 : 0);
		sockets_free = 32;
	}
	return (flag > 0);
}


/*============================================================================*/
int16_t __CDECL inet_host_addr (const char * name, int32_t * addr)
{
	int16_t ret = -1;

	if (init_iconnect()) {
		struct hostent * host = gethostbyname ((char*)name);
		if (host) {
			*addr = *(long*)host->h_addr;
			ret   = E_OK;
		}
	}

	return ret;
}


/*============================================================================*/
int32_t __CDECL inet_connect (int32_t addr, int32_t port, int32_t tout_sec)
{
	int32_t fh = -1;

	if (sockets_free <= 0) {
		fh = -35/*EMFILE*/;
	} else {
		clock_t timeout = 0;
		sockaddr_in s_in;
		s_in.sin_family = AF_INET;
		s_in.sin_port   = htons ((int16_t)port);
		s_in.sin_addr   = addr;
		do {
			if ((fh = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
				fh = -1;
				break;
			} else {
				int n = connect ((int32_t)fh, &s_in, (int32_t)sizeof(s_in));
				if (n == E_OK) {
					sfcntl ((int32_t)fh, F_SETFL, O_NDELAY);
					sockets_free--;
					break;
				}
				sclose ((int32_t)fh);
				fh = -1;
				if (!timeout) {
					timeout = clock() + tout_sec * CLK_TCK;
				} else if (n != -ETIMEDOUT || clock() < timeout) {
					break;
				}
			}
		} while (1);
	}

	return fh;
}


/*============================================================================*/
int32_t __CDECL inet_send (int32_t fh, const char * buf, size_t len, my_ssl_context *ssl_context)
{
	int32_t ret = 0;

	while (len) {
		short n = swrite ((int32_t)fh, buf, (int32_t)min(len, 16384l));
		if (n < 0) {
			ret = n;
			break;
		} else {
			ret += n;
			buf += n;
			len -= n;
		}
	}

	return ret;
}


/*============================================================================*/
int32_t __CDECL inet_recv (int32_t fh, char * buf, size_t len, my_ssl_context *ssl_context)
{
	int32_t ret = 0;

	while (len) {
		long n = sread ((int32_t)fh, buf, len);
		if (n < 0) {
			if (!ret) ret = n;
			break;
		} else if (n) {
			ret += n;
			buf += n;
			len -= n;
		} else { /* no data available yet */
			break;
		}
	}

	return ret;
}


/*============================================================================*/
void __CDECL inet_close (int32_t fh, my_ssl_context *ssl_context)
{
  if (ssl_context) { ldg_mbedtls_ssl_close_notify(ssl_context); }

	if (fh >= 0) {

		if (sclose ((int32_t)fh) == 0) sockets_free++;
	}
}


/*============================================================================*/
int32_t __CDECL inet_instat (int32_t fh)
{
	int32_t ret = -1;

	char buf[1024];
	ret = recv ((int32_t)fh, buf, sizeof(buf), (int)MSG_PEEK);

	return ret;
}

/*============================================================================*/
int32_t __CDECL inet_select (int32_t timeout, int32_t * rfds, int32_t * wfds) /* timeout is milliseconds */
{
	int32_t ret = 0;

	struct timeval to_in;
	fd_set * p_rf, * p_wf;
	if (rfds && *rfds) {
		static fd_set i_rf;
		char * c_rf = (char*)FD_ZERO (&i_rf);
		c_rf[0] = ((char*)&rfds)[3];
		c_rf[1] = ((char*)&rfds)[2];
		c_rf[2] = ((char*)&rfds)[1];
		c_rf[3] = ((char*)&rfds)[0];
		p_rf    = &i_rf;
	} else {
		p_rf    = NULL;
	}
	if (wfds && *wfds) {
		static fd_set i_wf;
		char * c_wf = (char*)FD_ZERO (&i_wf);
		c_wf[0] = ((char*)&wfds)[3];
		c_wf[1] = ((char*)&wfds)[2];
		c_wf[2] = ((char*)&wfds)[1];
		c_wf[3] = ((char*)&wfds)[0];
		p_wf    = &i_wf;
	} else {
		p_wf    = NULL;
	}
	to_in.tv_sec  = (int32_t)(timeout /1000);    /* calc seconds from milliseconds */
	to_in.tv_usec = (int32_t)((timeout%1000)*1000); /* calc remainder in microsecs */ 
	ret = select((int32_t)32, p_rf, p_wf, NULL, &to_in);
	if (p_rf) {
		char * c_rf = (char*)p_rf;
		((char*)&rfds)[3] = c_rf[0];
		((char*)&rfds)[2] = c_rf[1];
		((char*)&rfds)[1] = c_rf[2];
		((char*)&rfds)[0] = c_rf[3];
	}
	if (p_wf) {
		char * c_wf = (char*)p_wf;
		((char*)&wfds)[3] = c_wf[0];
		((char*)&wfds)[2] = c_wf[1];
		((char*)&wfds)[1] = c_wf[2];
		((char*)&wfds)[0] = c_wf[3];
	}

	return ret;
}


/*============================================================================*/
const char * __CDECL inet_info (void) { return "Iconnect"; }

/*============================================================================*/
const int16_t __CDECL inet_type (void) { return 3; }

/*============================================================================*/
void __CDECL inet_mbedtls_set (LDG_MBEDTLS_FTAB *ftab) { inet_ldg_mbedtls_ftab = ftab; }
