#ifndef __HTTP_H__
#define __HTTP_H__


typedef struct {
	short    Version;  /* which HTTP */
	long     Date;     /* -1 or from 'Last-Modified'  */
	long     Size;     /* -1 or from 'Content-Length' */
	MIMETYPE MimeType; /* -1 or from 'Content-Type'   */
	ENCODING Encoding; /* 'ENCODING_Unknown' or from 'Content-Type; charset' */
	BOOL     Chunked;  /* FALSE or from 'Transfer-Encoding: chunked' */
	const char * Head; /* points to an internal buffer that will be */
	const char * Tail; /*   overwritten on subsequent calls         */
	const char * Rdir;
} HTTP_HDR;

short http_header (LOCATION, HTTP_HDR *, short * keep_alive);

ENCODING http_charset (const char * buf, size_t len, MIMETYPE *);


#endif /*__HTTP_H__*/
