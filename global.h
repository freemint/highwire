/* @(#)highwire/global.h
 */
#include "defs.h"


/* for printf for testing */
#include <stdio.h>


/* ******** Variable Definitions ******************* */

/* Globals */
extern VDI_Workstation  vdi_dev;
#define vdi_handle     (vdi_dev.handle)
#define planes         (vdi_dev.planes)
#define ignore_colours (planes < 2)

extern WORD aes_max_window_title_length;

/* To be in-program settings */
extern WORD fonts[3][2][2];
extern WORD font_size;
extern WORD font_minsize;
extern WORD link_colour;
extern WORD highlighted_link_colour;
extern WORD text_colour;

extern WORD page_margin;

extern WORD slider_bkg;
extern WORD slider_col;

extern BOOL alternative_text_is_on;
extern BOOL force_frame_controls;

extern char	*va_helpbuf;  /* [HW_PATH_MAX] GLOBAL memory buffer for AV */

extern char fsel_path[HW_PATH_MAX];
extern char help_file[HW_PATH_MAX];

extern char * local_web; /* in Location.c */


/* ****************** Function Defs ******************************** */

/* in Nice_VDI.c */

WORD V_Opnvwk(VDI_Workstation *);
WORD V_Opnwk(WORD, VDI_Workstation *);

/* in HighWire.c */

void set_mouse_watch (WORD leaveNenter, const GRECT * watch);

/* in Mouse_R.c */

void button_clicked (WORD button, WORD clicks, UWORD state, WORD mx, WORD my);
void check_mouse_position (WORD mx, WORD my);

/* in AEI.c */

BOOL page_load   (void);
void rpopup_open (WORD, WORD);
void update_menu (ENCODING, BOOL raw_text);
BOOL process_messages (WORD msg[], PXY, UWORD state);
void    menu_open     (BOOL fsel);
void    menu_info     (void);
#define menu_quit()   exit(EXIT_SUCCESS)
void    menu_reload   (ENCODING);
void    menu_fontsize (char plus_minus);
void    menu_logging  (void);
void    menu_alt_text (void);
void    menu_frm_ctrl (void);
WORD    menu_history  (HISTORY hist[], UWORD used, WORD check);

/* in Widget.c */

#ifdef _GEMLIB_H_
WORD HW_form_do    (OBJECT *, WORD next);
#endif
WORD HW_form_popup (char * tab[], WORD x, WORD y, BOOL popNmenu);

/* in Redraws.c */

void frame_draw    (FRAME, const GRECT *, void * highlight);
void draw_hbar     (FRAME, BOOL complete);
void draw_vbar     (FRAME, BOOL complete);
void draw_contents (CONTENT *, long x, long y, const GRECT *, void * highlight);
void draw_border   (const GRECT *, short lu, short rd, short width);

/* in Config.c */

WORD read_config(char *fn);

/* in Frame.c */

FRAME new_frame    (LOCATION, TEXTBUFF,
                    ENCODING, UWORD mime_type, short margin_w, short margin_h);
void  delete_frame (FRAME *);
void     frame_finish    (FRAME, PARSER, TEXTBUFF);
void     frame_calculate (FRAME, const GRECT *);
BOOL     frame_slider    (struct slider *, long max_scroll);
OFFSET * frame_anchor    (FRAME, const char * name);
PARAGRPH frame_paragraph (FRAME, long x, long y, long area[4]);

/* in keyinput.c */

void key_pressed (WORD key, UWORD state);

/* in Loader.c */

void init_paths(void);
WORD identify_AES(void);
BOOL can_extended_mxalloc(void);
void launch_viewer(const char *name);

/* in render.c */

BOOL parse_html  (void*, long);
BOOL parse_plain (void*, long);
BOOL parse_image (void*, long);
BOOL parse_about (void*, long);
BOOL parse_dir   (void*, long);
void     render_hrule (TEXTBUFF, H_ALIGN align, short w, short h);
WORDITEM render_text  (TEXTBUFF, const char *);
WORDITEM render_link  (TEXTBUFF, const char *, const char * url, short color);

/* in list.c */

void list_start  (TEXTBUFF, BULLET, short counter);
void list_finish (TEXTBUFF);
void list_marker (TEXTBUFF, BULLET, short counter);

/* in O_Struct.c */

struct font_step * new_step     (WORD size, WORD color);
struct font_step * add_step     (struct font_step *);
struct font_step * destroy_step (struct font_step *);
ANCHOR new_named_location (const char * address, OFFSET *);
void   destroy_named_location_structure (ANCHOR);
struct url_link * new_url_link (WORDITEM start,
                                char * address, BOOL is_href, char * target);

/* in color.c */

void save_colors(void);
WORD remap_color (long value);

/* in Paragrph.c */

void     destroy_paragraph_structure (PARAGRPH);
PARAGRPH new_paragraph               (TEXTBUFF);
PARAGRPH add_paragraph               (TEXTBUFF, short vspace);
void     paragrph_finish  (TEXTBUFF);
WORDITEM paragrph_word    (PARAGRPH, long x, long y, long area[4]);
GRECT    paragraph_extend (WORDITEM);
void     content_setup     (CONTENT *, TEXTBUFF, short margins, short backgnd);
void     content_destroy   (CONTENT *);
long     content_minimum   (CONTENT *);
long     content_maximum   (CONTENT *);
long     content_calc      (CONTENT *, long width);
void     content_stretch   (CONTENT *, long height, V_ALIGN);
PARAGRPH content_paragraph (CONTENT *, long x, long y, long area[4]);


/* in image.c */

IMAGE new_image    (FRAME, TEXTBUFF, const char * src, LOCATION,
                    short w, short h, short vspace, short hspace);
void  delete_image (IMAGE*);
void image_calculate (IMAGE, short par_width);

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

void Init_AV_Protocol(void);
void Exit_AV_Protocol(void);
BOOL Send_AV(short to_ap_id, short message, const char *data1, const char *data2);
BOOL Receive_AV(const short msg[8]);
