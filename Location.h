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
	PROT_HTTP,
	PROT_HTTPS,
	PROT_FTP
} LC_PROTO;
#define PROTO_isLocal(p)  ((p) >= PROT_FILE  && (p) <= PROT_DIR)
#define PTOTO_isPseudo(p) ((p) >= PROT_ABOUT && (p) <= PROT_MAILTO)
#define PROTO_isRemote(p) ((p) >= PROT_HTTP)

struct s_location {
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

const char * location_Path  (LOCATION, UWORD * opt_len);
const char * location_Host  (LOCATION);
BOOL         location_equal (LOCATION, LOCATION);

BOOL location_resolve (LOCATION);
int  location_open    (LOCATION, const char ** host_name);



#endif /*__LOCATION_H__*/
