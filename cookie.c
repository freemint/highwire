#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "defs.h"
#include "mime.h"
#include "http.h"
#include "Location.h"
#include "cookie.h"

typedef struct s_cookie_jar * COOKIEJAR;
struct s_cookie_jar {
	COOKIEJAR    Next;
	COOKIE       Cookie[20];
	size_t       HostLen;
	const char * HostStr;
	size_t       DomainLen;
	char         DomainStr[1];
};
static COOKIEJAR jar_list = NULL;


/*#define DEBUG*/


/*----------------------------------------------------------------------------*/
static COOKIEJAR
create_jar (const char * hst_ptr, size_t hst_len,
            const char * dmn_ptr, size_t dmn_len)
{
	COOKIEJAR jar = malloc (sizeof (struct s_cookie_jar) + dmn_len);
	if (jar) {
		int i;
		for (i = 0; i < dmn_len; i++) {
			jar->DomainStr[i] = tolower (dmn_ptr[i]);
		}
		jar->DomainStr[dmn_len] = '\0';
		jar->DomainLen          = dmn_len;
		jar->HostStr            = hst_ptr;
		jar->HostLen            = hst_len;
		memset (jar->Cookie, 0, sizeof (jar->Cookie));
		jar->Next = NULL;
	}
	return jar;
}

/*----------------------------------------------------------------------------*/
static COOKIE
create_cookie (size_t nam_len, size_t val_len, size_t pth_len)
{
	COOKIE cookie = malloc (sizeof (struct s_cookie) + nam_len);
	if (cookie) {
		if (!pth_len) {
			cookie->PathStr = NULL;
		
		} else if ((cookie->PathStr = malloc (pth_len +1)) == NULL) {
			free (cookie);
			cookie = NULL; /* meory exhausted */
		}
	}
	if (cookie) {
		if ((cookie->ValueStr = malloc (val_len +1)) != NULL) {
			cookie->NameLen  = nam_len;
			cookie->ValueMax = val_len;
			cookie->ValueLen = val_len;
			cookie->PathLen  = pth_len;
			cookie->Expires  = 0;
		} else {
			if (cookie->PathStr) {
				free (cookie->PathStr);
			}
			free (cookie);
			cookie = NULL; /* meory exhausted */
		}
	}
	return cookie;
}

/*----------------------------------------------------------------------------*/
static void
destroy_cookie (COOKIE * list, short list_len)
{
	COOKIE cookie = *list;
	if (cookie) {
		if (cookie->ValueStr) free (cookie->ValueStr);
		if (cookie->PathStr)  free (cookie->PathStr);
		free (cookie);
	}
	while (--list_len > 0) {
		COOKIE  * temp = list++;
		*temp = *list;
	}
	*list = NULL;
}


/*----------------------------------------------------------------------------*/
static long
parse (const char * str, long len,
       const char ** key_p, long * k_len, const char ** val_p, long * v_len)
{
	const char * beg = str, * end, * ptr;
	while (len && isspace(*beg)) {
		len--;
		beg++;
	}
	if ((end = memchr (beg, ';', len)) == NULL) {
		end = beg + len;
	} else {
		len = end - beg;
	}
	if ((ptr = memchr (beg, '=', len)) == NULL) {
		ptr = end;
	} else {
		len = ptr - beg +1;
	}
	while (ptr-- > beg && isspace(*ptr));
	*key_p = beg;
	*k_len = ptr - beg +1;
	
	beg += len;
	while (beg < end && isspace(*beg)) beg++;
	ptr =  end;
	while (ptr-- > beg && isspace(*ptr));
	*val_p = beg;
	*v_len = ptr - beg +1;
	
	return (end - str);
}

/*============================================================================*/
void
cookie_set (LOCATION loc, const char * str, long len, long srvr_date)
{
#ifdef DEBUG
	const char * _dbg_str = str;
	size_t       _dbg_len = len;
	BOOL         _dbg_flg = FALSE;
	BOOL         _dbg_new = FALSE;
	const char * _drp_dmn = NULL;
#endif /*DEBUG*/
	COOKIEJAR    jar     = NULL;
	COOKIE       cookie  = NULL;
	const char * host_ptr;       UWORD  host_len;
	const char * nam_ptr;        size_t nam_len = 0;
	const char * val_ptr;        size_t val_len = 0;
	const char * exp_ptr = NULL; size_t exp_len = 0;
	const char * dmn_ptr = NULL; size_t dmn_len = 0;
	const char * pth_ptr = NULL; size_t pth_len = 0;
	long         expires = 0;
	BOOL         ck_remv = FALSE;
	
#ifndef DEBUG
	if (loc->Flags & (1uL << ('C' - 'A'))) {
		return;
	}
#endif
	
	while (isspace (*str) && len-- > 0) str++;
	while (len > 0 && isspace (str[len-1])) len--;
	nam_ptr = str;
	val_ptr = (len > 0 ? memchr (str, '=', len) : NULL);
#ifdef DEBUG
	_dbg_str = str;
	_dbg_len = len;
#endif /*DEBUG*/
	
	if (val_ptr && val_ptr > str) {
		const char * ptr = val_ptr;
		len -= ptr - str;
		while (isspace(*(--ptr)));
		nam_len = ptr - nam_ptr +1;
		while (--len && isspace(*(++val_ptr)));
		if ((ptr = memchr (val_ptr, ';', len)) != NULL) {
			str = ptr;
			len -= ptr - val_ptr;
		} else {
			ptr = val_ptr + len;
			str = "";
			len = 0;
		}
		while (ptr-- > val_ptr && isspace(*ptr));
		val_len = ptr - val_ptr +1;
	}
	if (nam_len > 0 && val_len > 0) {
		host_ptr = location_Host (loc, &host_len);
	} else {
		host_ptr = NULL;
		host_len = 0u;
	}
#ifdef DEBUG
	if (loc->Flags & (1uL << ('C' - 'A'))) {
		host_ptr = NULL;
	}
#endif
	
	while (*str == ';') {
		const char * key_p, * val_p;
		long         k_len,   v_len;
		long n = parse (++str, --len, &key_p, &k_len, &val_p, &v_len);
		str   += n;
		len   -= n;
		if        (k_len == 7 && strnicmp (key_p, "EXPIRES", 7) == 0) {
			exp_ptr = val_p;
			exp_len = v_len;
			if ((expires = http_date (exp_ptr)) != 0l && (expires < srvr_date)) {
				ck_remv = TRUE;
			}
		} else if (k_len == 6 && strnicmp (key_p, "DOMAIN",  6) == 0) {
			dmn_ptr = val_p;
			dmn_len = v_len;
		
		} else if (k_len == 4 && strnicmp (key_p, "PATH",    4) == 0) {
			pth_ptr = val_p;
			pth_len = v_len;
		
		} else if (k_len) {
			/* NS:       + secure                      */
			/* RFC 2109: + Comment + Max-Age + Version */
			/* RFC 2965: + CommentURL + Discard + Port */
		#ifdef DEBUG
			if (!_dbg_flg) {
				printf ("'%.*s'\n", (int)(_dbg_len > 0 ? _dbg_len : 0), _dbg_str);
				_dbg_flg = TRUE;
			}
			printf ("'%.*s' = '%.*s'\n", (int)k_len, key_p, (int)v_len, val_p);
		#endif
		}
		
		while (len && isspace(*str)) {
			len--;
			str++;
		}
		if (!len) break;
	}
	
	if (host_ptr && dmn_len) {   /* check DOMAIN= validity */
		
		if (dmn_len < 5 || dmn_ptr[0] != '.'
		                || !memchr (dmn_ptr +2, '.', dmn_len -3)) {
		#ifdef DEBUG
			_drp_dmn = "invalid";
		#endif
			dmn_len = 0; /* invalid nonsense */
		
		} else {
			if (host_len == dmn_len -1
			    ? strnicmp (host_ptr, dmn_ptr +1, dmn_len -1) != 0
			    : host_len <= dmn_len ||
			      strnicmp (host_ptr + (host_len - dmn_len),
			                dmn_ptr, dmn_len) != 0) {
		#ifdef DEBUG
			_drp_dmn = "spoofed";
		#endif
				host_ptr = NULL; /* spoofed */
			}
		}
	}
	
	if (host_ptr) {   /* check PATH= validity */
		
		if (pth_len && pth_ptr[0] != '/') {
			host_ptr = NULL; /* probably spoofed, drop completely */
		
		} else {
			if (!pth_len) {
				UWORD p_len;
				pth_ptr = location_Path (loc, &p_len);
				pth_len = (p_len > 1 && pth_ptr[p_len-1] == '/' ? p_len -1 : p_len);
			} else if (pth_len > 1) {
				UWORD        p_len;
				const char * p_ptr = location_Path (loc, &p_len);
				UWORD        p_max = min (pth_len, p_len);
				pth_len = 0;
				while (pth_ptr[pth_len] == p_ptr[pth_len] && ++pth_len < p_max);
			}
			if (pth_len == 1) { /* pth_ptr == "/" */
				pth_ptr = NULL;  /* no need to save this */
				pth_len = 0;
			}
		}
	}
	
	if (host_ptr) {
		jar = jar_list;
		if (dmn_len) {   /* search domain cookiejar */
			while (jar) {
				if (jar->DomainLen == dmn_len
				    && strncmp (jar->DomainStr, dmn_ptr, dmn_len) == 0) {
					break;
				}
				jar = jar->Next;
			}
		} else {         /* search server cookiejar */
			while (jar) {
				if (!jar->DomainLen && jar->HostStr == host_ptr) {
					break;
				}
				jar = jar->Next;
			}
		}
		if (!jar && !ck_remv &&
		    (jar = create_jar (host_ptr, host_len, dmn_ptr, dmn_len)) != NULL) {
			jar->Next = jar_list;
			jar_list  = jar;
	#ifdef DEBUG
			_dbg_new  = TRUE;
	#endif
		}
	}
	if (jar) {
		COOKIE * ck_ptr = jar->Cookie;
		short    n      = numberof (jar->Cookie);
		while ((cookie = *ck_ptr) != NULL) {
			if (cookie->NameLen == nam_len
			    && strnicmp (cookie->NameStr, nam_ptr, nam_len) == 0
			    && ((!cookie->PathStr && !pth_ptr) ||
			        (strncmp (cookie->PathStr, pth_ptr, pth_len) == 0
			         && cookie->PathStr[pth_len] == '\0'))) {
				if (ck_remv) {
					destroy_cookie (ck_ptr, n);
					cookie = NULL;
				}
				break;
			}
			if (!--n) {        /* not found */
				if (!ck_remv) { /* make space at table end */
					destroy_cookie (ck_ptr, 0);
					cookie = NULL;
				}
				break;
			}
			ck_ptr++;
		}
		if (!ck_remv) {
			if (!cookie) {
				cookie = create_cookie (nam_len, val_len, pth_len);
				if (cookie) {
					memcpy (cookie->NameStr, nam_ptr, nam_len);
					cookie->NameStr[nam_len] = '\0';
					if (pth_len) {
						memcpy (cookie->PathStr, pth_ptr, pth_len);
						cookie->PathStr[pth_len] = '\0';
					}
				}
			} else {
				if (expires) {
					cookie->Expires = 0;
				}
				if (cookie->ValueMax < val_len) {
					char * value = malloc (val_len +1);
					if (value) {
						free (cookie->ValueStr);
						cookie->ValueStr = value;
						cookie->ValueMax = val_len;
						cookie->ValueLen = val_len;
					} else {
						cookie = NULL; /* meory exhausted */
					}
				} else {
					cookie->ValueLen = val_len;
				}
			}
			if (cookie) {
				memcpy (cookie->ValueStr, val_ptr, val_len);
				cookie->ValueStr[val_len] = '\0';
				if (!cookie->Expires) {
					cookie->Expires = expires;
				}
				if (jar->Cookie[0] != cookie) { /* move cookie to table begin */
					COOKIE swap = cookie;
					ck_ptr      = jar->Cookie;
					do {
						COOKIE save = *ck_ptr;
						*(ck_ptr++) = swap;
						swap        = save;
					} while (swap && swap != cookie);
				}
			}
		}
	}
	
#ifdef DEBUG
	{
		FILE * ftmp = fopen ("U:\\tmp\\cookies.txt", "a");
		if (ftmp) {
			char buff[1024] = "________________________________________"
			                  "________________________________________\n";
			time_t lt = time (NULL);
			char * p = strchr (buff, '\0');
			strftime (p, sizeof(buff), "___%Y-%m-%d_%H:%M:%S", localtime (&lt));
			p = strchr (p, '\0');
			lt = srvr_date;
			strftime (p, sizeof(buff), "___%Y-%m-%d_%H:%M:%S", gmtime (&lt));
			p = strchr (p, '\0');
			strcpy (p, "__________\n");
			fputs (buff, ftmp);
			p = buff + location_FullName (loc, buff, sizeof(buff) -2);
			*(p++) = '\n';
			*(p)   = '\0';
			fputs (buff, ftmp);
			fprintf (ftmp, "|%.*s|\n", (int)_dbg_len, _dbg_str);
			if (val_ptr) {
				fprintf (ftmp, "   '%.*s' = '%.*s'\n",
				         (int)nam_len, nam_ptr, (int)val_len, val_ptr);
			}
			if (pth_ptr) {
				fprintf (ftmp, "   PATH    = '%.*s'\n", (int)pth_len, pth_ptr);
			} else {
				fprintf (ftmp, "   path    = '/'\n");
			}
			if (dmn_len) {
				fprintf (ftmp, "   DOMAIN  = '%.*s' %s\n",
				         (int)dmn_len, dmn_ptr, (_drp_dmn ? "denied" : ""));
			} else if (_drp_dmn) {
				fprintf (ftmp, "   DOMAIN  = dropped: %s\n", _drp_dmn);
			}
			if (exp_ptr) {
				strftime (buff, sizeof(buff),
				          "%Y-%m-%d %H:%M:%S", gmtime ((time_t*)&expires));
				fprintf (ftmp, "   EXPIRES = %s '%.*s'\n",
				         buff, (int)exp_len, exp_ptr);
			}
			if (jar) {
				fprintf (ftmp, "   %s> '%s'\n",
				         (_dbg_new ? "==" : "--"),
				         (jar->DomainStr[0] ? jar->DomainStr : jar->HostStr));
			} else if (loc->Flags & (1uL << ('C' - 'A'))) {
				fprintf (ftmp, "   >>> discarded <<<\n");
			}
			fclose (ftmp);
		}
	}
#else
	(void)exp_len; /* keep Pure C quiet */
#endif /*DEBUG*/
}


/*============================================================================*/
WORD
cookie_Jar (LOCATION loc, COOKIESET * cset)
{
	COOKIEJAR    jar  = jar_list;
	WORD         num  = 0;
	UWORD        h_ln;
	const char * host = location_Host (loc, &h_ln);
	const char * path = location_Path (loc, NULL);
	
	if (loc->Flags & (1uL << ('C' - 'A'))) {
#ifdef DEBUG
		FILE * ftmp = fopen ("U:\\tmp\\cookies.txt", "a");
		if (ftmp) {
			fputs ("========================================"
			       "========================================\n", ftmp);
			fprintf (ftmp, "%s:\n(discarded)\n", host);
			fputs ("========================================"
			       "========================================\n", ftmp);
			fclose (ftmp);
		}
#endif /*DEBUG*/
		return 0;
	}
	
	while (jar) {
		BOOL match;
		if (!jar->DomainLen) {
			match = (host == jar->HostStr);
		} else if (h_ln == jar->DomainLen -1) {
			match = (strcmp (host, jar->DomainStr +1) == 0);
		} else if (h_ln > jar->DomainLen) {
			match = (strcmp (host + (h_ln - jar->DomainLen), jar->DomainStr) == 0);
		} else {
			match = FALSE;
		}
		if (match) {
			short i;
			for (i = 0; i < numberof (jar->Cookie) -1 && jar->Cookie[i]; i++) {
				COOKIE cookie = jar->Cookie[i];
				if (!cookie->PathStr
				    || strncmp (path, cookie->PathStr, cookie->PathLen) == 0) {
					
					if (!num) {
						cset->Cookie[0] = cookie;
						num++;
					
					} else if (num == numberof (cset->Cookie) &&
					           cset->Cookie[num-1]->PathLen >= cookie->PathLen) {
						/* array is already full, skip this cookie */
					
					} else {
						short n = (num < numberof (cset->Cookie) ? num : --num);
						while (cset->Cookie[n-1]->PathLen < cookie->PathLen) {
							cset->Cookie[n] = cset->Cookie[n-1];
							if (!--n) break;
						}
						cset->Cookie[n] = cookie;
						num++;
					}
				}
			}
		}
		jar = jar->Next;
	}
#ifdef DEBUG
	if (num) {
		FILE * ftmp = fopen ("U:\\tmp\\cookies.txt", "a");
		if (ftmp) {
			short i = 0;
			fputs ("========================================"
			       "========================================\n", ftmp);
			fprintf (ftmp, "%s:\n", host);
			do {
				fprintf (ftmp, "%s=%s\n",
				         cset->Cookie[i]->NameStr, cset->Cookie[i]->ValueStr);
			} while (++i < num);
			fputs ("========================================"
			       "========================================\n", ftmp);
			fclose (ftmp);
		}
	}
#endif /*DEBUG*/
	return num;
}
