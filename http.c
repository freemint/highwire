/* @(#)highwire/http.c
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "defs.h"
#include "Location.h"
#include "mime.h"
#include "http.h"
#include "inet.h"
#include "token.h"  /* needed for and before scanner.h */
#include "scanner.h"


long
http_date (const char * beg)
{
	/* RFC 2616 grammar:
	HTTP-date    = rfc1123-date | rfc850-date | asctime-date
	rfc1123-date = wkday "," SP date1 SP time SP "GMT"
	rfc850-date  = weekday "," SP date2 SP time SP "GMT"
	asctime-date = wkday SP date3 SP time SP 4DIGIT
	date1        = 2DIGIT SP month SP 4DIGIT
	               ; day month year (e.g., 02 Jun 1982)
	date2        = 2DIGIT "-" month "-" 2DIGIT
	               ; day-month-year (e.g., 02-Jun-82)
	date3        = month SP ( 2DIGIT | ( SP 1DIGIT ))
	               ; month day (e.g., Jun  2)
	time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
	               ; 00:00:00 - 23:59:59
	wkday        = "Mon" | "Tue" | "Wed"
	             | "Thu" | "Fri" | "Sat" | "Sun"
	weekday      = "Monday" | "Tuesday" | "Wednesday"
	             | "Thursday" | "Friday" | "Saturday" | "Sunday"
	month        = "Jan" | "Feb" | "Mar" | "Apr"
	             | "May" | "Jun" | "Jul" | "Aug"
	             | "Sep" | "Oct" | "Nov" | "Dec"
	*/
	const char mon[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	                       "Aug", "Sep", "Oct", "Nov", "Dec" };
	struct tm  tm = { 0, 0, 0, -1, -1, -1, 0,0,0 };
	char     * end;
	BOOL       asc;
	short      i;
	long       date;
	
	/* skip weekdays */
	while (isalpha (*beg)) beg++;
	while (isspace (*beg)) beg++;
	
	if (*beg != ',')  { /* asctime-date */
		asc = TRUE;
		
	} else { /* rfc1123-date or rfc850-date */
		asc = FALSE;
		tm.tm_mday = (int)strtol (++beg, &end, 10);
		beg = end;
		if (*beg == '-') beg++;
	}
	
	/* read month */
	while (isspace (*beg)) beg++;
	for (i = 0; i < (short)numberof(mon); i++) {
		if (strnicmp (beg, mon[i], 3) == 0) {
			tm.tm_mon = i;
			beg += 3;
			break;
		}
	}
	
	if (asc) {
		tm.tm_mday = (int)strtol (++beg, &end, 10);
		beg = end;
		if (*beg == ',') beg++;
	
	} else {
		if (*beg == '-') beg++;
	}
	
	/* read year */
	if ((tm.tm_year = (int)strtol (beg, &end, 10)) >= 0) {
		if      (tm.tm_year <    70) tm.tm_year += 100;
		else if (tm.tm_year >= 1900) tm.tm_year -= 1900;
	}
	beg = end;
	
	/* read time as hh:mm:ss */
	sscanf (beg, "%2u:%2u:%2u", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
	
	/* convert to Coordinated Universal Time (UTC) */
	date =  mktime (&tm);     /* here tm is wrongly asumed to be local time... */
	date += (tm.tm_isdst > 0 ? 60l*60 : 0) - timezone; /* ... but now it       *
	                                                    * becomes UTC, again   */
	return (date > 0 ? date : 0);
}


/*============================================================================*/
ENCODING
http_charset (const char * beg, size_t len, MIMETYPE * p_type)
{
	MIMETYPE type = 0;
	ENCODING cset = ENCODING_Unknown;
	
	if (!p_type) {
		const char * p = memchr (beg, ';', len);
		if (p) {
			len -= p - beg;
			beg  = p;
			type = MIME_TEXT;
		} else {
			len = 0;
			beg = "";
		}
	
	} else {
		const char * end = beg;
		*p_type = type = mime_byString (beg, &end);
		if ((long)(len -= end - beg) < 0) {
			len = 0;
		} else {
			beg = end;
			while (len > 0 && isspace(*beg)) {
				len--;
				beg++;
			}
		}
		if (len <= 0) beg = "";
	}
	
	if (MIME_Major(type) == MIME_TEXT && *beg == ';') {
		while (--len > 0 && isspace (*(++beg)));
		if (len > 8 && strnicmp (beg, "charset=", 8) == 0) {
			beg += 8;
			len -= 8;
			cset = scan_encoding (beg, cset);
		}
	}
	
	return cset;
}


/*----------------------------------------------------------------------------*/
static BOOL
content_type (const char * beg, long len, HTTP_HDR * hdr)
{
	const char token[] = "Content-Type:";
	BOOL       found;
	
	if (len >= sizeof(token) && strnicmp (beg, token, sizeof(token)-1) == 0) {
		MIMETYPE type;
		ENCODING cset;
		beg += sizeof(token)-1;
		len -= sizeof(token)-1;
		while (isspace (*beg) && len-- > 0) beg++;
		if (len > 0) {
			cset = http_charset (beg, len, &type);
			if (type) {
				hdr->MimeType = type;
				if (cset) {
					hdr->Encoding = cset;
				}
			}
		}
		found = TRUE;
	} else {
		found = FALSE;
	}
	return found;
}

/*----------------------------------------------------------------------------*/
static BOOL
content_length (const char * beg, long len, HTTP_HDR * hdr)
{
	const char token[] = "Content-Length:";
	BOOL       found;
	
	if (len >= sizeof(token) && strnicmp (beg, token, sizeof(token)-1) == 0) {
		char  * p;
		long size = strtol (beg += sizeof(token)-1, &p, 10);
		if (size > 0 || (p > beg && *(--p) == '0')) {
			hdr->Size = size;
		}
		found = TRUE;
	} else {
		found = FALSE;
	}
	return found;
}

/*----------------------------------------------------------------------------*/
static BOOL
redirect (const char * beg, long len, HTTP_HDR * hdr)
{
	const char token[] = "Location:";
	BOOL       found;
	
	if (len >= sizeof(token) && strnicmp (beg, token, sizeof(token)-1) == 0) {
		beg += sizeof(token)-1;
		len -= sizeof(token)-1;
		while (isspace (*beg) && len-- > 0) beg++;
		if (len > 0) {
			beg += len;
			while (isspace (*(--beg)) && --len);
			if (len) {
				char * p = strchr (hdr->Tail, '\0');
				do {
					*(--p) = *(beg--);
				} while (--len);
				hdr->Rdir = p;
			}
		}
		found = TRUE;
	} else {
		found = FALSE;
	}
	return found;
}

/*----------------------------------------------------------------------------*/
static BOOL
transfer_enc (const char * beg, long len, HTTP_HDR * hdr)
{
	const char token[] = "Transfer-Encoding:";
	BOOL       found;
	
	if (len >= sizeof(token) && strnicmp (beg, token, sizeof(token)-1) == 0) {
		beg += sizeof(token)-1;
		len -= sizeof(token)-1;
		while (isspace (*beg) && len-- > 0) beg++;
		if (strnicmp (beg, "chunked", 7) == 0) {
			hdr->Chunked = TRUE;
		}
		found = TRUE;
	} else {
		found = FALSE;
	}
	return found;
}

/*----------------------------------------------------------------------------*/
static BOOL
server_date (const char * beg, long len, HTTP_HDR * hdr)
{
	const char token[] = "Date:";
	BOOL       found;
	
	if (len >= sizeof(token) && strnicmp (beg, token, sizeof(token)-1) == 0) {
		beg += sizeof(token)-1;
		len -= sizeof(token)-1;
		while (isspace (*beg) && len-- > 0) beg++;
		if (len > 0) {
			hdr->SrvrDate = http_date (beg);
		}
		found = TRUE;
	} else {
		found = FALSE;
	}
	return found;
}

/*----------------------------------------------------------------------------*/
static BOOL
last_modified (const char * beg, long len, HTTP_HDR * hdr)
{
	const char token[] = "Last-Modified:";
	BOOL       found;
	
	if (len >= sizeof(token) && strnicmp (beg, token, sizeof(token)-1) == 0) {
		beg += sizeof(token)-1;
		len -= sizeof(token)-1;
		while (isspace (*beg) && len-- > 0) beg++;
		if (len > 0) {
			hdr->Modified = http_date (beg);
		}
		found = TRUE;
	} else {
		found = FALSE;
	}
	return found;
}

/*----------------------------------------------------------------------------*/
static BOOL
expires (const char * beg, long len, HTTP_HDR * hdr)
{
	const char token[] = "Expires:";
	BOOL       found;
	
	if (len >= sizeof(token) && strnicmp (beg, token, sizeof(token)-1) == 0) {
		beg += sizeof(token)-1;
		len -= sizeof(token)-1;
		while (isspace (*beg) && len-- > 0) beg++;
		if (len > 0) {
			if (isdigit (*beg)) { /* invalid format but often used */
				long delta = strtol (beg, NULL, 10);
				if (delta >= 0) {
					hdr->Expires = hdr->SrvrDate + delta;
				}
			} else {
				hdr->Expires = http_date (beg);
			}
		}
		found = TRUE;
	} else {
		found = FALSE;
	}
	return found;
}

/*============================================================================*/
#ifdef USE_INET
short
http_header (LOCATION loc, HTTP_HDR * hdr, size_t blk_size,
             short * keep_alive, long tout_msec,
             LOCATION referer, const char * post_buf)
{
	static char buffer[2048];
	size_t left  = sizeof(buffer) -4;
	int    reply = 0;
	
	clock_t timeout = 0;
	
	char * ln_beg = buffer, * ln_end = ln_beg, * ln_brk = NULL;
	
	const char * name = NULL;
	int sock;
	
	hdr->Version  = 0x0000;
	hdr->SrvrDate = -1;
	hdr->Modified = -1;
	hdr->Expires  = -1;
	hdr->Size     = -1;
	hdr->MimeType = -1;
	hdr->Encoding = ENCODING_Unknown;
	hdr->Chunked  = FALSE;
	hdr->Rdir     = NULL;
	hdr->Head     = buffer;
	buffer[sizeof(buffer) -1] = '\0';
	
	if (keep_alive && *keep_alive >= 0) {
		while (hdr->Tlen > 0 && isspace (*hdr->Tail)) {
			hdr->Tlen--;
			hdr->Tail++;
		}
		if (hdr->Tlen > 0) {
			memcpy (buffer, hdr->Tail, hdr->Tlen);
			ln_beg = ln_end += hdr->Tlen;
			left            -= hdr->Tlen;
		}
		sock = *keep_alive;
	
	} else if ((sock = location_open (loc, &name)) < 0) {
		if (sock == -ETIMEDOUT) {
			strcpy (buffer, "Connection timeout!\n");
		} else if (sock < -1) {
			sprintf (buffer, "Error: %s\n", strerror(-sock));
		} else {
			strcpy (buffer, "No route to  host!\n");
		}
		return sock;
	
	} else {
		const char rest[] = " HTTP/1.1\r\n";
		const char * meth = (!keep_alive ? "HEAD " : post_buf ? "POST " : "GET ");
		char * p   = strchr (strcpy (buffer, meth), '\0');
		size_t len = min (strlen (loc->FullName),
		                  sizeof(buffer) - sizeof(rest) - (p - buffer));
		strncpy (p, loc->FullName, len);
		strcpy  (p += len, rest);
		if ((len = inet_send (sock, buffer, (p - buffer) + sizeof(rest)-1)) > 0) {
			const char * stack = inet_info();
			len = sprintf (buffer,
			      "HOST: %s\r\n"
			      "User-Agent: Mozilla 2.0 (compatible; Atari %s/%i.%i.%i %s)\r\n",
			      name,
			      _HIGHWIRE_FULLNAME_, _HIGHWIRE_MAJOR_, _HIGHWIRE_MINOR_,
			      _HIGHWIRE_REVISION_, (stack ? stack : ""));
			len = inet_send (sock, buffer, len);
		}
		if ((long)len > 0 && referer) {
			const char text[] = "Referer: ";
			len = sizeof (text) -1;
			memcpy (buffer, text, len);
			len += location_FullName (referer,
			                          buffer + len, sizeof(buffer) - len -2);
			strcpy (buffer + len, "\r\n");
			len = inet_send (sock, buffer, len +2);
		}
		if ((long)len > 0 && post_buf) {
			long n = strlen (post_buf);
			len = sprintf (buffer,
			               "Content-type: application/x-www-form-urlencoded\r\n"
			               "Content-length: %li\r\n\r\n", n);
			if ((long)(len = inet_send (sock, buffer, len)) > 0 && n) {
				len = inet_send (sock, post_buf, n);
			}
		}
		if ((long)len < 0 || (long)(len = inet_send (sock, "\r\n", 2)) < 0) {
			if ((long)len < -1) {
				sprintf (buffer, "Error: %s\n", strerror(-(int)len));
			} else {
				strcpy (buffer, "Connection error!\n");
			}
			inet_close (sock);
			
			return (short)len;
		}
	}
	hdr->Tail = buffer + sizeof(buffer) -1;
	hdr->Tlen = 0;
	
	tout_msec = (tout_msec * CLK_TCK) /1000; /* align to clock ticks */
	do {
		long n = inet_recv (sock, ln_end, (left <= blk_size ? left : blk_size));
		
		if (n < 0) { /* connection broken */
			if (reply) {
				ln_brk = ln_end;   /* seems to be a wonky server */
				*(ln_end++) = '\n';
				*(ln_end++) = '\n';
				*(ln_end)   = '\0';
			} else {
				reply = (short)n;
			}
			inet_close (sock);
			sock = -1;
			break;
		
		} else if (!n) { /* no data available yet */
			clock_t clk = clock();
			if (!timeout) {
				timeout = clk;
				continue;
			} else if ((clk -= timeout) < tout_msec) {
				continue;
			}
			/* else timed out */
			clk *= 1000 / CLK_TCK;
			sprintf (buffer, "Header %s %i.%03i sec.\n", 
			         (reply ? "stalled" : "timeout"),
			         (int)(clk /1000), (int)(clk % 1000));
			inet_close (sock);
			sock = -1;
			return -ETIMEDOUT;
		
		} else {
			timeout = 0;
			ln_brk  = memchr (ln_end, '\n', n);
			ln_end += n;
			left   -= n;
			if (!ln_brk) {
				continue;
			} else {
				*ln_end = '\0';
			}
		}
		
		if (!reply) {
			unsigned int major, minor;
			hdr->LoclDate = time (NULL);
			if (sscanf (ln_beg, "HTTP/%u.%u %i", &major, &minor, &reply) < 3) {
				reply = -1;
				inet_close (sock);
				sock = -1;
				break;
			
			} else {
				hdr->Version = (major <<8) | minor;
				ln_beg = ln_brk +1;
				n      = ln_end - ln_beg +1;
				ln_brk = (n ? memchr (ln_beg, '\n', n) : NULL);
			}
		}
		while (ln_brk) {
			*ln_brk = '\0';
			if (ln_beg == ln_brk || (ln_beg[0] == '\r' && !ln_beg[1])) {
				ln_beg = ln_brk +1;
				hdr->Tail = ln_beg;
				hdr->Tlen = (ln_end > ln_beg ? ln_end - ln_beg : 0);
				left = 0;
				break;
			}
			while (isspace (*ln_beg) && ++ln_beg < ln_brk);
			if ((n = ln_brk - ln_beg) > 0
				 && !content_length (ln_beg, n, hdr)
				 && !content_type   (ln_beg, n, hdr)
				 && !redirect       (ln_beg, n, hdr)
				 && !transfer_enc   (ln_beg, n, hdr)
				 && !server_date    (ln_beg, n, hdr)
				 && !last_modified  (ln_beg, n, hdr)
				 && !expires        (ln_beg, n, hdr)
				) {
				/* something else? */
			}
			*ln_brk = '\n';
			ln_beg = ln_brk +1;
			n      = ln_end - ln_beg +1;
			ln_brk = (n ? memchr (ln_beg, '\n', n) : NULL);
		}
	} while (left);
	
	if (reply <= 0) {
		strcpy (buffer, (reply == -ECONNRESET
		                 ? "Connection reset by peer." : "Protocoll Error!\n"));
	}
	if (hdr->SrvrDate <= 0) {
		hdr->SrvrDate = (hdr->Modified > 0 ? hdr->Modified : hdr->LoclDate);
	}
	
	if (keep_alive) { /* keep_alive */
		*keep_alive = sock;
	
	} else {
		inet_close (sock);
	}
	return reply;
}
#endif /* USE_INET */
