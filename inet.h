#ifndef __HW_INET_H__
#define __HW_INET_H__


/*** Basic Stuff **************************************************************/

short inet_host_addr (const char * host, long * addr);

long inet_connect (long addr, long port);
long inet_send    (long fh, char * buf, size_t len);
long inet_recv    (long fh, char * buf, size_t len);
void inet_close   (long fh);

const char * inet_info (void);


#endif /*__HW_INET_H__*/
