#ifndef __HW_INET_H__
#define __HW_INET_H__


/*------------------------------------------------------------------------------
 * Function table structure to store either builtin fallback functions (no OVL
 * loaded) or the functions provided by a loaded network OVL.
 * All parameters must either be long or pointer (all 4 byte types) to guarantee
 * stack compatibility in case that the calling application and the OVL are made
 * with different compilers.
 * The application must never call any of these functions directly!
*/
typedef struct {
	short __CDECL (*host_addr) (const char * host, long * addr);
	long  __CDECL (*connect) (long addr, long port);
	long  __CDECL (*send)    (long fh, char * buf, size_t len);
	long  __CDECL (*recv)    (long fh, char * buf, size_t len);
	void  __CDECL (*close)   (long fh);
	const char * __CDECL (*info) (void);
} INET_FTAB;

#ifdef USE_OVL /* Defines for the application side, not only for the *
                * programmer's convience.                            */
#define inet_host_addr(h,a) (*inet_ftab.host_addr)(h,a)
#define inet_connect(a,p)   (*inet_ftab.connect)(a,p)
#define inet_send(f,b,l)    (*inet_ftab.send)(f,b,l)
#define inet_recv(f,b,l)    (*inet_ftab.recv)(f,b,l)
#define inet_close(f)       (*inet_ftab.close)(f)
#define inet_info()         (*inet_ftab.info)()

extern INET_FTAB inet_ftab; /* This must be global accessible. */


#else /* OVLs must implement all of these functions. */

short __CDECL inet_host_addr (const char * host, long * addr);
long  __CDECL inet_connect (long addr, long port);
long  __CDECL inet_send    (long fh, char * buf, size_t len);
long  __CDECL inet_recv    (long fh, char * buf, size_t len);
void  __CDECL inet_close   (long fh);
const char * __CDECL inet_info (void);

#endif


#endif /*__HW_INET_H__*/
