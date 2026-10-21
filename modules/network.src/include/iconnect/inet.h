#if  !defined( __INET__ )
#define __INET__

#if  !defined( __IN__ )
#include <iconnect/in.h>
#endif

extern unsigned long inet_addr(const char *cpp);
extern unsigned long inet_network(const char *cp);
extern char *inet_ntoa(unsigned long in);
extern unsigned long	inet_lnaof(struct in_addr in);
extern struct in_addr	inet_makeaddr(unsigned long net, unsigned long host);
extern unsigned long	inet_aton(const char *cpp, struct in_addr *addr);
/*extern unsigned long	inet_netof(struct in_addr);*/
#endif
