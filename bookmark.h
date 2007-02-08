#ifndef __BOOKMARK_H__
#define __BOOKMARK_H__

extern const char * bkm_File;
extern const char * bkm_CSS;

BOOL read_bookmarks (void);
BOOL add_bookmark (const char * bookmark_url, const char *bookmark_title);
BOOL save_bookmarks     (const char *);
BOOL add_bookmark_group (const char *);


#endif /*__BOOKMARK_H__*/
