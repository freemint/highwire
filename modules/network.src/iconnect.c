
#define CLK_TCK 200
#define __CDECL cdecl

#include <stddef.h>
#include <time.h>
#include <string.h>

#include <misc/hw-types.h>
#include <misc/ldg.h>

#include <iconnect/sockinit.h>
#include <iconnect/netdb.h>
#include <iconnect/types.h>
#include <iconnect/socket.h>
#include <iconnect/in.h>
#include <iconnect/sfcntl.h>
#include <iconnect/sockios.h>
#include <iconnect/types.h>

WORD sockets_free = 0;

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
int __CDECL inet_host_addr (const char * name, long * addr)
{
	int ret = -1;

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
long __CDECL inet_connect (long addr, long port, long tout_sec)
{
	int fh = -1;

	if (sockets_free <= 0) {
		fh = -35/*EMFILE*/;
	} else {
		clock_t timeout = 0;
		sockaddr_in s_in;
		s_in.sin_family = AF_INET;
		s_in.sin_port   = htons ((short)port);
		s_in.sin_addr   = addr;
		do {
			if ((fh = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
				fh = -1;
				break;
			} else {
				int n = connect ((int)fh, &s_in, (int)sizeof(s_in));
				if (n == E_OK) {
					sfcntl ((int)fh, F_SETFL, O_NDELAY);
					sockets_free--;
					break;
				}
				sclose ((int)fh);
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
long __CDECL inet_send (long fh, const char * buf, size_t len, void *ssl_context)
{
	long ret = 0;

	if (ssl_context != NULL) { return ret; }

	while (len) {
		short n = swrite ((int)fh, buf, (int)min(len, 16384l));
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
long __CDECL inet_recv (long fh, char * buf, size_t len, void *ssl_context)
{
	long ret = 0;

	if (ssl_context != NULL) { return ret; }

	while (len) {
		int n = sread ((int)fh, buf, len);
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
void __CDECL inet_close (long fh, void *ssl_context)
{
	if (ssl_context) { return; }

	if (fh >= 0)
	{
		if (sclose ((int)fh) == 0) sockets_free++;
	}
}


/*============================================================================*/
long __CDECL inet_instat (long fh)
{
	long ret = -1;

	char buf[1024];
	ret = recv ((int)fh, buf, sizeof(buf), (int)MSG_PEEK);

	return ret;
}

/*============================================================================*/
long __CDECL inet_select (long timeout, long * rfds, long * wfds) /* timeout is milliseconds */
{
	long ret = 0;

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
	to_in.tv_sec  = (int)(timeout /1000);    /* calc seconds from milliseconds */
	to_in.tv_usec = (int)((timeout%1000)*1000); /* calc remainder in microsecs */
	ret = select((int)32, p_rf, p_wf, NULL, &to_in);
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
const int __CDECL inet_type (void) { return 3; }

/*============================================================================*/
void __CDECL inet_mbedtls_set (void *ftab) { if (ftab != NULL) { } }

/*============================================================================*/



PROC LibFunc[] =
{
	{"inet_host_addr", "int host_addr(const char * host, long * addr);\n", inet_host_addr},
  {"inet_connect", "long connect(long addr, long port, long tout_sec);\n", inet_connect},

  {"inet_send", "long send(long fh, const char * buf, size_t len, my_ssl_context *ssl_context);\n", inet_send},
  {"inet_recv", "long recv(long fh, char       * buf, size_t len, my_ssl_context *ssl_context);\n", inet_recv},
  {"inet_close", "void close(long fh, my_ssl_context *ssl_context);\n", inet_close},

  {"inet_instat", "long instat(long fh);\n", inet_instat},
  {"inet_select", "long select (long timeout, long * rfds, long * wfds);;\n", inet_select},

  {"inet_info", "const char * info(long fh);\n", inet_info},
  {"inet_type", "const int type(void);\n", inet_type},
	{"inet_mbedtls_set", "void mbedtls_set(void *ftab);\n", inet_mbedtls_set}
};

LDGLIB LibLdg[] = { { 0x0001,  10, LibFunc,  "IConnect overlay for HighWire, by AltF4@freemint.de", 1 } };

int main(void)
{
  ldg_init(LibLdg);
  return 0;
}
