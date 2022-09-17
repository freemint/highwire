#ifndef __HTTP_H__
#define __HTTP_H__

#include "Loader.h"

typedef struct {
	short    Version;   /* which HTTP */
	long     LoclDate;  /* local time when the header was received */
	long     SrvrDate;  /* remote time from 'Server-Date'          */
	long     Modified;  /* -1 or from 'Last-Modified'  */
	long     Expires;
	long     Size;      /* -1 or from 'Content-Length' */
	MIMETYPE MimeType;  /* -1 or from 'Content-Type'   */
	ENCODING Encoding;  /* 'ENCODING_Unknown' or from 'Content-Type; charset' */
	BOOL     Chunked;   /* FALSE or from 'Transfer-Encoding: chunked' */
	const char * Realm; /* NULL or from 'WWW-Authenticate' */
	const char * Head;  /* points to an internal buffer that will be */
	const char * Tail;  /*   overwritten on subsequent calls         */
	size_t       Tlen;  /* number of bytes in Tail */
	const char * Rdir;
} HTTP_HDR;

void hhtp_proxy (const char * host, short port);

short http_header (LOCATION, HTTP_HDR *, size_t blk_size,
                   short * keep_alive, long tout_msec,
                   LOCATION referer, const char * auth, POSTDATA post_buf);

long     http_date    (const char * buf);
ENCODING http_charset (const char * buf, size_t len, MIMETYPE *);


#endif /*__HTTP_H__*/
