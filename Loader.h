/* @(#)highwire/Loader.h
 */
#ifndef __LOADER_H__
#define __LOADER_H__


typedef enum {   /* Basic MIME types from RFC2045/2046 by the
                  * "Network Working Group",
                  * partly registered with IANA
                 */
	MIME_Unknown = 0,

	MIME_TEXT = 0x1000,
		MIME_TXT_PLAIN,   /* text/plain "*"                         */
		MIME_TXT_HTML,    /* text/html  "*.htm,*.html,*.php,*.php?" */

	MIME_IMAGE = 0x2000,
		MIME_IMG_JPEG,    /* image/jpeg   "*.jpg,*.jpeg" */
		MIME_IMG_GIF,     /* image/gif    "*.gif"        */
		MIME_IMG_PNG,     /* image/png    "*.png"        */
		MIME_IMG_X_XIMG,  /* image/x-ximg "*.img"        */

	MIME_AUDIO = 0x3000,
		MIME_AUD_BASIC,   /* audio/basic */

	MIME_VIDEO = 0x4000,
		MIME_VID_MPEG,    /* video/mpeg   "*.mpg" */

	MIME_APPL = 0x5000,
		MIME_APP_OCTET,   /* application/octet-stream         */
		MIME_APP_PDF      /* application/pdf          "*.pdf" */
} MIMETYPE;

#define MIME_Major(t)   (t & 0xF000)
#define MIME_Minor(t)   (t & 0x0FFF)


typedef struct s_loader {
	LOCATION Location;   /* where the data come from ... */
	CONTAINR Target;     /* ... and where they should go to */
	ENCODING Encoding;
	MIMETYPE MimeType;
	short    MarginW, MarginH;
	/* */
	size_t   DataSize; /* expected data size                   */
	size_t   DataFill; /* real number of loaded/received bytes */
	char   * Data;
	BOOL     notified; /* if a start notification was sent     */
	/* */
	BOOL   rdChunked;  /* whether the data can't be received in one block */
	short  rdSocket;   /* handle for remote connection */
	size_t rdLeft;     /* number of bytes to complete the current data chunk */
	char * rdDest;     /* pointer to write received data */
	size_t rdTlen;     /* remaining bytes in rdTemp */
	char   rdTemp[32]; /* buffer for chunk header data */
	void * rdList, * rdCurr; /* chunk pointers */
} * LOADER;


LOADER new_loader    (LOCATION);
void   delete_loader (LOADER *);

void new_loader_job (const char *address, LOCATION base, CONTAINR target,
                     ENCODING, short margin_w, short margin_h);


#endif /* __LOADER_H__ */
