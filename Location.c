/* @(#)highwire/Location.c
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h> /* printf */
#ifdef __PUREC__
# include <tos.h>
# include <ext.h>
#endif
#ifdef LATTICE
# include <dos.h>
#endif
#ifdef __GNUC__
# include <osbind.h>
#endif

#include "global.h"
#include "Location.h"
#include "Logging.h"
#include "inet.h"


static LOCATION __base  = NULL;
static LOCATION __local = NULL; /* map remote URLs to local dir */

char * local_web = NULL;


typedef struct s_host_entry {
	struct s_host_entry * Next;
	ULONG                 IdxTag;
	ULONG                 Ip;
	ULONG                 Flags;
	UWORD                 Length;
	char                  Name[1];
} * HOST_ENT;
static void * host_entry (const char ** name, UWORD max_len, BOOL resolve);
#define       host_addr(h)   (h ? h->Ip : 0uL)


typedef struct s_dir_entry {
	struct s_dir_entry * Next;
	ULONG                IdxTag;
	BOOL                 isTos;
	UWORD                Length;
	char                 Name[2];
} * DIR_ENT;
static void * dir_entry (const char ** name, DIR_ENT, BOOL local);

static long path_check (DIR_ENT);


/*----------------------------------------------------------------------------*/
static LOCATION
_alloc (DIR_ENT dir, const char * file)
{
	size_t   size = (file && *file ? strlen (file) : 0);
	LOCATION loc  = malloc (sizeof (struct s_location) + dir->Length + size);
	char   * ptr  = (char*)loc + offsetof (struct s_location, FullName);
	loc->__hash   = 0uL;
	loc->__reffs  = 0;
	loc->Proto    = PROT_FILE;
	loc->Port     = 0;
	loc->Flags    = 0uL;
	loc->Host     = NULL;
	loc->Dir      = dir;
	memcpy (ptr, dir->Name, dir->Length);
	loc->File     = ptr += dir->Length;
	loc->Path     = loc->FullName + (dir->isTos ? 2 : 0);
	if (!size) {
		*ptr = '\0';
		loc->Anchor = NULL;
	} else {
		ptr = memchr (memcpy (ptr, file, size +1), '#', size);
		if (ptr) {
			*ptr = '\0';
			if (!*(++ptr)) ptr = NULL;
		}
		loc->Anchor = ptr;
	}
	
	return loc;
}

/*============================================================================*/
LOCATION
new_location (const char * p_src, LOCATION base)
{
	const char * src = p_src;
	LOCATION loc;
	short    loc_proto = PROT_FILE;
	HOST_ENT loc_host  = NULL;
	short    loc_port  = 0;
	BOOL     read_host = FALSE;
	DIR_ENT  dir;
	
	if (!__base) {
		char         path[HW_PATH_MAX];
		const char * tmp = path;
		size_t len;
		path[0] = Dgetdrv();
		path[0] += (path[0] < 26 ? 'A' : -26 + '1');
		path[1] = ':';
		Dgetpath (path +2, 0);
		len = strlen (path);
		if (path[len -1] != '\\') {
			path[len++] = '\\';
			path[len]   = '\0';
		}
		__base = _alloc (dir_entry (&tmp, NULL, TRUE), NULL);
	}
	if (!base) {
		base = __base;
	}
	
	if (!src || !src[0]) {
		loc = base;
		loc->__reffs++;
		
		return loc;
	}
	
	if (src[1] == ':') {  /* TOS drive letter is A-Z[\]^_` or A-Z1-6 */
		base = NULL;
	
	} else if (src[0] == '/') {
		loc_proto = base->Proto;
		loc_port  = base->Port;
		loc_host  = base->Host;
		if (src[1] == '/'
		    && (PROTO_isRemote(loc_proto) || loc_proto == PROT_POP)) {
			src      += 2;
			read_host = TRUE;
		}
		base = NULL;
	
	} else { /* file, about, mailto, http, https, ftp */
		const char * s = src;
		char buf[8];
		short i = 0;
		
		while (isalpha(*s)) {
			if (i < sizeof(buf)) {
				buf[i++] = tolower(*(s++));
			} else {
				break;
			}
		}
		if (*s == ':') {
			buf[i] = '\0';
			if        (strcmp (buf, "file")   == 0) {
				loc_proto = PROT_FILE;
			} else if (strcmp (buf, "about")  == 0) {
				loc_proto = PROT_ABOUT;
			} else if (strcmp (buf, "mailto") == 0) {
				loc_proto = PROT_MAILTO;
			} else if (strcmp (buf, "pop")    == 0) {
				read_host = TRUE;
				loc_proto = PROT_POP;
				loc_port  = 110;
			} else if (strcmp (buf, "http")   == 0) {
				loc_proto = PROT_HTTP;
				loc_port  = 80;
			} else if (strcmp (buf, "https")  == 0) {
				loc_proto = PROT_HTTPS;
				loc_port  = 443;
			} else if (strcmp (buf, "ftp")    == 0) {
				loc_proto = PROT_FTP;
				loc_port  = 21;
			} else {
				loc_proto = PROT_Invalid;
			}
			src = (*(++s) == '/' && *(++s) == '/' ? ++s : s);
			if (PROTO_isLocal(loc_proto) || PROTO_isRemote(loc_proto)) {
				read_host = TRUE;
			}
			base = NULL;
		}
	}
	
	if (read_host) {
		const char * s = src;
		loc_host = host_entry (&s, 0, TRUE);
		if (loc_host) {
			if (*s == ':') {
				char * end;
				loc_port = strtoul (s +1, &end, 10);
				s        = end;
			}
			if (!loc_port) {
				loc_proto = PROT_FTP;
				loc_port  = 21; /* ftp */
			}
		/*** ugly hack to get yahoo's buggy standard login working */
			else if (loc_proto == PROT_HTTPS
			         && strncmp (loc_host->Name, "login.yahoo.", 12) == 0) {
				loc_proto = PROT_HTTP;
				loc_port  = 80;
			}
		/*** end of ugly hack */
			if (local_web && !host_addr (loc_host)) {
				s = src;
			}
		}
		src  = s;
	}
	
	if (local_web && PROTO_isRemote(loc_proto) && !host_addr (loc_host)) {
		if (!__local) {
			const char  * path = local_web;
			__local = _alloc (dir_entry (&path, NULL, TRUE), NULL);
		}
		base = __local;
	}
	
	if (base) {
		loc_proto = base->Proto;
		loc_port  = base->Port;
		loc_host  = base->Host;
		dir = dir_entry (&src, base->Dir, PROTO_isLocal(loc_proto));
	} else {
		dir = dir_entry (&src, NULL, PROTO_isLocal(loc_proto));
	}
	loc = _alloc (dir, src);
	loc->Proto = (!PROTO_isLocal(loc_proto)
	              ? loc_proto : loc->File[0] ? PROT_FILE : PROT_DIR);
	loc->Port  = loc_port;
	if (loc_host) {
		loc->Host  = loc_host;
		loc->Flags = loc_host->Flags;
	}
	loc->__reffs++;
	
	if (PROTO_isRemote (loc->Proto)) {
		loc->Flags |= path_check (dir);
	}
	
	if (logging_is_on) {
		char b_buf[1024], l_buf[1024];
		if (base) location_FullName (base, b_buf, sizeof(b_buf));
		else      b_buf[0] = '\0';
		location_FullName (loc, l_buf, sizeof(l_buf));
		logprintf (LOG_BLACK, "new_location('%s', '%s') returns '%s'\n",
		           p_src, b_buf, l_buf);
	}
	
	return loc;
}

/*============================================================================*/
void
free_location (LOCATION * _loc)
{
	LOCATION loc = *_loc;
	if (loc) {
		if ((!loc->__reffs || !--loc->__reffs)
		    && loc != __base && loc != __local) {
			free (loc);
		}
		*_loc = NULL;
	}
}


/*============================================================================*/
LOCATION
location_share  (LOCATION loc)
{
	if (loc) {
		loc->__reffs++;
	}
	return loc;
}


/*============================================================================*/
ULONG
_loc_Hash (LOCATION loc)
{
	ULONG hash = 0;
	if (*loc->File) {
		const char * src = strchr (loc->File, '\0');
		do {
			hash =  ((long)hash < 0 ? (hash <<1) | 1 : (hash <<1));
			hash ^= (ULONG)*(--src);
		} while (src > loc->File);
	}
	hash ^= (ULONG)loc->Dir ^ (ULONG)loc->Host;
	
	return (loc->__hash = (hash ? hash : 0xFFFFFFFFuL));
}


/*============================================================================*/
size_t
location_FullName (LOCATION loc, char * buffer, size_t max_len)
{
	char       * dst = buffer;
	DIR_ENT      dir;
	const char * src;
	
	if (!loc) {
		const char inv[] = "<nil>";
		size_t     len   = sizeof(inv);
		if (len > max_len) {
			len = max_len;
		}
		memcpy (dst, inv, len);
		return len;
	}
	
	dir = loc->Dir;
	
	switch (loc->Proto) {
		case PROT_ABOUT:  src = "about:";   break;
		case PROT_MAILTO: src = "mailto:";  break;
		case PROT_POP:    src = "pop://";   break;
		case PROT_HTTP:   src = "http://";  break;
		case PROT_HTTPS:  src = "https://"; break;
		case PROT_FTP:    src = "ftp://";   break;
		default:          src = (!dir || dir->isTos ? NULL : "file://");
	}
	if (src) {
		size_t len = strlen (src);
		if (len > max_len) {
			len = max_len;
		}
		memcpy (dst, src, len);
		dst     += len;
		max_len -= len;
	}
	
	if (max_len && loc->Host) {
		HOST_ENT host = loc->Host;
		size_t   len  = strlen (host->Name);
		if (len > max_len) {
			len = max_len;
		}
		memcpy (dst, host->Name, len);
		dst     += len;
		max_len -= len;
	}
	
	if (max_len && dir) {
		size_t len = (dir->Length < max_len ? dir->Length : max_len);
		src        = dir->Name;
		if (PROTO_isPseudo(loc->Proto) && *src == '/') {
			src++;
			len--;
		}
		if (len) {
			memcpy (dst, dir->Name, len);
			dst     += len;
			max_len -= len;
		}
	}
	
	if (max_len && *loc->File) {
		size_t len = strlen (loc->File);
		if (len > max_len) {
			len = max_len;
		}
		memcpy (dst, loc->File, len);
		dst     += len;
		max_len -= len;
	}
	
	dst[max_len ? 0 : -1] = '\0';
	
	return (dst - buffer);
}


/*============================================================================*/
const char *
location_Path  (LOCATION loc, UWORD * opt_len)
{
	DIR_ENT dir = loc->Dir;
	if (opt_len) {
		*opt_len = dir->Length;
	}
	return dir->Name;
}

/*============================================================================*/
const char *
location_Host  (LOCATION loc, UWORD * opt_len)
{
	HOST_ENT     host = loc->Host;
	const char * name;
	UWORD        len;
	if (host) {
		name = host->Name;
		len  = host->Length;
	} else {
		name = "localhost";
		len  = 9;
	}
	if (opt_len) {
		*opt_len = len;
	}
	return name;
}


/*============================================================================*/
BOOL
location_equal (LOCATION a, LOCATION b)
{
	return (a == b || (a && b
	                   && *(long*)&a->Proto == *(long*)&b->Proto
	                   && a->Host == b->Host
	                   && a->Dir == b->Dir
	                   && ((!a->File[0] && !b->File[0])
	                       || strcmp (a->File, b->File) == 00)));
}


/*============================================================================*/
int
location_open (LOCATION loc, const char ** host_name)
{
	HOST_ENT host = loc->Host;
	int      sock = -1;
	
	if (!host) {
		*host_name = "";
	
	} else {
		*host_name = host->Name;
		
#ifdef USE_INET
		if (host->Ip) {
			sock = (int)inet_connect (host->Ip, loc->Port, conn_timeout);
		}
#endif /* USE_INET */
	}
	return sock;
}


/*******************************************************************************
 *
 * Database Stuff
 *
 */
typedef struct s_domain_entry {
	struct s_domain_entry * Next;
	ULONG                   Flags;
	UWORD                   Length;
	char                    Name[1];
} * DOMAIN_ENT;
static        DOMAIN_ENT  domain_base = NULL;
static        HOST_ENT    host_base   = NULL;
static struct s_dir_entry dir_base    = { NULL, 0ul, FALSE, 1, "/" };


/*============================================================================*/
const char *
location_DBdomain (const char * name, UWORD max_len, ULONG * p_flags)
{
	DOMAIN_ENT * ptr = &domain_base, ent;
	char  buf[258], c;
	UWORD len = 0;
	
	while (*name == '.' && max_len != 1) {
		name++;
		if (max_len) max_len--;
	}
	if (!max_len || max_len >= sizeof(buf)) {
		max_len = sizeof(buf) -1;
	}
	while ((c = *name) != '\0' && len < max_len) {
		if (isalpha(c)) {
			buf[len++] = tolower(c);
			name++;
		} else if (isdigit(c) || c == '.' || c == '-') {
			buf[len++] = c;
			name++;
		} else {
			break;
		}
	}
	if (len < 2) {
		*p_flags = 0ul;
		return NULL;
	}
	buf[len] = '\0';
	
	while ((ent = *ptr) != NULL && ent->Length < len) {
		ptr = &ent->Next;
	}
	while (ent) {
		if (ent->Length > len) {
			ent = NULL;
		} else if (memcmp (name, ent->Name, len) == 0) {
			break;
		} else {
			ent = *(ptr = &ent->Next);
		}
	}
	if (!ent && (long)*p_flags > 0l
	         && (ent = malloc (sizeof(struct s_domain_entry) + len)) != NULL) {
		ent->Flags  = 0uL;
		ent->Length = len;
		memcpy (ent->Name, buf, len +1);
		ent->Next = *ptr;
		*ptr      = ent;
	}
	if (ent) {
		ULONG flags = *p_flags;
		if (flags & 0x80000000uL) ent->Flags &= ~flags;
		else                      ent->Flags |=  flags;
		
		if (host_base) { /* update domain flags to matching host entries */
			HOST_ENT host = host_base;
			while (host->Length < len && (host = host->Next) != NULL);
			while (host && host->Length == len) {
				if (memcmp (host->Name, ent->Name, len) == 0) {
					host->Flags = ent->Flags;
					while ((host = host->Next) != NULL && host->Length == len);
					break;
				}
				host = host->Next;
			}
			while (host) { /* host->Length > len */
				const char * p = host->Name + (host->Length - len -1);
				if (*p == '.' && memcmp (++p, ent->Name, len) == 0) {
					host->Flags = ent->Flags;
				}
				host = host->Next;
			}
		}
		*p_flags = ent->Flags;
		name     = ent->Name;
	} else {
		*p_flags = 0ul;
		name     = NULL;
	}
	return name;
}


/*----------------------------------------------------------------------------*/
static HOST_ENT
host_store (const char * name, size_t len)
{
	HOST_ENT * ptr = &host_base, ent;
	
	while ((ent = *ptr) != NULL && ent->Length < len) {
		ptr = &ent->Next;
	}
	while (ent) {
		if (ent->Length > len) {
			ent = NULL;
		} else if (memcmp (name, ent->Name, len) == 0) {
			break;
		} else {
			ent = *(ptr = &ent->Next);
		}
	}
	if (!ent && (ent = malloc (sizeof(struct s_host_entry) + len)) != NULL) {
		ent->IdxTag = 0uL;
		ent->Ip     = 0uL;
		ent->Flags  = 0uL;
		ent->Length = len;
		memcpy (ent->Name, name, len +1);
		ent->Next = *ptr;
		*ptr      = ent;
		
		if (domain_base) { /* set host flags from domain entries */
			DOMAIN_ENT domain = domain_base;
			while (domain->Length < len) {
				const char * p = ent->Name + (len - domain->Length -1);
				if (*p == '.' && memcmp (++p, domain->Name, domain->Length) == 0) {
					ent->Flags |= domain->Flags;
				}
				if ((domain = domain->Next) == NULL) break;
			}
			while (domain && domain->Length == len) {
				if (memcmp (ent->Name, domain->Name, domain->Length) == 0) {
					ent->Flags |= domain->Flags;
					break;
				}
				domain = domain->Next;
			}
		}
	}
	return ent;
}

/*----------------------------------------------------------------------------*/
static HOST_ENT
host_search (unsigned long tag)
{
	HOST_ENT ent = host_base;
	while (ent && ent->IdxTag != tag) {
		ent = ent->Next;
	}
	return ent;
}

/*----------------------------------------------------------------------------*/
static void *
host_entry (const char ** name, UWORD max_len, BOOL resolve)
{
	HOST_ENT     ent;
	const char * n = *name;
	char  buf[258], c;
	UWORD len = 0;
	
	if (!max_len || max_len >= sizeof(buf)) {
		max_len = sizeof(buf) -1;
	}
	while ((c = *n) != '\0' && len < max_len) {
		if (isalpha(c)) {
			buf[len++] = tolower(c);
			n++;
		} else if (isdigit(c) || c == '.' || c == '-') {
			buf[len++] = c;
			n++;
		} else {
			break;
		}
	}
	buf[len] = '\0';
	*name    = n;
	
	if (!len || strcmp (buf, "localhost") == 0) {
		ent = NULL;
	} else {
		ent = host_store (buf, len);
	}
#ifdef USE_INET
	if (ent && !ent->Ip && resolve) {
		inet_host_addr (ent->Name, (long*)&ent->Ip);
	}
#endif /* USE_INET */
	
	return ent;
}

/*============================================================================*/
const char *
location_DBhost (const char * name, UWORD len, ULONG * p_flags)
{
	HOST_ENT ent = host_entry (&name, len, FALSE);
	if (ent) {
		ULONG flags = *p_flags;
		if (flags & 0x80000000uL) ent->Flags &= ~flags;
		else                      ent->Flags |=  flags;
		*p_flags = ent->Flags;
		name     = ent->Name;
	} else {
		*p_flags = 0ul;
		name     = NULL;
	}
	return name;
}


/*----------------------------------------------------------------------------*/
typedef struct s_path_entry {
	struct s_path_entry * Next;
	ULONG                 Flags;
	UWORD                 Length;
	char                  Name[1];
} * PATH_ENT;
static PATH_ENT path_base = NULL;
/* - - - - - - - - - - - - - - - - - - - - - - - */
static long
path_check (DIR_ENT dir)
{
	PATH_ENT ent = (dir->Length >= 4 ? path_base : NULL);
	long   flags = 0;
	while (ent) {
		if (dir->Length >= ent->Length && strstr (dir->Name, ent->Name)) {
			flags |= ent->Flags;
		}
		ent = ent->Next;
	}
	return flags;
}

/*============================================================================*/
const char *
location_DBpath (const char * name, UWORD _len, ULONG * p_flags)
{
	size_t   len = (_len ? _len : name ? strlen (name) : 0);
	PATH_ENT ent = (len >= 4 ? malloc (sizeof(struct s_path_entry) +len) : NULL);
	if (!len) {
		name = NULL;
	} else {
		ent->Next   = path_base;
		path_base   = ent;
		ent->Flags  = *p_flags;
		ent->Length = len;
		name        = memcpy (ent->Name, name, len +1);
	}
	return name;
}


/*----------------------------------------------------------------------------*/
static DIR_ENT
dir_store (const char * name, size_t len)
{
	DIR_ENT * p_dir = &dir_base.Next, dir;
	
	while ((dir = *p_dir) != NULL && dir->Length < len) {
		p_dir = &dir->Next;
	}
	while (dir) {
		if (dir->Length > len) {
			dir = NULL;
		} else if (memcmp (name, dir->Name, len) == 0) {
			break;
		} else {
			dir = *(p_dir = &dir->Next);
		}
	}
	if (!dir && (dir = malloc (sizeof(struct s_dir_entry) + len -1)) != NULL) {
		dir->IdxTag = 0uL;
		dir->isTos  = (name[1] == ':');
		dir->Length = len;
		memcpy (dir->Name, name, len +1);
		dir->Next = *p_dir;
		*p_dir    = dir;
	}
	return dir;
}

/*----------------------------------------------------------------------------*/
static DIR_ENT
dir_search (unsigned long tag)
{
	DIR_ENT ent = &dir_base;
	while (ent && ent->IdxTag != tag) {
		ent = ent->Next;
	}
	return ent;
}

/*----------------------------------------------------------------------------*/
static void *
dir_entry (const char ** p_name, DIR_ENT base, BOOL local)
{
	DIR_ENT dir = &dir_base;
	
	char buf[1024 +2] = "", * b = buf;
	
	const char * name = *p_name;
	size_t n_len   = 0;
	char   n_delim = '\0';
	char * slash   = NULL;
	
	if (name && *name) {
		const char * n = strchr (name, '\0'), * p;
		if ((p = memchr (name, '#', n - name)) != NULL && p < n) n = p;
		if ((p = memchr (name, '?', n - name)) != NULL && p < n) n = p;
		if ((p = memchr (name, '*', n - name)) != NULL && p < n) n = p;
		n_len = n - name;
		if      (memchr (name, '\\', n_len)) n_delim = '\\';
		else if (memchr (name, '/',  n_len)) n_delim = '/';
	}
	if (!n_delim && !base) {
		return dir; /*this should never occure, indicates an internal bug! */
	}
	
	if (base) {
		char * b_name  = base->Name   + (base->isTos ? 2 : 0);
		size_t b_max   = base->Length - (base->isTos ? 3 : 1);
		size_t b_len   = b_max;
		char   b_delim = (base->isTos ? '\\' : '/');
		
		while (*name == '.') {
			if (n_len == 1) {
				name += 1;
				n_len = 0;
			
			} else if (name[1] == '\\' || name[1] == '/') {
				name  += 2;
				n_len -= 2;
				while (n_len && (*name == '\\' || *name == '/')) {
					name++;
					n_len--;
				}
			
			} else if (name[1] == '.') {
				if (n_len == 2) {
					name += 2;
					n_len = 0;
				} else if (name[2] == '\\' || name[2] == '/') {
					name  += 3;
					n_len -= 3;
					while (n_len && (*name == '\\' || *name == '/')) {
						name++;
						n_len--;
					}
				} else {
					break;
				}
				while (b_len && b_name[--b_len] != b_delim);
			}
			if (!n_len) break;
		}
		*p_name = name;
		
		if (++b_len < b_max || n_len) {
			if (base->isTos) b_len += 2;
			memcpy (buf, base->Name, b_len);
			b    += b_len;
			slash = b -1;
		}
		*b = '\0';
		
		n_delim = b_delim;
	}
	
	if (n_len) {
		BOOL   delim = FALSE;
		char c = '\0';
		if (b + n_len > buf + sizeof(buf) -3) {
			n_len = (sizeof(buf) -3) - (b - buf);
		}
		while (n_len) {
			while (!delim) {
				c     = *(name++);
				delim = (c == '\\' || c == '/');
				if (delim) {
					slash  = b;
					*(b++) = n_delim;
				} else {
					*(b++) = c;
					if (!--n_len) {
						c = '\0';
						break;
					}
				}
			}
			if (delim) {
				while (--n_len) {
					c = *(name++);
					if (c != '\\' && c != '/') break;
				}
				*p_name = name - (n_len ? 1 : 0);
				delim   = FALSE;
			}
			if (c == '.') {
				if (n_len == 1) {
					*p_name = name;
					break;
				} else if (name[0] == '\\' || name[0] == '/') {
					name  += 1;
					n_len -= 1;
					delim  = TRUE;
				} else if (name[0] == '.' && 
				           (n_len == 2 || name[1] == '\\' || name[1] == '/')) {
					if (slash) {
						b = slash;
						while (b > buf) {
							if (*(--b) == n_delim) {
								slash = b;
								break;
							}
						}
						b = slash +1;
					} else {
						slash  = b;
						*(b++) = n_delim;
					}
					name  += 2;
					n_len -= 2;
					delim  = TRUE;
				} else {
					*(b++) = '.';
				}
			} else {
				--name;
			}
		}
		if (slash) {
			if (local) {
				n_len = b - slash -1; /* that much chars behind the slash */
			} else {
				b = slash +1;
			}
		}
	}
	*b = '\0';
	
	if (!slash) {
		b = buf;
	
	} else if (n_len) {
		DIR_ENT d     = &dir_base;
		size_t  b_len = b - buf +1; /* plus slash */
		*(b++) = n_delim;
		*(b)   = '\0';
		while ((d = d->Next) != NULL && d->Length < b_len);
		if (d) do {
			if (d->Length > b_len) {
				d = NULL;
				break;
			} else if (memcmp (buf, d->Name, b_len) == 0) {
				break;
			}
		} while ((d = d->Next) != NULL);
		if (d) {
			*p_name = name +1;
			dir = d;
			b   = buf;
		
		} else if ((Fattrib (buf, 0,0) & 0xFF1A) == 0x10) {
			*p_name = name +1;
		
		} else {		
			b  = slash +1;
			*b = '\0';
		}
	}
	
	if ((n_len = b - buf) > 1) {
		dir = dir_store (buf, n_len);
	
	} else if (!n_len && base) {
		dir = base;
	}
	
	return dir;
}


/*---------------------------------------------------------------------------*/
static void
clear_idx_tags (void)
{
	DIR_ENT dir = &dir_base;
	do {
		dir->IdxTag = 0ul;
	} while ((dir = dir->Next) != NULL);
	if (host_base) {
		HOST_ENT host = host_base;
		do {
			host->IdxTag = 0ul;
		} while ((host = host->Next) != NULL);
	}
}

/*---------------------------------------------------------------------------*/
static char * 
wr_hex (char * ptr, short len, long val)
{
	const char hex[] = "0123456789ABCDEF";
	short shift = (len -1) *4;
	do {
		*(ptr++) = hex[(BYTE)(val >> shift) & 0x0F];
	} while ((shift -= 4) >= 0);
	*(ptr++) = ':';
	return ptr;
}

/*---------------------------------------------------------------------------*/
static long
rd_hex (char ** ptr, short len)
{
	char * p = *ptr;
	long val = 0;
	while(1) {
		if      (*p >  'F') break;
		else if (*p >= 'A') val = (val <<4) | (*(p++) - 'A' +10);
		else if (*p >  '9') break;
		else if (*p >= '0') val = (val <<4) | (*(p++) - '0');
		else                break;
	}
	if (*p == ':' && p == *ptr + len) *ptr = p +1;
	else                              val  = -2;
	return val;
}

/*============================================================================*/
BOOL
location_wrIdx (FILE * file, LOCATION loc)
{
	char buf[30], * p = buf;
	
	if (!file) {
		clear_idx_tags();
		return FALSE;
	
	} else if (!loc) {
		return FALSE;
	}
	
	if (loc->Host) {
		HOST_ENT entry = loc->Host;
		if (!entry->IdxTag) {
			entry->IdxTag = (long)entry;
			*(p++) = 'H';
			*(p++) = ':';
			p = wr_hex (p, 8, entry->IdxTag);
			fwrite (buf, 1, p - buf, file);
			fputs  (entry->Name, file);
			p = buf;
			*(p++) = '\n';
		}
	}
	if (loc->Dir) {
		DIR_ENT entry = loc->Dir;
		if (!entry->IdxTag) {
			entry->IdxTag = (long)entry;
			*(p++) = 'D';
			*(p++) = ':';
			p = wr_hex (p, 8, entry->IdxTag);
			fwrite (buf, 1, p - buf, file);
			fputs  (entry->Name, file);
			p = buf;
			*(p++) = '\n';
		}
	}
	*(p++) = 'L';
	*(p++) = ':';
	p = wr_hex (p, 8, (long)loc->Host);
	p = wr_hex (p, 8, (long)loc->Dir);
	p = wr_hex (p, 2, loc->Proto);
	p = wr_hex (p, 4, loc->Port);
	fwrite (buf, 1, p - buf, file);
	fputs  (loc->File, file);
	fputc  ('\n', file);
	
	return TRUE;
}

/*============================================================================*/
LOCATION
location_rdIdx (FILE * file)
{
	HOST_ENT host = NULL;
	DIR_ENT  dir  = NULL;
	LOCATION loc  = NULL;
	char     buf[2048];
	
	if (!file) {
		clear_idx_tags();
		return NULL;
	
	} else if (!fgets (buf, (int)sizeof(buf), file)) {
		return NULL;
	}
	
	if (buf[0] == 'H' && buf[1] == ':') {
		char * ptr = buf +2;
		long   tag = rd_hex (&ptr, 8);
		size_t len = (tag > 0 ? strlen (ptr) : 0);
		while (len && isspace (ptr[len-1])) ptr[--len] = '\0';
		if (!len) {
			puts ("location_rdIdx(): zero host name.");
			return NULL;
		} else if ((host = host_store (ptr, len)) == NULL) {
			puts ("location_rdIdx(): memory exhausted at host name.");
			return NULL;
		} else {
			host->IdxTag = tag;
			if (!fgets (buf, (int)sizeof(buf), file)) {
				puts ("location_rdIdx(): unexpected EOF after host name.");
				return NULL;
			}
		}
	}
	if (buf[0] == 'D' && buf[1] == ':') {
		char * ptr = buf +2;
		long   tag = rd_hex (&ptr, 8);
		size_t len = (tag > 0 ? strlen (ptr) : 0);
		while (len && isspace (ptr[len-1])) ptr[--len] = '\0';
		if (!len) {
			puts ("location_rdIdx(): zero dir name.");
			return NULL;
		} else if (len > 1 && (dir = dir_store (ptr, len)) == NULL) {
			puts ("location_rdIdx(): memory exhausted at dir name.");
			return NULL;
		} else {
			if (!dir) dir = &dir_base;
			dir->IdxTag = tag;
			if (!fgets (buf, (int)sizeof(buf), file)) {
				puts ("location_rdIdx(): unexpected EOF after dir name.");
				return NULL;
			}
		}
	}
	if (buf[0] != 'L' && buf[1] == ':') {
		if (buf[0] != '!') {
			puts ("location_rdIdx(): main entry missing.");
		}
	} else {
		char * ptr   = buf +2;
		long   h_tag = rd_hex (&ptr, 8);
		long   d_tag = rd_hex (&ptr, 8);
		short  proto = rd_hex (&ptr, 2);
		short  port  = rd_hex (&ptr, 4);
		size_t len   = (d_tag > 0 ? strlen (ptr) : 0);
		while (len && isspace (ptr[len-1])) ptr[--len] = '\0';
		if (h_tag <= 0) {
			puts ("location_rdIdx(): host tag missing.");
			return NULL;
		} else if (!host && (host = host_search (h_tag)) == NULL) {
			puts ("location_rdIdx(): host entry not found.");
			return NULL;
		} else if (d_tag <= 0) {
			puts ("location_rdIdx(): dir tag missing.");
			return NULL;
		} else if (!dir && (dir = dir_search (d_tag)) == NULL) {
			puts ("location_rdIdx(): dir entry not found.");
			return NULL;
		} else if (proto < 0 || port < 0) {
			printf ("location_rdIdx(): format error %i/%i.", proto, port);
			return NULL;
		} else if ((loc = _alloc (dir, ptr)) != NULL) {
			loc->Proto = proto;
			loc->Port  = port;
			loc->Host  = host;
			loc->Flags = host->Flags;
		}
	}
	return loc;
}
