/* @(#)highwire/Location.h
 */
#ifndef __LOCATION_H__
#define __LOCATION_H__


typedef enum {
	PROT_Invalid = -1,
	PROT_FILE    = 0,
	PROT_DIR,
	PROT_ABOUT,
	PROT_MAILTO,
	PROT_POP,
	PROT_HTTP,
	PROT_HTTPS,
	PROT_FTP
} LC_PROTO;
#define PROTO_isLocal(p)  ((p) >= PROT_FILE  && (p) <= PROT_DIR)
#define PROTO_isPseudo(p) ((p) >= PROT_ABOUT && (p) <= PROT_MAILTO)
#define PROTO_isRemote(p) ((p) >= PROT_HTTP)

struct s_location {
	ULONG    __hash;
	unsigned __reffs;
	LC_PROTO Proto;
	short    Port;
	BOOL     resolved;
	void   * Host, * Dir;
	const char * File;
	const char * Path;
	const char * Anchor;
	const char   FullName[4];
};


LOCATION new_location  (const char * src, LOCATION base);
void     free_location (LOCATION *);

LOCATION location_share (LOCATION);

#define      location_Hash(l)  (!l ? 0uL : l->__hash ? l->__hash : _loc_Hash(l))
size_t       location_FullName (LOCATION, char * buffer, size_t max_len);
const char * location_Path     (LOCATION, UWORD * opt_len);
const char * location_Host     (LOCATION, UWORD * opt_len);
BOOL         location_equal    (LOCATION, LOCATION);

BOOL location_resolve (LOCATION);
int  location_open    (LOCATION, const char ** host_name);

#if defined(_STDIO_H) || defined(__STDIO)
BOOL     location_wrIdx (FILE *, LOCATION);
LOCATION location_rdIdx (FILE *);
#endif /* <stdio.h> */


/*----- private -----*/

ULONG _loc_Hash (LOCATION);


#endif /*__LOCATION_H__*/
