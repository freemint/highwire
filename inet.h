#ifndef __HW_INET_H__
#define __HW_INET_H__


typedef struct {
	short __CDECL (*host_addr) (const char * host, long * addr);
	long  __CDECL (*connect) (long addr, long port);
	long  __CDECL (*send)    (long fh, char * buf, size_t len);
	long  __CDECL (*recv)    (long fh, char * buf, size_t len);
	void  __CDECL (*close)   (long fh);
	const char * __CDECL (*info) (void);
} INET_FTAB;


#ifndef USE_OVL

short __CDECL inet_host_addr (const char * host, long * addr);

long __CDECL inet_connect (long addr, long port);
long __CDECL inet_send    (long fh, char * buf, size_t len);
long __CDECL inet_recv    (long fh, char * buf, size_t len);
void __CDECL inet_close   (long fh);

const char * __CDECL inet_info (void);


#else /* USE_OVL */

extern INET_FTAB inet_ftab;

#define inet_host_addr(h,a) (*inet_ftab.host_addr)(h,a)
#define inet_connect(a,p)   (*inet_ftab.connect)(a,p)
#define inet_send(f,b,l)    (*inet_ftab.send)(f,b,l)
#define inet_recv(f,b,l)    (*inet_ftab.recv)(f,b,l)
#define inet_close(f)       (*inet_ftab.close)(f)
#define inet_info()         (*inet_ftab.info)()

#endif


#endif /*__HW_INET_H__*/
