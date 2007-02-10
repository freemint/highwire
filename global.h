/* @(#)highwire/global.h
 */
#include "defs.h"


/* for printf for testing */
#include <stdio.h>


/* ******** Variable Definitions ******************* */

extern const BOOL mem_TidyUp; /* for debugging, to release memory *
                               * at program end                   */

/* Globals */
extern VDI_Workstation  vdi_dev;
#define vdi_handle     (vdi_dev.handle)
#define planes         (vdi_dev.planes)
#define ignore_colours (planes < 2)

extern LONG hdr_tout_doc; /* milliseconds to wait for a reply header */
extern LONG hdr_tout_gfx; /* same as above but for graphics etc.     */

extern WORD aes_max_window_title_length;

/* To be in-program settings */
extern WORD fonts[3][2][2];
extern WORD font_size;
extern WORD font_minsize;
extern WORD link_colour;
extern WORD highlighted_link_colour;
extern WORD background_colour;
extern WORD text_colour;

extern WORD page_margin;

extern WORD slider_bkg;
extern WORD slider_col;

extern BOOL force_frame_controls;

extern char	*va_helpbuf;  /* [HW_PATH_MAX] GLOBAL memory buffer for AV */

extern char fsel_path[HW_PATH_MAX];
extern char help_file[HW_PATH_MAX];

extern char * local_web; /* in Location.c */
/* extern BOOL cfg_image1; in image.c */


/* ****************** Function Defs ******************************** */

/* in Nice_VDI.c */

WORD V_Opnvwk(VDI_Workstation *);
WORD V_Opnwk(WORD, VDI_Workstation *);

/* in HighWire.c */

void set_mouse_watch (WORD leaveNenter, const GRECT * watch);

/* in Mouse_R.c */

void button_clicked (CONTAINR, WORD button, WORD clicks, UWORD state, PXY);
void check_mouse_position (WORD mx, WORD my);

/* in AEI.c */

#define SYS_TOS    0x0001
#define SYS_MAGIC  0x0002
#define SYS_MINT   0x0004
#define SYS_GENEVA 0x0010
#define SYS_NAES   0x0020
#define SYS_XAAES  0x0040
extern UWORD _systype_v;
extern UWORD _systype(void);
/* detect the system type, AES + kernel */
#define sys_type()    (_systype_v ? _systype_v : _systype())
#define sys_MAGIC()   ((sys_type() & SYS_MAGIC) != 0)
#define sys_NAES()    ((sys_type() & SYS_NAES)  != 0)
#define sys_XAAES()   ((sys_type() & SYS_XAAES) != 0)

char * file_selector (const char * label, const char * patt,
                      const char * file, char * buff, size_t blen);
void rpopup_open    (WORD, WORD);
void rpoplink_open  (WORD, WORD, CONTAINR, void *);
void rpopilink_open (WORD, WORD, CONTAINR, void *);
void rpopimg_open   (WORD, WORD, CONTAINR);
void update_menu (ENCODING, BOOL raw_text);
BOOL process_messages (WORD msg[], PXY, UWORD state);
void    menu_open     (BOOL fsel);
void    menu_info     (void);
#define menu_quit()   exit(EXIT_SUCCESS)
void    menu_reload   (ENCODING);
void    menu_fontsize (char plus_minus);
void    menu_cookies  (int);
void    menu_images   (int);
void    menu_use_css  (int);
void    menu_frm_ctrl (int);
void    menu_logging  (int);
WORD    menu_history  (HISTORY hist[], UWORD used, WORD check);

/* in formwind.c */

#ifdef _GEMLIB_H_
WORD formwind_do (OBJECT *, WORD start, const char * title,
                  BOOL modal, BOOL(*handler)(OBJECT*,WORD));
#endif

/* in dl_mngr.c */

void dl_manager (LOCATION what, LOCATION ref);

/* in fntsetup.c */

void fonts_setup (WORD msg[]);

/* in Widget.c */

#ifdef _GEMLIB_H_
WORD HW_form_do    (OBJECT *, WORD next);
#endif
WORD HW_form_popup (char * tab[], WORD x, WORD y, BOOL popNmenu);

#ifndef VARIADIC            /* Gnu C provides a (non-portable) method to have */
# ifndef __GNUC__           /* the compiler check the arguments of a variadic */
#  define VARIADIC(v,a)     /* function against its format string if this is  */
# else                      /* compatible with printf or scanf.               */
#  define VARIADIC(v,a) __attribute__ ((format (printf, v, a)))
# endif
#endif
void hwUi_fatal (const char * hint, const char * text, ...) VARIADIC(2,3);
void hwUi_error (const char * hint, const char * text, ...) VARIADIC(2,3);
void hwUi_warn  (const char * hint, const char * text, ...) VARIADIC(2,3);
void hwUi_info  (const char * hint, const char * text, ...) VARIADIC(2,3);

/* in Redraws.c */

void frame_draw     (FRAME, const GRECT *, void * highlight);
void draw_hbar      (FRAME, BOOL complete);
void draw_vbar      (FRAME, BOOL complete);
void draw_hr        (PARAGRPH, WORD x, WORD y);
long draw_paragraph (PARAGRPH, WORD x, long y, const GRECT *, void * highlight);
void draw_border    (const GRECT *, short lu, short rd, short width);
void draw_TBLR_border (const GRECT *, short lu, short rd, TBLR width);

/* in Config.c */

extern WORD         cfg_UptoDate;
extern const char * cfg_File;
extern const char * cfg_StartPage;
extern BOOL         cfg_AllowCookies;
extern BOOL         cfg_DropImages;  /* view ALT-texts instead of the image */
extern BOOL         cfg_ViewImages;  /* view image instead of a placeholder */
extern BOOL         cfg_FixedCmap;
extern BOOL         cfg_UseCSS;
extern BOOL         cfg_AVWindow; /* AVSERVER window enables AV-drag&drop */
extern BOOL         cfg_GlobalCycle; /* global window cycling by AV_SENDKEY */
extern WORD         cfg_ConnTout;  /* seconds for connection establishing  */
extern WORD         cfg_ConnRetry; /* number of tries for connection that  *
                                    * couldn't get established immediately */
BOOL read_config (void);
BOOL save_config (const char * key, const char * arg);
const char * devl_flag (const char * flag);


/* in Frame.c */

FRAME new_frame    (LOCATION, TEXTBUFF,
                    ENCODING, UWORD mime_type, short margin_w, short margin_h);
void  delete_frame (FRAME *);
void     frame_finish    (FRAME, PARSER, TEXTBUFF);
void     frame_calculate (FRAME, const GRECT *);
BOOL     frame_slider    (struct slider *, long max_scroll);
OFFSET * frame_anchor    (FRAME, const char * name);

/* in keyinput.c */

void key_pressed (WORD scan, WORD ascii, UWORD state);

/* in Loader.c */

void init_paths(void);
void launch_viewer (const char *name);

/* in render.c */

FNTSTACK css_text_styles (PARSER, FNTSTACK);
void     css_box_styles  (PARSER, DOMBOX *, H_ALIGN);
int parse_html  (void*, long);
int parse_plain (void*, long);
int parse_image (void*, long);
int parse_about (void*, long);
int parse_dir   (void*, long);
DOMBOX * create_box (TEXTBUFF, BOXCLASS, WORD par_top);
DOMBOX * leave_box  (TEXTBUFF, WORD tag);
DOMBOX * render_hrule (TEXTBUFF, H_ALIGN, short w, short size, BOOL shade);
WORDITEM render_text  (TEXTBUFF, const char *);
WORDITEM render_link  (TEXTBUFF, const char *, const char * url, short color);

/* in list.c */

void list_start  (PARSER, TEXTBUFF, BULLET, short counter, short type);
void list_finish (TEXTBUFF);
void list_marker (TEXTBUFF, BULLET, short counter);

/* in O_Struct.c */

ANCHOR new_named_location (const char * address, DOMBOX *);
void   destroy_named_location_structure (ANCHOR);
struct url_link * new_url_link (WORDITEM start,
                                char * address, BOOL is_href, char * target);
IMAGEMAP create_imagemap  (IMAGEMAP * list, const char * name, BOOL empty);
void     destroy_imagemap (IMAGEMAP * list, BOOL all);
MAPAREA  new_maparea (const char * shape, const char * coords,
                      char * href, char * target, char * text);

/* in color.c */

extern BOOL color_FixedMap;
void  color_mapsetup(void);
void  save_colors   (void);
WORD  remap_color   (long value);
ULONG color_lookup  (WORD idx);
void  color_tables  (ULONG cube[216], ULONG gray[32], WORD pixel_val[256]);

/* in Paragrph.c */

PARAGRPH new_paragraph    (TEXTBUFF);
PARAGRPH add_paragraph    (TEXTBUFF, short vspace);
void     paragrph_finish  (TEXTBUFF);
WORDITEM paragrph_word    (PARAGRPH, long x, long y, long area[4]);
GRECT    paragraph_extend (WORDITEM);

/* in image.c */

IMAGE new_image    (FRAME, TEXTBUFF, const char * src, LOCATION,
                    short w, short h, short vspace, short hspace, BOOL win_image);
void  delete_image (IMAGE*);
void         image_calculate (IMAGE, short par_width);
const char * image_dispinfo  (void);

/* in W_Struct.c */

WORDITEM destroy_word_list (WORDITEM start, WORDITEM last_OR_null);
WORDITEM new_word   (TEXTBUFF, BOOL do_link);
void     word_store (TEXTBUFF);
void word_set_bold      (TEXTBUFF, BOOL onNoff);
void word_set_italic    (TEXTBUFF, BOOL onNoff);
void word_set_strike    (TEXTBUFF, BOOL onNoff);
void word_set_underline (TEXTBUFF, BOOL onNoff);
#define word_set_color(textbuff, color) {TA_Color((textbuff)->word->attr)=color;}
#define word_set_point(textbuff, point) {TA_Size ((textbuff)->word->attr)=point;}
#define word_set_font( textbuff, font)  {TAsetFont((textbuff)->word->attr,font);}
long word_offset (WORDITEM);

/* in av_prot.c */

short get_avserver     (void);
void  Init_AV_Protocol (void);
void  Exit_AV_Protocol (void);
BOOL  Receive_AV       (short msg[8]);
BOOL  Send_AV	        (short message, const char *data1, const char *data2);

void send_avwinopen  (short handle);
void send_avwinclose (short handle);  
