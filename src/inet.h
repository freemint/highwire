#ifndef __HW_INET_H__
#define __HW_INET_H__


#include "ldg.h"
#include "mbedTLS.h"

/*------------------------------------------------------------------------------
 * Function table structure to store either builtin fallback functions (no OVL
 * loaded) or the functions provided by a loaded network OVL.
 * All parameters must either be long or pointer (all 4 byte types) to guarantee
 * stack compatibility in case that the calling application and the OVL are made
 * with different compilers.
 * The application must never call any of these functions directly!
*/
typedef struct {
	int16_t __CDECL (*host_addr)   (const char * host, int32_t * addr);
	int32_t  __CDECL (*connect)     (int32_t addr, int32_t port, int32_t tout_sec);
	int32_t  __CDECL (*send)        (int32_t fh, const char * buf, size_t len, my_ssl_context *ssl_context);
	int32_t  __CDECL (*recv)        (int32_t fh, char       * buf, size_t len, my_ssl_context *ssl_context);
	void  __CDECL (*close)       (int32_t fh, my_ssl_context *ssl_context);
	int32_t  __CDECL (*instat)      (int32_t fh);
	int32_t  __CDECL (*select)      (int32_t timeout, int32_t * rfds, int32_t * wfds);
	const char * __CDECL (*info) (void);
	const int16_t __CDECL (*type)    (void);
	void  __CDECL (*mbedtls_set) (LDG_MBEDTLS_FTAB *ftab);
} LDG_INET_FTAB;

#define inet_host_addr(h,a) (*inet_ftab->host_addr)(h,a)
#define inet_connect(a,p,t) (*inet_ftab->connect)(a,p,t)
#define inet_send(f,b,l,w)  (*inet_ftab->send)(f,b,l,w)
#define inet_recv(f,b,l,w)  (*inet_ftab->recv)(f,b,l,w)
#define inet_close(f,w)     (*inet_ftab->close)(f,w)
#define inet_instat(f)      (*inet_ftab->instat)(f)
#define inet_select(t,r,w)  (*inet_ftab->select)(t,r,w)
#define inet_info()         (*inet_ftab->info)()
#define inet_type()         (*inet_ftab->type)()
#define inet_mbedtls_set(f) (*inet_ftab->mbedtls_set)(f)

extern LDG_INET_FTAB *inet_ftab;

void ldg_inet_init(WORD *gl);
LDG *ldg_inet_load(void);
BOOL ldg_has_inet(void);
void ldg_inet_unload(void);


/* if no module is loaded (ie no use of internet calls, html browser only), the inetfab points to these noop functions */

int16_t no_inet_host_addr(const char * name, int32_t * addr);
int32_t no_inet_connect(int32_t addr, int32_t port, int32_t tout_sec);
int32_t no_inet_send(int32_t fh, const char * buf, size_t len, my_ssl_context *ssl);
int32_t no_inet_recv(int32_t fh, char * buf, size_t len, my_ssl_context *ssl);
void no_inet_close(int32_t fh, my_ssl_context *ssl);
int32_t no_inet_instat(int32_t fh);
int32_t no_inet_select(int32_t timeout, int32_t * rfds, int32_t * wfds);
const char * no_inet_info(void);
const int16_t no_inet_type(void);
void no_inet_mbedtls_set(LDG_MBEDTLS_FTAB *ftab);

#endif /*__HW_INET_H__*/
