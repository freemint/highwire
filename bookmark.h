#ifndef __BOOKMARK_H__
#define __BOOKMARK_H__

extern const char * bkm_File;
extern const char * bkm_CSS;

BOOL read_bookmarks (void);
BOOL save_bookmarks (const char *);
BOOL add_bookmark       (const char * url, const char *title);
BOOL del_bookmark       (const char * lnk);
BOOL add_bookmark_group (const char * lnk, const char *title);
BOOL del_bookmark_group (const char * grp);
BOOL set_bookmark_group (const char * grp, BOOL openNclose);

#endif /*__BOOKMARK_H__*/
