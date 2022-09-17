#ifndef __BOOKMARK_H__
#define __BOOKMARK_H__

extern const char * bkm_File;
extern const char * bkm_CSS;

long bookmark_id2date (const char * id, char * buff, size_t b_len);

BOOL read_bookmarks (char ** old_file);
BOOL save_bookmarks (void);
BOOL pick_bookmarks (const char * name,
                     void (*progress)(ULONG size, ULONG part,
                                      const char * txt0, const char * txt1));

BOOL add_bookmark       (const char * url, const char * title);
BOOL del_bookmark       (const char * lnk);
BOOL pos_bookmark       (const char * lnk, BOOL dnNup);
BOOL txt_bookmark_entry (const char * id,  char * rw_buf, size_t lenNwr);
BOOL pos_bookmark_entry (const char * id,  const char * other);
BOOL add_bookmark_group (const char * lnk, const char * title);
BOOL del_bookmark_group (const char * grp);
BOOL pos_bookmark_group (const char * grp, BOOL dnNup);
BOOL set_bookmark_group (const char * grp, BOOL openNclose);

#endif /*__BOOKMARK_H__*/
