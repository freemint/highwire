#ifndef __HW_INET_H__
#define __HW_INET_H__


/*** Basic Stuff **************************************************************/

int  inet_host_addr (const char * host, long * addr);

int  inet_connect (long addr, short port);
long inet_send    (int fh, char * buf, size_t len);
long inet_recv    (int fh, char * buf, size_t len);
void inet_close   (int fh);

const char * inet_info (void);


#endif /*__HW_INET_H__*/
