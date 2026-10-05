#if  !defined( __NETDB__ )
#define __NETDB__

#if !defined( __UTYPES__ )
#define __UTYPES__
typedef	unsigned char uchar;
typedef	uchar					byte;
typedef unsigned short ushort;
typedef	unsigned int	uint;
typedef	unsigned long ulong;
#endif

struct hostent
{
	char	*h_name;
	char	**h_aliases;
	int		h_addrtype;
	int		h_length;
	char	**h_addr_list;
};
#define h_addr h_addr_list[0]

struct netent
{
	char	*n_name;
	char	**n_aliases;
	int		n_addrtype;
	ulong	n_net;
};

struct servent
{
  char  *s_name;  /* official service name */
  char  **s_aliases;  /* alias list */
  int		s_port;   /* port # */
  char  *s_proto; /* protocol to use */
};

struct protoent
{
	char	*p_name;
	char	**p_aliases;
	int		p_proto;
};

struct rpcent
{
	char	*r_name;
	char	**r_aliases;
	long	r_number;
};

extern struct hostent	*gethostbyname(const char *name);
extern struct hostent	*gethostbyaddr(const void *addr, int len, int type);
extern struct hostent	*gethostent(void);
extern void			sethostent(int stayopen);
extern void			endhostent(void);

extern struct netent		*getnetbyname(const char *name);
extern struct netent		*getnetbyaddr(unsigned long net, int type);
extern struct netent		*getnetent(void);
extern void			setnetent(int stayopen);
extern void			endnetent(void);

extern struct servent	*getservbyname(const char *name, const char *proto);
extern struct servent	*getservbyport(int port, const char *proto);
extern struct servent	*getservent(void);
extern void			setservent(int stayopen);
extern void			endservent(void);

extern struct protoent	*getprotobyname(const char *name);
extern struct protoent	*getprotobynumber(int proto);
extern struct protoent	*getprotoent(void);
extern void			setprotoent(int stayopen);
extern void			endprotoent(void);

extern struct rpcent		*getrpcbyname(const char *name);
extern struct rpcent		*getrpcbynumber(int number);
extern struct rpcent		*getrpcent(void);
extern void			setrpcent(int stayopen);
extern void			endrpcent(void);



/* This doesn't belong here */
extern ulong gethostid(void);
extern int		gethostname(char *name, int namelen);

#endif
