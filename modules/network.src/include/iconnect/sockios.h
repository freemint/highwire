#if !defined(__SOCKIOS__)
#define __SOCKIOS__
#include <iconnect/types.h>

#undef select
extern int cdecl select(int nfds, fd_set	*readlist,  fd_set *writelist, fd_set *exceptlist, struct timeval *TimeOut);

#endif
