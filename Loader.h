/* @(#)highwire/Loader.h
 */
#ifndef __LOADER_H__
#define __LOADER_H__


#include "mime.h"


typedef struct s_loader {
	LOCATION Location;   /* where the data come from ... */
	CONTAINR Target;     /* ... and where they should go to */
	ENCODING Encoding;
	MIMETYPE MimeType;
	char   * ExtAppl;
	short    MarginW, MarginH;
	short    ScrollV, ScrollH;
	/* */
	LOCATION Cached;
	long     Date;
	long     Expires;
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


LOADER new_loader    (LOCATION, CONTAINR target);
void   delete_loader (LOADER *);

LOADER new_loader_job   (const char *address, LOCATION base, CONTAINR target);
void   loader_setParams (LOADER, ENCODING, short margin_w, short margin_h);


#endif /* __LOADER_H__ */
