#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconnect/sockerr.h>
#include <iconnect/socket.h>
#include <iconnect/netdb.h>
#include <iconnect/inet.h>

#include "network.h"
#include "resolver.h"

extern cookie_struct *sint;

static char *nullp=NULL;
static char *aliases[18];
static char *addresses[18];
static char	mem_addresses[18][4];

ulong gethostid(void)
{
	return(sint->defs->my_ip);
}

int	gethostname(char *name, int namelen)
{
	char	buf[20];
	
	strcpy(buf, inet_ntoa(sint->defs->my_ip));
	strncpy(name, buf, namelen);
	return(0);
}

struct hostent	*dns_gethostent(void)
{/* We're always using the DNS, so: */
	return(NULL);
}

char *dns2asc(char *src)
{/* converts a dns-answer-name into an ascii-string
		(that means, all len-octets are changed to dots)
		and returns a pointer to the first char.
    e.g.:
src:    <3><nic><5><nordu><3><net><0>
after:  <.nic.nordu.net><0>
return:   ^
*/
	char	*ret=src+1, p;
	
	while(*src)
	{
		p=*src;
		*src='.';
		src+=p+1;
	}
	return(ret);
}

int divbuf(uchar *buf, struct hostent *h)
{/* buf contains a dns-answer, h will be filled */
	dns_header	*dnsh=(dns_header*)buf;
	rrec				*ans;
	uchar				*off;
	int					ac=dnsh->ancount, ixa, ixad, a;

	if(ac < 1)
		return(-1);

	ixa=ixad=0;

	if(dnsh->qdcount > 0)
	{/* Supress returned query */
		off=(uchar*)(dnsh->records);
		while(*off++); /* Supress name by finding trailing zero */
		off+=4;				 /* Supress qtype & qclass integers */
		ans=(rrec*)off;
	}
	else
		ans=dnsh->records;
	
	while(ac--)
	{
		if(get_uint((uchar*)&(ans->r_class)) == C_IN)
		{
			switch(get_uint((uchar*)&(ans->r_type)))
			{
				case T_A:
					if(ixad == 16) break;
					for(a=0; a < 4; ++a)
						mem_addresses[ixad][a]=ans->r_data[a];
					addresses[ixad]=&(mem_addresses[ixad][0]);
					++ixad;
				break;
				case T_CNAME:
					if(ixa == 16) break;
						aliases[ixa++]=dns2asc(ans->r_data);
				break;
			}
		}
		off=(uchar*)ans;
		off+=get_uint((uchar*)&(ans->r_size))+sizeof(rrec);
		ans=(rrec*)off;
	}
	addresses[ixad]=NULL;
	aliases[ixa]=NULL;
	
	h->h_aliases=aliases;
	h->h_addrtype=AF_INET;
	h->h_length=(int)sizeof(ulong);
	h->h_addr_list=addresses;
	return(0);
}

struct hostent	*dns_gethostbyname(const char *name)
{
	int							ierr;
	static uchar		buf[1024];
	static ulong		ip, *rip[2];
	static struct hostent	host;
	ip=inet_addr(name);
	if(ip != INADDR_NONE)
	{/* name was given as dottet decimals */
		host.h_name=(char*)name;
		host.h_aliases=&nullp;
		host.h_addrtype=AF_INET;
		host.h_length=(int)sizeof(ulong);
		host.h_addr_list=(char**)(rip);
		rip[0]=&ip;
		rip[1]=NULL;
		return(&host);
	}
	ierr=res_query((char*)name, C_IN, T_A, buf, 1024);
	if(ierr < 0)
	{
		return(NULL);
	}

	ierr=divbuf(buf, &host);
	if(ierr < 0)
		return(NULL);

	host.h_name=(char*)name;
	return(&host);
}

struct hostent	*MT_dns_gethostbyname(const char *name, uchar *buf, ulong *ip, ulong **rip, struct hostent *host)
{
	int							ierr;
	/* This variant does not use static variables. All static variables
	   must be supplied by caller.
	static uchar		buf[1024];
	static ulong		ip, *rip[2];
	static hostent	host;
	*/
	
	*ip=inet_addr(name);
	if(*ip != INADDR_NONE)
	{/* name was given as dottet decimals */
		host->h_name=(char*)name;
		host->h_aliases=&nullp;
		host->h_addrtype=AF_INET;
		host->h_length=(int)sizeof(ulong);
		host->h_addr_list=(char**)(rip);
		rip[0]=ip;
		rip[1]=NULL;
		return(host);
	}
	ierr=res_query((char*)name, C_IN, T_A, buf, 1024);
	if(ierr < 0)
	{
		return(NULL);
	}

	ierr=divbuf(buf, host);
	if(ierr < 0)
		return(NULL);

	host->h_name=(char*)name;
	return(host);
}

struct hostent *dns_gethostbyaddr(const char *addr, socklen_t len, int type)
{/* This is gethostbyname vice versa, a AF_INET-address is
		given and the name is looked up */

	/* Not supported */
	(void)addr;
	(void)len;
	(void)type;
	return(NULL);
}

int	dns_sethostent(int stayopen)
{/* Stream sockets are not supported */
	if(stayopen)
		return(EOPNOTSUPP);
	return(0);
}

int dns_endhostent(void)
{
	return(0);
}
