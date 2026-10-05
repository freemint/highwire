
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <mintbind.h>
#include <errno.h>

/*----------------------------------------------------------------------------*/
static BOOL init_mintnet (void)
{
	static BOOL __once = TRUE;
	if (__once) {
		int16_t n;
		sockets_free = 32;
		for (n = 0; n < 32; n++) {
			if (Finstat (n) >= 0 || Foutstat (n) >= 0) sockets_free--;
		}
		if ((sockets_free -= 2) > 0) { /* always save two free handles */
			__once = FALSE;
		} else {
			sockets_free = 0;
		}
	}
	return (sockets_free > 0);
}



/*============================================================================*/
int16_t CDECL inet_host_addr (const char * name, int32_t * addr)
{
	int16_t ret = -1;

	struct hostent * host = gethostbyname (name);
	if (host) {
		*addr = *(int32_t*)host->h_addr;
		ret   = E_OK;
	} else {
		ret   = -errno;
	}
	return ret;
}


/*----------------------------------------------------------------------------*/

static BOOL sig_tout = FALSE;

#ifndef __mint_sighandler_t_defined
typedef void *__mint_sighandler_t;
#endif

static void __CDECL sig_alrm (int32_t sig)
{
	(void)sig;
	sig_tout = TRUE;
}

/*============================================================================*/
int32_t CDECL inet_connect (int32_t addr, int32_t port, int32_t tout_sec)
{
	int32_t fh = -1;

	if (!init_mintnet()) {
		fh = -35/*EMFILE*/;
	} else {
		struct sockaddr_in s_in;
		s_in.sin_family = AF_INET;
		s_in.sin_port   = htons ((int16_t)port);
		s_in.sin_addr.s_addr   = addr;
		if ((fh = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
			fh = -errno;
		} else {
			__mint_sighandler_t alrm = (__mint_sighandler_t)Psignal (14/*SIGALRM*/, sig_alrm);
			if ((long)alrm >= 0) {
				sig_tout = FALSE;
				Talarm (tout_sec);
			}
			if (connect (fh, (struct sockaddr *)&s_in, sizeof (s_in)) < 0) {
				close (fh);
				fh = -(sig_tout && errno == EINTR ? ETIMEDOUT : errno);
			} else {
				sockets_free--;
			}
			if ((long)alrm >= 0) {
				Talarm (0);
				Psignal (14/*SIGALRM*/, alrm);
			}
		}
	}

	return fh;
}


/*============================================================================*/
int32_t CDECL inet_send (int32_t fh, const char * buf, size_t len, my_ssl_context *ssl_context)
{
	int32_t ret = 0;

  if (ssl_context)
  {
    ret = MBEDTLS_ERR_SSL_WANT_WRITE;
    
    while ((ret = ldg_mbedtls_ssl_write(ssl_context, buf, len)) <= 0)
    {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) { break; }
    }
  }
  else
  {
    while (len)
    {
      int32_t n = Fwrite (fh, len, buf);
      
      if (n < 0) { ret = n; break; } else { ret += n; buf += n; len -= n; }
    }
  }

	return ret;
}


/*============================================================================*/
int32_t CDECL inet_recv (int32_t fh, char * buf, size_t len, my_ssl_context *ssl_context)
{
	int32_t ret = 0;

  if (ssl_context)
  {
    memset(buf, 0, len);
    while(len)
    {
      int32_t n = Finstat (fh);
      if (n < 0) { if (!ret) { ret = n; } break; }
      else if (n == 0x7FFFFFFFL) { if (!ret) { ret = -ECONNRESET; } break; /* connection closed */ }
      else if (n && (n = ldg_mbedtls_ssl_read(ssl_context, buf, (n < len ? n : len))) < 0) { if (n != MBEDTLS_ERR_SSL_WANT_READ && n != MBEDTLS_ERR_SSL_WANT_WRITE) { if (!ret) { ret = -errno; } break; } }
      else if (n > 0) { ret += n; buf += n; len -= n; }
      else { break; /* no data available yet */ }
    }
  }
  else
  {
    while (len)
    {
      int32_t n = Finstat (fh);
      if (n < 0) { if (!ret) { ret = n; } break; }
      else if (n == 0x7FFFFFFFL) { if (!ret) { ret = -ECONNRESET; } break; /* connection closed */ }
      else if (n && (n = Fread (fh, (n < len ? n : len), buf)) < 0) { if (!ret) { ret = -errno; } break; }
      else if (n > 0) { ret += n; buf += n; len -= n; }
      else { break; /* no data available yet */ }
    }
  }

	return ret;
}


/*============================================================================*/
void CDECL inet_close (int32_t fh, my_ssl_context *ssl_context)
{
  if (ssl_context) { ldg_mbedtls_ssl_close_notify(ssl_context); }

	if (fh >= 0) {

		if (close ((int32_t)fh) == 0) sockets_free++;
	}
}


/*============================================================================*/
int32_t CDECL inet_instat (int32_t fh)
{
	int32_t ret = -1;

	ret = Finstat (fh);
	if (ret == 0x7FFFFFFFL) { /* connection closed */
		ret = -ECONNRESET;
	}

	return ret;
}

/*============================================================================*/
int32_t CDECL inet_select (int32_t timeout, int32_t * rfds, int32_t * wfds) /* timeout is milliseconds */
{
	int32_t ret = 0;

	ret = Fselect (timeout, rfds, wfds, NULL);

	return ret;
}


/*============================================================================*/
const char * CDECL inet_info (void) {	return "MiNTnet"; }

/*============================================================================*/
const int16_t CDECL inet_type (void) { return 1; }

/*============================================================================*/
void CDECL inet_mbedtls_set (LDG_MBEDTLS_FTAB *ftab) { inet_ldg_mbedtls_ftab = ftab; }
