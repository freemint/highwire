
#include <stdio.h> /*printf/puts */
#include <string.h>
#include <mint/sysbind.h>
#include <time.h>

#include "include/sting/transprt.h"
#undef min
#define	min(x,y)   	(((x)<(y))?(x):(y))

#define TCP_OBUFF_SIZE   2048   /* TCP_open/TCP_send */

TPL * tpl = NULL;
DRV_LIST * drivers = NULL;

/*----------------------------------------------------------------------------*/
static BOOL init_stik (void)
{
	if (!tpl) {
		struct {
			int32_t cktag;
			int32_t ckvalue;
		}  * jar = (void*)Setexc (0x5A0 /4, (void (*)())-1);
		int32_t tag = 0x5354694BL; // 'STiK'
	
		/* Plain TOS up to 1.04 has no cookie jar unless something in AUTO
		 * made one, and then the pointer read above is NULL: walking it
		 * took the whole browser down on the first http URL.
		*/
		if (jar) while (jar->cktag) {
			if (jar->cktag == tag) {
				drivers = (DRV_LIST*)jar->ckvalue;
				if (strcmp (STIK_DRVR_MAGIC, drivers->magic) == 0) {
					tpl = (TPL*)get_dftab(TRANSPORT_DRIVER);
				}
				break;
			}
			jar++;
		}
		sockets_free = 32;
	}
	return (tpl != NULL);
}


/*============================================================================*/
int16_t __CDECL inet_host_addr (const char * name, int32_t * addr)
{
	int16_t ret = -1;

	if (init_stik()) {
		if (resolve ((char*)name, NULL, (uint32*)addr, 1) > 0) {
			ret = E_OK;
		}
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
int32_t __CDECL inet_connect (int32_t addr, int32_t port, int32_t tout_sec)
{
	int fh = -1;

	if (!init_stik()) {
	} else if (sockets_free <= 0) {
		fh = -35/*EMFILE*/;
	} else {
		__mint_sighandler_t alrm = (__mint_sighandler_t)Psignal (14/*SIGALRM*/, sig_alrm);
		if ((long)alrm >= 0) {
			sig_tout = FALSE;
			Talarm (tout_sec);
		}
		if ((fh = TCP_open (addr, (int16_t)port, 0, TCP_OBUFF_SIZE)) < 0) {
			fh = -(fh == -1001L ? ETIMEDOUT : 1);
		} else {
			sockets_free--;
		}
		if ((long)alrm >= 0) {
			Talarm (0);
			Psignal (14/*SIGALRM*/, alrm);
		}
	}

	return fh;
}


/*============================================================================*/
int32_t __CDECL inet_send (int32_t fh, const char * buf, size_t len, my_ssl_context *ssl_context)
{
	int32_t ret = 0;

	if (tpl)
  {
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
        int16 n = (int16)min(len, TCP_OBUFF_SIZE);
        int16 r;
        
        char *tmp = (char*)buf;
        
        if ((r = TCP_send ((int16_t)fh, tmp, n)) < E_OBUFFULL) { ret = -1; break; }
        else if (r == E_NORMAL) { ret += n; buf += n; len -= n; }
      }
    }
  }
 
	return ret;
}


/*============================================================================*/
int32_t __CDECL inet_recv (int32_t fh, char * buf, size_t len, my_ssl_context *ssl_context)
{
	int32_t ret = 0;

	if (tpl)
  {
    if (ssl_context)
    {
      memset(buf, 0, len);
      while (len)
      {
        int16_t n = CNbyte_count ((int16_t)fh);
		
        if (n < E_NODATA) { if (!ret) ret = (n == E_EOF || n == E_RRESET ? -ECONNRESET : -1); break; }
        else if (n > 0)
        {
          if (n > len) { n = len; }
			
          if ((n = ldg_mbedtls_ssl_read(ssl_context, buf, n)) < 0) { if (n != MBEDTLS_ERR_SSL_WANT_READ && n != MBEDTLS_ERR_SSL_WANT_WRITE) { if (!ret) { ret = -1; } break; } }
          else { ret += n; buf += n; len -= n; }
        }
        else { break; /* no data available yet */ }
      }
    }
    else
    {
      while (len)
      {
        int16_t n = CNbyte_count ((int16_t)fh);
		
        if (n < E_NODATA) { if (!ret) ret = (n == E_EOF || n == E_RRESET ? -ECONNRESET : -1); break; }
        else if (n > 0)
        {
          if (n > len) { n = len; }
			
          if ((n = CNget_block ((int16_t)fh, buf, n)) < 0) { if (!ret) ret = -1; break; }
          else { ret += n; buf += n; len -= n; }
        }
        else { break; /* no data available yet */ }
      }
    }
  }
  else
  {
    ret = -1;
  }

	return ret;
}


/*============================================================================*/
void __CDECL inet_close (int32_t fh, my_ssl_context *ssl_context)
{
  if (ssl_context) { ldg_mbedtls_ssl_close_notify(ssl_context); }

	if (fh >= 0) {

		if (tpl)
    {
			if (TCP_close ((int16_t)fh, 0, NULL) == 0) sockets_free++; // TODO: if SSL, TCP_close should be discarded.
		}
	}
}


/*============================================================================*/
int32_t __CDECL inet_instat (int32_t fh)
{
	int32_t ret = -1;

	if (tpl)
  {
		ret = CNbyte_count ((int16_t)fh);
		if (ret < E_NODATA) {
			ret = (ret == E_EOF || ret == E_RRESET ? -ECONNRESET : -1);
		}
	}

	return ret;
}

/*============================================================================*/
int32_t __CDECL inet_select (int32_t timeout, int32_t * rfds, int32_t * wfds) /* timeout is milliseconds */
{
	int32_t ret = 0;

	int16_t bit;
	int32_t rf_in, wf_in, rf_out, wf_out;
	rf_in = wf_in = rf_out = wf_out = 0;
	if (rfds) rf_in = *rfds;
	if (wfds) wf_in = *wfds;
	do {
		for (bit = 0; bit < 32; bit++) {
			int32_t mask = 1 << bit;
			if ((rf_in & mask) || (wf_in & mask)) {
				int16_t n = CNbyte_count ((int32_t)bit);
				if (n == E_BADHANDLE) {
					rf_out = wf_out = 0;
					ret = -1;
					break;
				}
				if ((rf_in & mask) && (n != 0)) {
					ret++;
					rf_out |= mask;
				}
				if (wf_in & mask) ret++;
			}
		}
	} while ((timeout == 0) && (ret == 0));
	if (ret > 0) wf_out = wf_in;
	if (rfds) *rfds = rf_out;
	if (wfds) *wfds = wf_out;

	return ret;
}


/*============================================================================*/
const char * __CDECL inet_info (void) { return "STinG/STiK2"; }

/*============================================================================*/
const int16_t __CDECL inet_type (void) { return 2; }

/*============================================================================*/
void __CDECL inet_mbedtls_set (LDG_MBEDTLS_FTAB *ftab) { inet_ldg_mbedtls_ftab = ftab; }
