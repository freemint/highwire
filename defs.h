/* @(#)highwire/defs.h
 */
#define _HIGHWIRE_MAJOR_     0
#define _HIGHWIRE_MINOR_     2
#define _HIGHWIRE_REVISION_  4
#define _HIGHWIRE_BETATAG_   "alpha"
#define _HIGHWIRE_VERSION_   "0.2.4"

#include "hw-types.h"   /* get base definitions */

#define _HIGHWIRE_FULLNAME_  "HighWire"
#define _HIGHWIRE_RSC_       "highwire.rsc"
#define _HIGHWIRE_CFG_       "highwire.cfg"

#define _ERROR_SPEEDO_   "needs SpeedoGDOS or NVDI ò 3!"
#define _ERROR_NOMEM_    "got no GLOBAL memory!"
#define _ERROR_NORSC_    "cannot load RSC file '"_HIGHWIRE_RSC_"'!"


#ifdef _HW_NO_GUI
# define _HIGHWIRE_REALINFO_  0 /* switch for the window infoline */
# define _HIGHWIRE_INFOLINE_  0 /* switch for the window infoline */
# define _HIGHWIRE_ENCMENU_   0 /* show encoding in menu */
# define _HIGHWIRE_RPOP_      0 /* switch for the right mouse popup */
# undef  GEM_MENU

#else
# define _HIGHWIRE_REALINFO_  0 /* switch for the window infoline */
# define _HIGHWIRE_INFOLINE_  1 /* switch for displaying info */
# define _HIGHWIRE_ENCMENU_   1 /* show encoding in menu */
# define _HIGHWIRE_RPOP_      1 /* switch for the right mouse popup */
# define  GEM_MENU
#endif


/* ******* Type Definitions ********* */

typedef enum {
	PAR_NONE = 0, PAR_LI, PAR_IMG
} PARAGRAPH_CODE;

typedef enum {
	ALN_LEFT = 0, ALN_JUSTIFY = 1, ALN_CENTER = 2, ALN_RIGHT = 3,
	#define ALN_NO_FLT ALN_JUSTIFY /* justify is impossible for floating */
	FLT_LEFT = 0x100, FLT_RIGHT = 0x103
	#define FLT_MASK 0x100
} H_ALIGN;

typedef enum {
	ALN_TOP = 0, ALN_MIDDLE, ALN_BOTTOM, ALN_BASELINE,
	ALN_ABOVE, ALN_BELOW   /* what are these both for?  Sub & Sup */
} V_ALIGN;

typedef enum {
	BRK_NONE = 0,
	BRK_LN = 0x01, BRK_LEFT = 0x11, BRK_RIGHT = 0x21, BRK_ALL = 0x31
} L_BRK;

typedef enum {
	SCROLL_AUTO = 0, SCROLL_ALWAYS, SCROLL_NEVER
} SCROLL_CODE;

typedef enum {
	METH_GET = 0, METH_POST
} FORM_METHOD;

typedef enum {  /* HTML:          CSS:          */
	LT_NONE = 0, /* ''                    'none' */
	LT_DISC,     /* 'disc'                'disc' */
	LT_SQUARE,   /* 'square'            'square' */
	LT_CIRCLE,   /* 'circle'            'circle' */
	LT_DECIMAL,  /* 'Number'           'decimal' */
	LT_L_ALPHA,  /* 'alpha'        'lower-alpha' */
	LT_U_ALPHA,  /* 'Alpha'        'upper-alpha' */
	LT_L_ROMAN,  /* 'roman'        'lower-roman' */
	LT_U_ROMAN/*, * 'Roman'        'upper-roman' */
/* not yet implemented:
	LT_L_LATIN,  / * ''           'lower-latin' * /
	LT_U_LATIN,  / * ''           'upper-latin' * /
	LT_DEZI_LZ   / * ''  'decimal-leading-zero' * /
*/
} BULLET;


/* http://www.indigo.ie/egt/standards/iso639/iso639-2-en.html
 * http://selfhtml.teamone.de/diverses/sprachenkuerzel.html
 * http://lcweb.loc.gov/standards/iso639-2/codechanges.html
 */
typedef enum {
	LANG_Unknown = 0,
	LANG_CA = ('c' << 8) + 'a',  /* Catalan, Katalanisch */
	LANG_CS = ('c' << 8) + 's',  /* Czech, Tschechisch */
	LANG_DA = ('d' << 8) + 'a',  /* Danish, D„nisch */
	LANG_DE = ('d' << 8) + 'e',  /* German, Deutsch */
	LANG_EL = ('e' << 8) + 'l',  /* Greek, Griechisch */
	LANG_EN = ('e' << 8) + 'n',  /* English, Englisch */
	LANG_ES = ('e' << 8) + 's',  /* Spanish, Spanisch */
	LANG_ET = ('e' << 8) + 't',  /* Estonian, Estnisch */
	LANG_FI = ('f' << 8) + 'i',  /* Finnish, Finnisch */
	LANG_FR = ('f' << 8) + 'r',  /* French, Franz”sisch */
	LANG_FY = ('f' << 8) + 'y',  /* Frisian, Friesisch */
	LANG_HE = ('h' << 8) + 'e',  /* Hebrew, Hebr„isch */
	LANG_HR = ('h' << 8) + 'r',  /* Croatian, Kroatisch */
	LANG_HU = ('h' << 8) + 'u',  /* Hungarian, Ungarisch */
	LANG_IT = ('i' << 8) + 't',  /* Italian, Italienisch */
	LANG_JA = ('j' << 8) + 'a',  /* Japanese, Japanisch */
	LANG_KO = ('k' << 8) + 'o',  /* Korean, Koreanisch */
	LANG_NL = ('n' << 8) + 'l',  /* Dutch, Niederl„ndisch */
	LANG_NB = ('n' << 8) + 'b',  /* Bokm†l Norwergian, Alt-Norwegisch */
	LANG_NN = ('n' << 8) + 'n',  /* Nynorsk Norwergian, Neu-Norwegisch */
	LANG_NO = ('n' << 8) + 'o',  /* Norwergian, Norwegisch */
	LANG_PL = ('p' << 8) + 'l',  /* Polish, Polnisch */
	LANG_PT = ('p' << 8) + 't',  /* Portuguese, Portugiesisch */
	LANG_RO = ('r' << 8) + 'o',  /* Rumanian, Rum„nisch */
	LANG_RU = ('r' << 8) + 'u',  /* Russian, Russisch */
	LANG_SH = ('s' << 8) + 'h',  /* Serbo-Croatian, Serbokroatisch, deprecated! */
	LANG_SK = ('s' << 8) + 'k',  /* Slovak, Slowakisch */
	LANG_SL = ('s' << 8) + 'l',  /* Slovenian, Slowenisch */
	LANG_SQ = ('s' << 8) + 'q',  /* Albanian, Albanisch */
	LANG_SR = ('s' << 8) + 'r',  /* Serbian, Serbisch */
	LANG_SV = ('s' << 8) + 'v',  /* Swedish, Schwedisch */
	LANG_TR = ('t' << 8) + 'r',  /* Turkish, Trkisch */
	LANG_YI = ('y' << 8) + 'i',  /* Yiddish, Jiddisch */
	LANG_ZH = ('z' << 8) + 'h'   /* Chinese, Chinesisch */
} LANGUAGE;


/***** Forward Declarations *****/

typedef struct s_table_stack  * TBLSTACK;   /* in Table.c */
typedef struct s_table        * TABLE;      /* in Table.c */
typedef struct named_location * ANCHOR;
typedef struct s_map_area     * MAPAREA;
typedef struct s_img_map      * IMAGEMAP;
typedef struct frame_item     * FRAME;
typedef struct paragraph_item * PARAGRPH;
typedef struct word_item      * WORDITEM;
typedef struct s_font         * FONT;       /* in fontbase.h */
typedef struct s_location     * LOCATION;   /* in Location.h */
#if !defined(__PUREC__)
typedef struct s_containr     * CONTAINR;
typedef struct s_history      * HISTORY;
typedef struct s_parser       * PARSER;
typedef struct s_form         * FORM;
typedef struct s_input        * INPUT;
#else
# ifndef  CONTAINR
#  define CONTAINR void*
# endif
# ifndef  HISTORY
#  define HISTORY  void*
# endif
# ifndef  PARSER
#  define PARSER   void*
# endif
# ifndef  FORM
#  define FORM     void*
# endif
# ifndef  INPUT
#  define INPUT    void*
# endif
#endif


/* ********** Consts ************** */

#define SPACE_BICS  561
#define SPACE_ATARI ' '
#define SPACE_UNI   0x0020 /* ' ' */

#define NOBRK_BICS  560
#define NOBRK_ATARI (0x100|SPACE_ATARI)
#define NOBRK_UNI   0x00A0

#define normal_font 0
#define header_font 1
#define pre_font 2
#define scroll_bar_width 15
#define scroll_step 24
#define DOMAIN_MINT 1
#define HW_PATH_MAX 260  /* at least VFAT standard 260 */


/* ********** Structures ***************** */

typedef struct {
	WORD handle;
	WORD dev_id;
	WORD wchar;
	WORD hchar;
	WORD wbox;
	WORD hbox;
	WORD xres;
	WORD yres;
	WORD noscale;
	WORD wpixel;
	WORD hpixel;
	WORD cheights;
	WORD linetypes;
	WORD linewidths;
	WORD markertypes;
	WORD markersizes;
	WORD faces;
	WORD patterns;
	WORD hatches;
	WORD colors;
	WORD ngdps;
	WORD cangdps[10];
	WORD gdpattr[10];
	WORD cancolour;
	WORD cantextrot;
	WORD canfillarea;
	WORD cancellarray;
	WORD palette;
	WORD locators;
	WORD valuators;
	WORD choicedevs;
	WORD stringdevs;
	WORD wstype;
	WORD minwchar;
	WORD minhchar;
	WORD maxwchar;
	WORD maxhchar;
	WORD minwline;
	WORD zero5;
	WORD maxwline;
	WORD zero7;
	WORD minwmark;
	WORD minhmark;
	WORD maxwmark;
	WORD maxhmark;
	WORD screentype;
	WORD bgcolors;
	WORD textfx;
	WORD canscale;
	WORD planes;
	WORD lut;
	WORD rops;
	WORD cancontourfill;
	WORD textrot;
	WORD writemodes;
	WORD inputmodes;
	WORD textalign;
	WORD inking;
	WORD rubberbanding;
	WORD maxvertices;
	WORD maxintin;
	WORD mousebuttons;
	WORD widestyles;
	WORD widemodes;
	WORD clipflag;  /* GEM 2, NVDI */
	WORD pixelm;  /* GEM 3 */
	WORD wpixelm;  /* GEM 3 */
	WORD hpixelm;  /* GEM 3 */
	WORD hpixelperinch;  /* GEM 3 */
	WORD vpixelperinch;  /* GEM 3 */
	WORD prnrot;  /* GEM 3 */
	void *quarter;  /* GEM 3 */
	WORD flags;  /* GEM 3, NVDI */
	WORD zero8;
	WORD flags2;  /* NVDI 5 */
	WORD zero9[14];
	WORD clipx;  /* undocumented! */
	WORD clipy;  /* undocumented! */
	WORD clipw;  /* undocumented! */
	WORD cliph;  /* undocumented! */
	WORD reserved[8];
} VDI_Workstation;


/***** Generic Structure, used in all Block Elements *****/

typedef struct {
	long X, Y;
	long W, H;
} LRECT;

typedef struct {
	WORD Top, Bot;
	WORD Lft, Rgt;
} TBLR;

typedef enum {
	BC_MAIN = 0, /* BODY                        */
	BC_TABLE,    /* TABLE                       */
	BC_STRUCT,   /* TD, TH                      */
	BC_GROUP,    /* DIV, CENTER, BLOCKQUOTE, .. */
	BC_LIST,     /* DL, OL, UL, MENU            */
	BC_MIXED,    /* LI, ..                      */
	BC_TXTPAR,   /* P, ..                       */
	BC_SINGLE    /* IMG, HR, ..                 */
} BOXCLASS;

typedef struct blocking_area * BLOCKER;
typedef struct s_dombox        DOMBOX;
struct s_dombox {
	struct s_dombox_vtab {
		void     (*delete)(DOMBOX *);
		LONG     (*MinWidth) (DOMBOX *);
		LONG     (*MaxWidth) (DOMBOX *);
		PARAGRPH (*Paragrph) (DOMBOX *);
		DOMBOX * (*ChildAt)  (DOMBOX *, LRECT *, long x, long y, long clip[4]);
		void     (*draw)   (DOMBOX *, long x, long y, const GRECT * clip, void *);
		void     (*format) (DOMBOX *, long width, BLOCKER);
	}      * _vtab;
	DOMBOX * Parent, * Sibling;
	DOMBOX * ChildBeg, * ChildEnd;
	LRECT    Rect;
	LONG     MaxWidth;
	LONG     MinWidth; /* smallest width where the content fits in          */
	LONG     SetWidth; /* set Value, must not be smaller than Minimum       */
	char   * IdName;   /* from id="<ID>" attribute, refferenced by ".ID"    */
	char   * ClName;   /* from class="<CL>" attribute, refferenced by "#CL" */
	BOXCLASS BoxClass;
	WORD     HtmlCode;
	WORD     Backgnd;  /* -1 for transparency, or colour value              */
	TBLR     Margin;
	TBLR     Padding;
	WORD     BorderWidth, BorderColor;
	H_ALIGN  Floating;
	H_ALIGN  TextAlign;
	WORD     TextIndent; /* paragraph hanginging, <0: left, >0: right       */
};
extern struct s_dombox_vtab DomBox_vTab;
DOMBOX * dombox_ctor (DOMBOX *, DOMBOX * parent, BOXCLASS);
DOMBOX * dombox_dtor (DOMBOX *);
#define  Delete(this)   ((*((this)->_vtab->delete))(this))
#define  dombox_Dist(this,what)    ((this)->Margin.what + (this)->Padding.what)
#define  dombox_TopDist(this)      (dombox_Dist(this,Top) + (this)->BorderWidth)
#define  dombox_BotDist(this)      (dombox_Dist(this,Bot) + (this)->BorderWidth)
#define  dombox_LftDist(this)      (dombox_Dist(this,Lft) + (this)->BorderWidth)
#define  dombox_RgtDist(this)      (dombox_Dist(this,Rgt) + (this)->BorderWidth)
DOMBOX * dombox_Offset  (DOMBOX *, long * x, long * y);
#define  dombox_MinWidth(this)     ((*((this)->_vtab->MinWidth))(this))
#define  dombox_MaxWidth(this)     ((*((this)->_vtab->MaxWidth))(this))
#define  dombox_Paragrph(this)     ((*((this)->_vtab->Paragrph))(this))
DOMBOX * dombox_byCoord (DOMBOX *, LRECT *, long * px, long * py);
void     dombox_draw    (DOMBOX *, long x, long y, const GRECT * clip, void *);
char *   dombox_setId   (DOMBOX *, const char *, BOOL force);
char *   dombox_setClass(DOMBOX *, const char *, BOOL force);
void     dombox_reorder (DOMBOX *, DOMBOX * behind);
void     dombox_adopt   (DOMBOX *, DOMBOX * stepchild);
void     dombox_format  (DOMBOX *, long width);
void     dombox_stretch (DOMBOX *, long height, V_ALIGN valign);

struct blocking_area {
	struct {
		long width;
		long bottom;
	} L, R;
};


/* ************ Parsing Constructs ************************ */

typedef struct long_offset {
	DOMBOX * Origin;
	long     X, Y;
} OFFSET;

struct s_img_data {
	/* MFDB */
	void * fd_addr;
	WORD   fd_w, fd_h;
	WORD   fd_wdwidth;
	WORD   fd_stand;
	WORD   fd_nplanes;
	WORD   fd_r1, fd_r2, fd_r3;
	/*---*/
	UWORD  img_w, img_h; /* original extents           */
	WORD   fgnd,  bgnd;  /* colors for duo-chrome data */
	size_t mem_size;     /* additional size, of data   */
};
typedef const struct s_img_data * cIMGDATA;
typedef       struct s_img_data * pIMGDATA;

typedef struct s_image {
	union {
		void   * Mfdb;
		cIMGDATA Data;
	}        u;
	WORDITEM word;
	PARAGRPH paragraph;
	FRAME    frame;
	LOCATION source;
	IMAGEMAP map;
	OFFSET offset;
	short  backgnd;
	short  set_w,  set_h;
	short  disp_w, disp_h;
	short  alt_w,  alt_h;
	short  vspace;
	short  hspace;
} * IMAGE;

typedef struct s_fontstack * FNTSTACK;
typedef struct s_fontstack   FNTSBASE;
struct s_fontstack {
	FNTSTACK Prev, Next;
	WORD Color;
	WORD Type;  /* normal, header, header */
	WORD Size;  /* real size in point */
	WORD Step;  /* HTML font size 0..7 */
	BOOL setBold;
	BOOL setItalic;
	BOOL setUndrln;
	BOOL setStrike;
	BOOL setCondns;
	BOOL setNoWrap;
};

struct font_style {
	unsigned italic;
	unsigned bold;
	unsigned underlined;
	unsigned strike;
};

typedef union {
	unsigned long packed;
	struct {
		unsigned _pad1  :8;    /* struct alignment, always 0 */
		unsigned color  :8;
		unsigned undrln :1; /* underlined     */
		unsigned _pad2  :1;    /* struct alignment, always 0 */
		unsigned strike :1; /* strike through */
		unsigned condns :1; /* condensed      */
		unsigned italic :1;
		unsigned bold   :1;
		unsigned font   :2;
		unsigned size   :8;
	} s;
} TEXTATTR;
#define TEXTATTR(s,c) (((long)(c & 0xFF) <<16) | (s & 0xFF))
                      /* pseudo constructor for text size and colour */
#define TA_SizeMASK 0x000000FFuL
#define TA_FaceMASK 0x00001F00uL
#define TA_AttrMASK 0x0000A000uL
#define TA_ColrMASK 0x00FF0000uL
#define TA_ALL_MASK (TA_SizeMASK|TA_FaceMASK|TA_AttrMASK|TA_ColrMASK)

#ifdef __LATICE__
# define _TAsplit(a,n) (((unsigned char *)&(a).packed)[n])
# define TA_Color(a)      _TAsplit(a,1)
# define TAsetUndrln(a,b) {if(b) _TAsplit(a,2)|=0x80; else _TAsplit(a,2)&=0x7F;}
# define TAgetUndrln(a)   ((_TAsplit(a,2) & 0x80) ? 8 :0)
# define TAsetStrike(a,b) {if(b) _TAsplit(a,2)|=0x20; else _TAsplit(a,2)&=0xDF;}
# define TAgetStrike(a)   ((_TAsplit(a,2) & 0x20) ? 1 :0)
# define TAsetCondns(a,b) {if(b) _TAsplit(a,2)|=0x10; else _TAsplit(a,2)&=0xEF;}
# define TAgetCondns(a)   ((_TAsplit(a,2) & 0x40) ? 4 :0)
# define TAsetItalic(a,b) {if(b) _TAsplit(a,2)|=0x08; else _TAsplit(a,2)&=0xF7;}
# define TAgetItalic(a)   ((_TAsplit(a,2) & 0x08) ? 1 :0)
# define TAsetBold(a,b)   {if(b) _TAsplit(a,2)|=0x04; else _TAsplit(a,2)&=0xFB;}
# define TAgetBold(a)     ((_TAsplit(a,2) & 0x04) ? 1 :0)
# define TAsetFont(a,f)   {_TAsplit(a,2) = (_TAsplit(a,2) & 0xFC) | (f & 0x03);}
# define TAgetFont(a)     (_TAsplit(a,2) & 0x03)
# define TA_Size(a)       _TAsplit(a,3)
#else
# define TA_Color(a)      ((a).s.color)
# define TAsetUndrln(a,b) {(a).s.undrln = (b ? 1 : 0);}
# define TAgetUndrln(a)   ((a).s.undrln ? 8 :0)
# define TAsetStrike(a,b) {(a).s.strike = (b ? 1 : 0);}
# define TAgetStrike(a)   ((a).s.strike ? 1 :0)
# define TAsetCondns(a,b) {(a).s.condns = (b ? 1 : 0);}
# define TAgetCondns(a)   ((a).s.condns ? 1 :0)
# define TAsetItalic(a,b) {(a).s.italic = (b ? 1 : 0);}
# define TAgetItalic(a)   ((a).s.italic ? 1 :0)
# define TAsetBold(a,b)   {(a).s.bold = (b ? 1 : 0);}
# define TAgetBold(a)     ((a).s.bold ? 1 :0)
# define TAsetFont(a,f)   {(a).s.font = f;}
# define TAgetFont(a)     ((a).s.font)
# define TA_Size(a)       ((a).s.size)
#endif
#define TAgetFace(a) (((short)(a).packed & 0x0F00) >>8)

typedef struct word_line {
	PARAGRPH Paragraph;  /* back reference */
	long     OffsetY;    /* distance of the baseline to the paragraph's top */
	WORDITEM Word;    /* the first word of the line */
	short    Count;   /* number of words */
	short    Ascend;  /* space above the baseline */
	short    Descend; /* space below the baseline */
	long     Width;
	struct word_line * NextLine;
} * WORDLINE;

struct word_item {
	const UWORD _priv; /* * for internal use, don't touch! */
	UWORD    length;   /* number of 16-bit characters in item */
	WCHAR  * item;
	TEXTATTR attr;
	FONT     font;
	WORD h_offset;
	WORD word_width;
	WORD word_height;
	WORD word_tail_drop;
	WORD space_width;
	BOOL wrap;
	L_BRK   line_brk;
	V_ALIGN vertical_align;
	struct url_link  * link;
	IMAGE    image;
	INPUT    input;
	WORDLINE line;     /* back reference */
	WORDITEM next_word;
};

struct paragraph_item {
	DOMBOX   Box;
	WORDITEM item;
	WORDLINE Line;    /* list of word lines */
	PARAGRAPH_CODE paragraph_code;
	PARAGRPH next_paragraph;
};

struct frame_item {
	DOMBOX   Page;
	CONTAINR Container;
	LOCATION Location;
	LOCATION BaseHref;
	ENCODING Encoding;
	BOOL     ForceEnc; /* don't use encoding from meta tag */
	LANGUAGE Language;
	UWORD    MimeType; /* determines whether the content needs no parsing */
	struct slider {
		long scroll;
		BOOL on;
		WORD lu, rd; /* left/up, right/down arrow borders */
		WORD pos;    /* slider position */
		WORD size;   /* slider size     */
	}    v_bar, h_bar;
	SCROLL_CODE scroll;
	WORD     text_color;
	WORD     link_color;
	char   * base_target;
	GRECT    clip;
	ANCHOR   first_named_location;
	IMAGEMAP MapList;
	FORM     FormList;        /* FORM list for user interaction */
};

typedef struct list_stack_item * LSTSTACK;

typedef struct parse_sub {
	LSTSTACK           lst_stack;
	TBLSTACK           tbl_stack;
	void             * form;
	LANGUAGE           quot_lang; /* language used for quotation marks */
	short              backgnd;
	FNTSBASE           fnt_stack;
	FNTSTACK           font;
	struct font_style  styles;
	UWORD              nowrap;
	DOMBOX           * parentbox;
	PARAGRPH           paragraph;
	PARAGRPH           prev_par;
	WORDITEM           word;      /* the item actually to process */
	WORDITEM           prev_wrd;  /* <br> tags affect this        */
	ANCHOR           * anchor;
	MAPAREA          * maparea;
	WCHAR            * text;  /* points behind the last stored character */
	WCHAR            * buffer;
} PARSESUB;
typedef struct parse_sub * TEXTBUFF;

struct list_stack_item {
	WORD     Type;
	BULLET   BulletStyle;
	WORD     Counter;
	short    Hanging;  /* left shift value for <li> bullet/number  */
	PARAGRPH ListItem; /* last <li> which contains a bullet/number */
	FNTSTACK FontStk;
	LSTSTACK next_stack_item;
};

/* ******************** Action Constructs ************************* */

struct named_location {
	OFFSET offset;
	const char * address;
	struct named_location *next_location;
};

struct url_link {
	BOOL     isHref;
	char   * address;
	WORDITEM start;   /* where this link is reffered the first time */
	union {
		ANCHOR anchor; /* if isHref == FALSE */
		char * target; /* if isHref == TRUE  */
	} u;
	ENCODING encoding;
};

struct s_map_area {
	MAPAREA Next;
	char  * Address;
	char  * Target;
	char  * AltText;
	WORD    Type;   /* 'R'ect, 'C'ircle, 'P'oly */
	union {
		/* all */ GRECT Extent;
		struct  { GRECT Extent;                           } Rect;
		struct  { GRECT Extent; WORD  Radius; PXY Centre; } Circ;
		struct  { GRECT Extent; WORD  Count;  PXY P[1];   } Poly;
	} u;
};

struct s_img_map {
	IMAGEMAP        Next;
	MAPAREA         Areas;
	struct url_link Link;
	char            Name[1];
};


#ifdef __PUREC__
/* Pure C 1.1 reports errors as "Undefined structure 's_table[_stack]'"!
 * So let the compiler know the structures. */
#include "Table.h"
#include "Location.h"
#include "fontbase.h"
#endif /* __PUREC__ */
