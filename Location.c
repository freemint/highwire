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

#include "defs.h"
#include "Location.h"
#include "Logging.h"
#include "inet.h"


static LOCATION __base  = NULL;
static LOCATION __local = NULL; /* map remote URLs to local dir */

char * local_web = NULL;


typedef struct s_host_entry {
	struct s_host_entry * Next;
	unsigned long         Ip;
	char                  Name[1];
} * HOST_ENT;
static void * host_entry (const char ** name);
#define       host_addr(h)   (h ? h->Ip : 0uL)


typedef struct s_dir_entry {
	struct s_dir_entry * Next;
	BOOL                 isTos;
	UWORD                Length;
	char                 Name[2];
} * DIR_ENT;
static void * dir_entry (const char ** name, DIR_ENT, BOOL local);


/*----------------------------------------------------------------------------*/
static LOCATION
_alloc (DIR_ENT dir, const char * file)
{
	size_t   size = (file && *file ? strlen (file) : 0);
	LOCATION loc  = malloc (sizeof (struct s_location) + dir->Length + size);
	char   * ptr  = (char*)loc + offsetof (struct s_location, FullName);
	loc->__reffs  = 0;
	loc->Proto    = PROT_FILE;
	loc->Port     = 0;
	loc->resolved = TRUE;
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
		if (src[1] == '/' && PROTO_isRemote(loc_proto)) {
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
		loc_host = host_entry (&s);
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
	loc->Host  = loc_host;
	loc->__reffs++;
	
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
		case PROT_ABOUT:  src = "about:"; break;
		case PROT_MAILTO: src = "mailto:";  break;
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
location_Host  (LOCATION loc)
{
	HOST_ENT host = loc->Host;
	
	return (host ? host->Name : "localhost");
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
			sock = (int)inet_connect (host->Ip, loc->Port);
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

/*----------------------------------------------------------------------------*/
static void *
host_entry (const char ** name)
{
	static HOST_ENT _base = NULL;
	
	HOST_ENT ent = _base;
	const char * n = *name;
	char buf[258], c;
	short len = 0;
	
	while ((c = *n) != '\0' && len < sizeof(buf) -1) {
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
		return NULL;
	}
	
	while (ent) {
		if (strcmp (buf, ent->Name) == 0) {
			break;
		} else {
			ent = ent->Next;
		}
	}
	if (!ent) {
		ent = malloc (sizeof(struct s_host_entry) + len);
		memcpy (ent->Name, buf, len +1);
		ent->Ip   = 0uL;
		ent->Next = _base;
		_base     = ent;
	}
#ifdef USE_INET
	if (!ent->Ip) {
		inet_host_addr (ent->Name, (long*)&ent->Ip);
	}
#endif /* USE_INET */
	
	return ent;
}


/*----------------------------------------------------------------------------*/
static void *
dir_entry (const char ** p_name, DIR_ENT base, BOOL local)
{
	static struct s_dir_entry _base = { NULL, FALSE, 1, "/" };
	
	DIR_ENT dir = &_base;
	
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
		DIR_ENT d     = &_base;
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
		DIR_ENT * p_dir = &_base.Next;
		
		while ((dir = *p_dir) != NULL && dir->Length < n_len) {
			p_dir = &dir->Next;
		}
		while (dir) {
			if (dir->Length > n_len) {
				dir = NULL;
			} else if (memcmp (buf, dir->Name, n_len) == 0) {
				break;
			} else {
				dir = *(p_dir = &dir->Next);
			}
		}
		if (!dir) {
			dir = malloc (sizeof(struct s_dir_entry) + n_len -1);
			dir->isTos  = (buf[1] == ':');
			dir->Length = n_len;
			memcpy (dir->Name, buf, n_len +1);
			dir->Next = *p_dir;
			*p_dir    = dir;
		}
	
	} else if (!n_len && base) {
		dir = base;
	}
	
	return dir;
}
