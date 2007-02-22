#ifndef __BOOKMARK_H__
#define __BOOKMARK_H__

extern const char * bkm_File;
extern const char * bkm_CSS;

typedef struct bkm_group B_GRP;
typedef struct bkm_url	 B_URL;

struct bkm_group {
	char *  ID;
	long	Time_Added;
	B_GRP *	Next;	
};

struct bkm_url {
	int     ID;
	char *  Class;
	char *  Target; 
	long    Time_Added;
	long    Last_Visit;
	char *  Address;
	B_GRP *	Parent;
	B_URL *	Next;
};

BOOL read_bookmarks (void);
BOOL add_bookmark (const char * bookmark_url, const char *bookmark_title);
BOOL save_bookmarks     (const char *);
BOOL add_bookmark_group (const char *);

B_GRP *bkm_group_ctor (B_GRP *, B_GRP *);
B_GRP *bkm_group_dtor (B_GRP * );
B_URL *bkm_url_ctor (B_URL *, B_URL *);
B_URL *bkm_url_dtor (B_URL * );



#endif /*__BOOKMARK_H__*/
