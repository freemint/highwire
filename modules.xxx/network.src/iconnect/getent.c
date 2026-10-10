#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconnect/sockerr.h>
#include <iconnect/socket.h>
#include <iconnect/netdb.h>
#include <iconnect/in.h>
#include <iconnect/inet.h>
#include <tos.h>
#include <iconnect/atarierr.h>
#include <iconnect/usis.h>
#include <iconnect/mt_sock.h>

extern	int set_flag(int bit_nr);
extern	void	clear_flag(int bit_nr);
#define MT_FILE_SEM 0

#include "getent.h"
/* Needed to separate DNS-hostent-FNs from etc/hosts-FNs */
#include "network.h"
extern cookie_struct *sint;

static UNI_ENT	uni;

static R_ENT	r_host={-1,0,HOSTS};
static R_ENT	r_net={-1,0,NETWORKS};
static R_ENT	r_serv={-1,0,SERVICES};
static R_ENT	r_proto={-1,0,PROTOKOLS};
static R_ENT	r_rpc={-1,0,RPC};

/* TOS File handling */

int Eopen(char *fname)
{/* Open file <fname> in etc/ */
	USIS_REQUEST ur;
	char path[256];
	
	ur.request=UR_ETC_PATH;
	ur.result=path;
	if(sint->user->usis(&ur) != UA_FOUND)
		return(-1);
	strcat(path, fname);
	return((int)Fopen(path, FO_READ));
}

int Freads(int fh, char *buf, int len)
{/* Liest bis LF. Gibt Bytes oder <=0(error) zurck */
	int red;
	char *b;
	char					ign;
	long err;
	long					opos;

	/* Fill buffer */
	while(1)
	{
		opos=Fseek(0,fh,1);
		/* Reserve one Byte for Memory-Termination, if last line in
				file is not terminated */
		err=Fread(fh, len-1, buf);
		if(err <=0) return((int)err);
	
		/* Search for end of buffer (Line Feed) */
		b=buf;
		red=0;
		while((b[red] !=10) && (red <err))++red;
	
		if(b[red]==10)	/* End found */
		{
			++red;	/* Count LF byte */
			Fseek(opos+red, fh,0);	/* correct Fpos */
			return(red);
		}

		/* No LF found, Line too long or last line in File is unterminated */
		if((err<len-1)||(Fread(fh, 1, &ign)==0))	/* EOF */
		{
			buf[red++]=10;	/* Terminate in Memory and count LF byte*/
			return(red);
		}
		
		/* Line longer than buf, ignore it */
		while(ign!=10)
		{
			err=Fread(fh, 1, &ign);
			if(err<=0) return((int)err);	/* Error or EOF */
		}
	}
}

/* unique */

int setent(R_ENT *r_ent, int stayopen)
{
	/* File already open? */
	if(r_ent->fhandle > -1)
		Fclose(r_ent->fhandle);
		
	/* Open File */
	r_ent->fhandle=Eopen(r_ent->basefile);
	r_ent->stayopen=stayopen;
	if(r_ent->fhandle >= 0)
		return(E_OK);
	return(r_ent->fhandle);
}

int endent(R_ENT *r_ent)
{
	if(r_ent->fhandle > -1)
	{
		Fclose(r_ent->fhandle);
		r_ent->fhandle=-1;
	}
	return(E_OK);
}

int getent(R_ENT *r_ent)
{/* Return 0 or number of words in line */
	register int 	ix;
	register char	*p;
	
	if(r_ent->fhandle < 0)
		if(setent(r_ent, 0) < 0)
			return(0);
	
	/* Read one line */
	do
	{
		if(Freads(r_ent->fhandle, uni.first, MAX_LINE) <= 0)
			return(0);
	
		/* Put EOL */
		p=uni.first;
		while((*p!='#')&&(*p!=10)&&(*p!=13))++p;
		*p=0;
	}while(strlen(uni.first)==0);
	
	/* Split line */
	p=uni.first; ix=0;
	
	while(1)
	{
		while(*p && (*p!=' ') && (*p!=9))++p;	/* Omit word */
		if(!*p) break;									/* Break on EOL */
		*p++=0; 												/* Terminate string */
		while((*p==' ')||(*p==9)) ++p;	/* Omit whitespace */
		if(!*p) break;									/* Break on EOL */
		uni.others[ix++]=p;							/* Set pointer to new word */
		if(ix==MAX_ENT) break;					/* Break on static limit */
	}

	uni.others[ix++]=NULL;
	
	if(r_ent->stayopen==0)
		endnetent();
		
	return(ix);
}	

/* host */

struct hostent	*MT_gethostbyname(const char *name, uchar *buf, ulong *ip, ulong **rip, struct hostent *host)
{
	struct hostent *p;
	char **cp;

	if(sint->defs->name_server_ip!=INADDR_NONE)
		return(MT_dns_gethostbyname(name, buf, ip, rip, host));

	while(!set_flag(MT_FILE_SEM));
	sethostent(r_host.stayopen);
	while ((p = gethostent())!=NULL)
  {
		if (stricmp(p->h_name, name) == 0)
			break;
		for (cp = p->h_aliases; *cp != 0; cp++)
			if (stricmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!r_host.stayopen)
		endhostent();
	clear_flag(MT_FILE_SEM);
	return (p);
}

struct hostent		*gethostbyname(const char *name)
{
	struct hostent *p;
	char **cp;

	if(sint->defs->name_server_ip!=INADDR_NONE)
		return(dns_gethostbyname(name));
		
	sethostent(r_host.stayopen);
	while ((p = gethostent())!=NULL)
  {
		if (stricmp(p->h_name, name) == 0)
			break;
		for (cp = p->h_aliases; *cp != 0; cp++)
			if (stricmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!r_host.stayopen)
		endhostent();
	return (p);
}

struct hostent		*gethostbyaddr(const void *addr, socklen_t len, int type)
{
	struct hostent *p;
	unsigned long	soll;
	int	ix;

	if(sint->defs->name_server_ip!=INADDR_NONE)
		return(dns_gethostbyaddr(addr, len, type));
			
	if(type!=AF_INET) return(NULL);
	if(len!=(int)sizeof(unsigned long)) return(NULL);
	
	soll=*(ulong*)addr;
	
	sethostent(r_host.stayopen);
	while ((p = gethostent())!=NULL)
	{
		ix=0;
		do
		{
			if(soll==*(ulong*)(p->h_addr_list[ix++]))
				goto found;
		}while(p->h_addr_list[ix]);
		p=NULL;
	}
found:
	if (!r_net.stayopen)
		endnetent();
	return (p);
}

struct hostent		*gethostent(void)
{
	static	unsigned long addr_list[MAX_ADD];
	static	unsigned long	*p_addr_list[MAX_ADD+1];
	static 	struct hostent	my_ent;
	int 		ret,n;
	unsigned long	addr;
	
	if(sint->defs->name_server_ip!=INADDR_NONE)
		return(dns_gethostent());
		
	ret=getent(&r_host);
	if(ret<1) return(NULL);
	
	/* read all internet addresses of host */
	addr=inet_addr(uni.first);
	if(addr!=INADDR_NONE)
	{
		addr_list[0]=addr;
		p_addr_list[0]=&(addr_list[0]);
	}
	n=1;
	while((addr!=INADDR_NONE) && (n < ret))
	{
		addr=inet_addr(uni.others[n-1]);
		if(addr!=INADDR_NONE)
		{
			if(n<MAX_ADD)
			{
				addr_list[n]=addr;
				p_addr_list[n]=&(addr_list[n]);
			}
			++n;
		}
	}
	if(n>=MAX_ADD)
		p_addr_list[MAX_ADD+1]=NULL;
	else
		p_addr_list[n]=NULL;
		
	if(n>=ret) /* Illegal line, get next */
		return(gethostent());
		
	my_ent.h_name=uni.others[n-1];
	my_ent.h_aliases=&(uni.others[n]);
	my_ent.h_addrtype=AF_INET;
	my_ent.h_length=(int)sizeof(unsigned long);
	(ulong**)(my_ent.h_addr_list)=&(p_addr_list[0]);
	return(&my_ent);
}

void			sethostent(int stayopen)
{
	if(sint->defs->name_server_ip!=INADDR_NONE)
		dns_sethostent(stayopen);
	else
		setent(&r_host, stayopen);
}

void endhostent(void)
{
	if(sint->defs->name_server_ip!=INADDR_NONE)
		dns_endhostent();
	else
		endent(&r_host);
}

/* net */

struct netent		*getnetbyname(const char *name)
{
	struct netent *p;
	char **cp;

	setnetent(r_net.stayopen);
	while ((p = getnetent())!=NULL)
  {
		if (strcmp(p->n_name, name) == 0)
			break;
		for (cp = p->n_aliases; *cp != 0; cp++)
			if (strcmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!r_net.stayopen)
		endnetent();
	return (p);
}

struct netent *getnetbyaddr(unsigned long net, int type)
{
	struct netent *p;

	setnetent(r_net.stayopen);
	while ((p = getnetent())!=NULL)
	{
		if ((p->n_net==net)&&(p->n_addrtype==type))
			break;
	}
	if (!r_net.stayopen)
		endnetent();
	return (p);
}

struct netent		*getnetent(void)
{
	static struct netent	my_ent;
	int ret=getent(&r_net);
	
	if(ret<1) return(NULL);
	if(ret<2) return(getnetent()); /* Ilegal line->get next */
		
	my_ent.n_name=uni.first;
	my_ent.n_aliases=&(uni.others[1]);
	my_ent.n_addrtype=AF_INET;
	my_ent.n_net=atoi(uni.others[0]);
	return(&my_ent);
}

void			setnetent(int stayopen)
{
	setent(&r_net, stayopen);
}

void		endnetent(void)
{
	endent(&r_net);
}

/* serv */

struct servent	*getservbyname(const char *name, const char *proto)
{
	struct servent *p;
	char **cp;
	setservent(r_serv.stayopen);
	while ((p = getservent())!=NULL)
  {
		if(strcmp(p->s_proto, proto)!=0)
			continue;
			
		if (strcmp(p->s_name, name) == 0)
			break;

		for (cp = p->s_aliases; *cp != 0; cp++)
			if (strcmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!r_serv.stayopen)
		endservent();
	return (p);
}

struct servent	*getservbyport(int port, const char *proto)
{
	struct servent *p;

	setservent(r_serv.stayopen);
	while ((p = getservent())!=NULL)
  {
		if (p->s_port != port)
			continue;
		if (proto == 0 || strcmp(p->s_proto, proto) == 0)
			break;
	}
	if (!r_serv.stayopen)
		endservent();
	return (p);
}

struct servent	*getservent(void)
{
	static struct servent	my_ent;
	int ret=getent(&r_serv);
	char	*slash;
	
	if(ret<1) return(NULL);
	if(ret<2) return(getservent()); /* Illegal line->get next */
	
	slash=strchr(uni.others[0],'/');
	if(slash==NULL) return(getservent()); /* Illegal line->get next */
	*slash++=0;
	
	my_ent.s_name=uni.first;
	my_ent.s_aliases=&(uni.others[1]);
	my_ent.s_port=atoi(uni.others[0]);
	my_ent.s_proto=slash;
	return(&my_ent);
}

void setservent(int stayopen)
{
	setent(&r_serv, stayopen);
}

void endservent(void)
{
	endent(&r_serv);
}

/* proto */

struct protoent	*getprotobyname(const char *name)
{
	struct protoent *p;
	char **cp;

	setprotoent(r_proto.stayopen);
	while ((p = getprotoent())!=NULL)
  {
		if (strcmp(p->p_name, name) == 0)
			break;
		for (cp = p->p_aliases; *cp != 0; cp++)
			if (strcmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!r_proto.stayopen)
		endprotoent();
	return (p);
}

struct protoent	*getprotobynumber(int proto)
{
	struct protoent *p;

	setprotoent(r_proto.stayopen);
	while ((p = getprotoent())!=NULL)
	{
		if (p->p_proto==proto)
			break;
	}
	if (!r_proto.stayopen)
		endprotoent();
	return (p);
}

struct protoent	*getprotoent(void)
{
	static struct protoent	my_ent;
	int ret=getent(&r_proto);
	
	if(ret<1) return(NULL);
	if(ret<2) return(getprotoent()); /* Illegal line->get next */
	
	my_ent.p_name=uni.first;
	my_ent.p_aliases=&(uni.others[1]);
	my_ent.p_proto=atoi(uni.others[0]);
	return(&my_ent);
}

void setprotoent(int stayopen)
{
	setent(&r_proto, stayopen);
}

void endprotoent(void)
{
	endent(&r_proto);
}

/* rpc */

struct rpcent		*getrpcbyname(const char *name)
{
	register struct rpcent *p;
	register char **cp;

	setrpcent(r_rpc.stayopen);
	while ((p = getrpcent())!=NULL)
  {
		if (strcmp(p->r_name, name) == 0)
			break;
		for (cp = p->r_aliases; *cp != 0; cp++)
			if (strcmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!r_rpc.stayopen)
		endrpcent();
	return (p);
}

struct rpcent		*getrpcbynumber(int number)
{
	struct rpcent *p;

	setrpcent(r_rpc.stayopen);
	while ((p = getrpcent())!=NULL)
	{
		if (p->r_number==number)
			break;
	}
	if (!r_rpc.stayopen)
		endrpcent();
	return (p);
}

struct rpcent		*getrpcent(void)
{
	static struct rpcent	my_ent;
	int ret=getent(&r_rpc);
	
	if(ret<1) return(NULL);
	if(ret<2) return(getrpcent()); /* Illegal line->get next */
		
	my_ent.r_name=uni.first;
	my_ent.r_aliases=&(uni.others[1]);
	my_ent.r_number=(int)atol(uni.others[0]);
	return(&my_ent);
}

void setrpcent(int stayopen)
{
	setent(&r_rpc, stayopen);
}

void endrpcent(void)
{
	endent(&r_rpc);
}
