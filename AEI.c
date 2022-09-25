/* @(#)highwire/AEI.c
 */
#include <stdlib.h>
#include <string.h>

#ifdef __PUREC__
#include <tos.h>

#else /* LATTICE || __GNUC__ */
#include <osbind.h>
#endif

#include <gem.h>

#include "token.h" /* rpopbkm_open */

#ifdef __PUREC__
# define CONTAINR struct s_containr *
# define HISTORY  struct s_history *
#endif
#include "global.h"

#include "version.h"
#include "vaproto.h"
/* #include "av_comm.h" */
#include "bookmark.h"
#include "Location.h"
#include "Containr.h"
#include "hwWind.h"
#include "Loader.h"
#include "cache.h"
#include "Logging.h"
#include "dragdrop.h"
#include "olga.h"
#ifdef GEM_MENU
#	include "highwire.h"
	extern OBJECT * menutree;
#endif


/*==============================================================================
 * Checks if appl_getinfo() exists, and, if possible, calls it.
 * WINX has appl_getinfo() since 2.2, appl_find("?AGI    ") since 2.3c beta.
 */
#if (__GEMLIB_MINOR__<42)||((__GEMLIB_MINOR__==42)&&(__GEMLIB_REVISION__<2))
WORD appl_xgetinfo(WORD type, WORD *out1, WORD *out2, WORD *out3, WORD *out4)
{
	static WORD has_agi = -1;
	WORD u;

	if (has_agi < 0)
		has_agi = gl_ap_version >= 0x400 || appl_find("?AGI    ") == 0
		          || wind_get(0, WF_WINX, &u, &u, &u, &u) == WF_WINX;
		          /* || MagiC_version >= 0x0200? */

	if (has_agi)
		return appl_getinfo(type, out1, out2, out3, out4);
	else
		return FALSE;
}
#endif


/*==============================================================================
 * Detects the system type, AES + kernel.
 * This function shouldn't be called directly, use the sys_type() instead!
*/
UWORD _systype_v;
UWORD
_systype (void)
{
	long * ptr = (long *)Setexc(0x5A0/4, (void (*)())-1);
	
	_systype_v = SYS_TOS;
	
	if (!ptr) {
		return _systype_v;   /* stone old TOS without any cookie support */
	}
	while (*ptr) {
		if (*ptr == 0x4D674D63L/*MgMc*/ || *ptr == 0x4D616758L/*MagX*/) {
			_systype_v = (_systype_v & ~0xF) | SYS_MAGIC;
		} else if (*ptr == 0x4D694E54L/*MiNT*/) {
			_systype_v = (_systype_v & ~0xF) | SYS_MINT;
		
		} else if (*ptr == 0x476E7661L/*Gnva*/) {
			_systype_v |= SYS_GENEVA;
		} else if (*ptr == 0x6E414553L/*nAES*/) {
			_systype_v |= SYS_NAES;
		}
		ptr += 2;
	}
	if (_systype_v & SYS_MINT) { /* check for XaAES */
		WORD out = 0, u;
		if (wind_get (0, (((WORD)'X') <<8)|'A', &out, &u,&u,&u) && out) {
			_systype_v |= SYS_XAAES;
		}
	}
	return _systype_v;
}


/*============================================================================*/
char *
file_selector (const char * label, const char * patt,
               const char * file, char * buff, size_t blen)
{
	char fsel_file[256] = "";
	WORD r, butt;
	char * slash = strrchr (fsel_path, '\\');
	if (!slash) {
		strcpy (fsel_path, "C:\\");
		slash = fsel_path +2;
	}
	strcpy (slash +1, (patt ? patt : "*.*"));
	if (file && *file) {
		size_t f_len = strlen (file);
		if (f_len >= sizeof(fsel_file)) {
			f_len = sizeof(fsel_file) -1;
		}
		((char*)memcpy (fsel_file, file, f_len))[f_len] = '\0';
	}
	if ((gl_ap_version >= 0x140 && gl_ap_version < 0x200)
	    || gl_ap_version >= 0x300 /* || getcookie(FSEL) */) {
		r = fsel_exinput (fsel_path, fsel_file, &butt, label);
	} else {
		r = fsel_input (fsel_path, fsel_file, &butt);
	}
	if (!r || butt == FSEL_CANCEL) {
		if (buff) {
			*buff = '\0';
			buff  = NULL;
		}
	} else {
		size_t f_len = strlen (fsel_file);
		size_t p_len = strlen (fsel_path);
		while (p_len && fsel_path[p_len-1] != '\\') p_len--;
		if (f_len && p_len) {
			if (!buff) {
				buff = malloc (f_len + p_len +1);
			} else if (blen < f_len + p_len +1) {
				buff = NULL;
			}
			if (buff) {
				memcpy (buff,         fsel_path, p_len);
				memcpy (buff + p_len, fsel_file, f_len +1);
			}
		}
	}
	return buff;
}


/*----------------------------------------------------------------------------*/
static BOOL
page_load(void)
{
	char fsel_file[HW_PATH_MAX] = "";
	const char * label = "HighWire: Open HTML or text";
	const char * patt  = (sys_XAAES() ? "*.[HJPT]*" : NULL);
	if (!file_selector (label, patt, NULL, fsel_file,sizeof(fsel_file))) {
		return FALSE;
	} else {
		start_cont_load (hwWind_Top->Pane, fsel_file, NULL, TRUE, TRUE);
	}
	return TRUE;
}


/*------------------------------------------------------------------------------
 * rewrote by Rainer  May 2002
*/
static void
vastart (const WORD msg[8], PXY mouse, UWORD state)
{
	short answ[8];
	char filename[HW_PATH_MAX];
	const char *cmd = *(const char *const *)&msg[3];
	/* if a parameter contains spaces then
	 *    it is enclosed with '...', and a ' is encoded as '' */
	const BOOL quoted = (cmd[0] == '\'' && cmd[1] != '\0'
	                     && strrchr(&cmd[2], '\'') != NULL
	                     && strrchr(&cmd[2], '\'')[1] == '\0');
	
	HwWIND wind = (state && K_ALT
	               ? NULL : hwWind_byCoord (mouse.p_x, mouse.p_y));
	if (quoted) {
		char *p = filename;

		cmd++;
		while (cmd[0] != '\0') {
			if (cmd[0] == '\'' && cmd[1] == '\'')
				cmd++;
			p++[0] = cmd++[0];
		}
		p[-1] = '\0';  /* overwrite closing ' */
		cmd = filename;
	}
	
	if (wind) {
		start_page_load (wind->Pane, cmd, NULL, TRUE, NULL);
	} else {
		new_hwWind ("", cmd, TRUE);
	}

	answ[0] = (msg[0] == VA_START)? AV_STARTED : VA_WINDOPEN;
	answ[1] = gl_apid;
	answ[2] = 0;
	answ[3] = msg[3];
	answ[4] = msg[4];
	answ[5] = 0;
	answ[6] = 0;
	answ[7] = 0;
	appl_write(msg[1], 16, answ);
}


/*----------------------------------------------------------------------------*/
/* GEMScript support added
 * Matthias Jaap  December 21, 2001
 */

#define GS_REQUEST    0x1350
#define GS_REPLY      0x1351
#define GS_COMMAND    0x1352
#define GS_ACK        0x1353
#define GS_QUIT       0x1354
#define GS_OPENMACRO  0x1355
#define GS_MACRO      0x1356
#define GS_WRITE      0x1357
#define GS_CLOSEMACRO 0x1358

#define GSM_COMMAND   0x0001
#define GSM_MACRO     0x0002
#define GSM_WRITE     0x0004
#define GSM_HEXCODING 0x0008

#define GSACK_OK      0
#define GSACK_UNKNOWN 1
#define GSACK_ERROR   2

typedef struct
{
	long len;
	WORD version;
	WORD msgs;
	long ext;
} GS_INFO;

WORD gsapp = -1;
/* All GLOBAL memory is merged to one block and allocated in main().
 * All pointers to GLOBAL memory are initialized before any AES call. */
char *gslongname;  /* 32 byte */
GS_INFO *gsi;      /* 16 byte */
char *gsanswer;    /* HW_PATH_MAX byte */


static const char *
nextToken(const char *pcmd)
{
	if (!pcmd)
		return (NULL);

	pcmd += (strlen(pcmd) + 1);

	while (TRUE)
		switch (*pcmd)
		{
			case 0:
				return (NULL);

			case 1:  /* empty parameter */
			case 2:  /* hexadecimal coded parameter */
			case 3:  /* reserved, to ignore */
			case 4:
			case 5:
			case 6:
				pcmd += (strlen(pcmd) + 1);
				break;

			default:
				return (pcmd);
		}
}


/* Returns TRUE if HighWire shall exit. */
static BOOL
doGSCommand(const WORD msg[8])
{
	const char *cmd = *(const char *const *)&msg[3];
	WORD answ[8];
	BOOL quit = FALSE;

	answ[0] = GS_ACK;
	answ[1] = gl_apid;
	answ[2] = 0;
	answ[3] = msg[3];
	answ[4] = msg[4];
	answ[5] = 0;
	answ[6] = 0;
	answ[7] = GSACK_UNKNOWN;

	if (cmd)
	{
		char **pp;

		if (!stricmp(cmd, "AppGetLongName"))
		{
			pp = (char **)&answ[5];
			*pp = strcpy (gslongname, _HIGHWIRE_FULLNAME_);
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "CheckCommand"))
		{
			cmd = nextToken(cmd);
			if (!stricmp(cmd, "AppGetLongName") || !stricmp(cmd, "CheckCommand")
			    || !stricmp(cmd, "Close") || !stricmp(cmd, "GetAllCommands")
			    || !stricmp(cmd, "Open") || !stricmp(cmd, "Quit")) {
				pp = (char **)&answ[5];
				*pp = memcpy(gsanswer, "1\0", 3);
			}
				/* else answ[5..6] is initialized with NULL */
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "GetAllCommands"))
		{
			#define _ALL_1 "AppGetLongName\0CheckCommand\0Close\0"
			#define _ALL_2 "GetAllCommands\0Open <file>\0Quit\0"
			#define ALL   _ALL_1 _ALL_2
			#ifdef __PUREC__
				#if sizeof(ALL) + 1 > HW_PATH_MAX
					#error gsanswer[HW_PATH_MAX] too small for GetAllCommands!
				#endif
			#endif
			pp = (char **)&answ[5];
			*pp = memcpy(gsanswer, ALL, sizeof(ALL));
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "Quit"))
		{
			quit = TRUE;
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "Open"))
		{
			cmd = nextToken(cmd);
			new_hwWind ("", cmd, TRUE);
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "AppGetLongName"))
		{
			strcpy(gslongname, _HIGHWIRE_FULLNAME_);
			pp = (char **)&answ[5];
			*pp = gslongname;
			answ[7] = GSACK_OK;
		}
	}

	appl_write(msg[1], 16, answ);

	return quit;
}


/*----------------------------------------------------------------------------*/
static void
menu_about (void)
{
	form_alert (1, "[1]"
	               "[" _HIGHWIRE_FULLNAME_ " HTML Browser"
	               "|GEM Menu v. 0.4"
	               "|Core     v. " _HIGHWIRE_VERSION_ " " _HIGHWIRE_BETATAG_
	               "|http://highwire.atari-users.net/]"
	               "[  OK  ]");
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
menu_open_handler (OBJECT * form, WORD obj)
{
	if (obj == URL_OK) {
		char * ptext = form[URL_EDIT].ob_spec.tedinfo->te_ptext;
		if (ptext[0]) {
			start_page_load (hwWind_Top->Pane, ptext, NULL, TRUE, NULL);
		}
	} else if (obj == URL_FILE) {
		page_load();
	}
	return TRUE; /* to be deleted */
}

/*============================================================================*/
void
menu_open (BOOL fsel)
{
#ifndef GEM_MENU
	(void)fsel;
	{
		
#else
	if (!fsel) {
		static OBJECT * form = NULL;
/*		short x, y, w, h, n;*/
		char * ptext;
		if (!form) {
			TEDINFO * tedinfo;
			WORD     dmy;
			rsrc_gaddr (R_TREE, URLINPUT, &form);
			if ((ptext = malloc (MAX_LEN *3)) == NULL) {
				form = NULL;
				return;
			}
			tedinfo = form[URL_EDIT].ob_spec.tedinfo;
			tedinfo->te_ptext    = ptext;
			tedinfo->te_ptext[0] = '\0';
			tedinfo->te_ptmplt   = memset (ptext + MAX_LEN *1, '_', MAX_LEN-1);
			tedinfo->te_ptmplt[MAX_LEN-1] = '\0';
			tedinfo->te_pvalid   = memset (ptext + MAX_LEN *2, 'X', MAX_LEN-1);
			tedinfo->te_pvalid[MAX_LEN-1] = '\0';
			tedinfo->te_txtlen   = MAX_LEN;
			form[URL_EDIT].ob_width = form[URL_ED_BG].ob_width
			                        = (MAX_LEN -1) * 8;
			form->ob_spec.obspec.framesize = 0;
			form_center (form, &dmy,&dmy,&dmy,&dmy);
		} else if (!(form->ob_flags & OF_FLAG15)) {
			ptext = form[URL_EDIT].ob_spec.tedinfo->te_ptext;
			form[URL_OK    ].ob_state &= ~OS_SELECTED;
			form[URL_CANCEL].ob_state &= ~OS_SELECTED;
			form[URL_FILE  ].ob_state &= ~OS_SELECTED;
		} else {
			return; /* already open */
		}
		ptext[0] = '\0';
		formwind_do (form, URL_EDIT, "Open Page", TRUE, menu_open_handler);
	
	} else {
#endif
		page_load();
	}
}

/*============================================================================*/
void
menu_info (void)
{
	FRAME    frame = hwWind_ActiveFrame (NULL);
	CONTAINR cont  = (frame ? frame->Container : hwWind_Top->Pane);
	LOCATION loc   = (frame ? frame->Location  : NULL);
	const char * file_ptr = (!loc ? "(empty)" : *loc->File ? loc->File : ".");
	int          file_ln  = (int)strlen (file_ptr);
	const char * file_pp  = "";
	const char * dir_ptr  = (loc ? location_Path (loc, NULL) : fsel_path);
	int          dir_ln   = (int)strlen (dir_ptr);
	const char * dir_pp   = "";
	char       * enc_ptr;
	int          enc_ln;
	char       * frm_ptr  = (cont->Name ? cont->Name : "");
	int          frm_ln   = (int)strlen (frm_ptr);
	char       * frm_pp   = "";
	
	char text[1000];
	
	if (file_ln > 32) {
		file_ln = 31;
		file_pp = "\257";
	}
	if (dir_ln > 40) {
		dir_ln = 39;
		dir_pp = "\257";
	}
	switch (frame ? frame->Encoding : ENCODING_WINDOWS1252) {
		case ENCODING_WINDOWS1252: enc_ptr =   "Windows-1252"; break;
		case ENCODING_ISO8859_2:   enc_ptr =      "ISO8859-2"; break;
		case ENCODING_ISO8859_15:  enc_ptr =     "ISO8859-15"; break;
		case ENCODING_UTF8:        enc_ptr =   "Unicode-UTF8"; break;
		case ENCODING_UTF16:       enc_ptr =  "Unicode-UTF16"; break;
		case ENCODING_MACINTOSH:   enc_ptr = "MacintoshRoman"; break;
		case ENCODING_ATARIST:     enc_ptr =      "AtariFont"; break;
		default:                   enc_ptr =      "<invalid>";
	}
	if (frm_ln > 37 - (enc_ln = (int)strlen (enc_ptr))) {
		frm_ln = 37 - enc_ln;
		frm_pp = "\257";
	} else {
		enc_ln = 37 - frm_ln;
	}
	sprintf (text, "[0][File '%.*s%s':|%.*s%s|'%.*s%s' %*s|][Ok|about:|About..]",
	                file_ln, file_ptr, file_pp,
	                dir_ln,  dir_ptr,  dir_pp,
	                frm_ln,  frm_ptr,  frm_pp,
	                enc_ln,  enc_ptr);
	switch (form_alert (1, text)) {
		case 2:
			new_hwWind ("About", "about:", TRUE);
			break;
		case 3:
			menu_about();
			break;
	}
}

/*============================================================================*/
void
menu_quit (void)
{
	hwWind_store (HWWS_GEOMETRY);
	hwWind_store (HWWS_BOOKMGEO);
	exit (EXIT_SUCCESS);
}


/*============================================================================*/
void
menu_fontsize (char plus_minus)
{
	FRAME frame;
	
	if (plus_minus == '+') {
		if (font_size < 18) {
			font_size++;
		} else {
			font_size += 3;
		}
		logprintf(LOG_BLACK, "+font_size %d\n", (int)font_size);
		frame = hwWind_ActiveFrame (NULL);
		
	} else if (font_size > 2) {
		if (font_size < 20) {
			font_size--;
		} else {
			font_size -= 3;
		}
		if (font_minsize > font_size) {
			font_minsize = font_size; /* override setting */
		}
		logprintf(LOG_BLACK, "-font_size %d\n", (int)font_size);
		frame = hwWind_ActiveFrame (NULL);
	
	} else {
		frame = NULL; /* nothing to do */
	}
	if (frame) {
		start_cont_load (frame->Container, NULL, frame->Location, TRUE, TRUE);
	}
}


/*============================================================================*/
void
menu_reload (ENCODING encoding)
{
	if (encoding > ENCODING_Unknown) {
		/* if an encoding is given (!=ENCODING_Unknown) set bit 7 to instruct
		 * the parser not to switch it.
		 */
		FRAME frame = hwWind_ActiveFrame (NULL);
		if (frame) {
			LOADER ldr = start_cont_load (frame->Container,
			                              NULL, frame->Location, TRUE, TRUE);
			if (ldr) {
				ldr->Encoding = (encoding | 0x80u);
				ldr->MarginW  = frame->Page.Margin.Lft;
				ldr->MarginH  = frame->Page.Margin.Top;
			}
		}
	} else {
		hwWind_history (hwWind_Top, hwWind_Top->HistMenu, TRUE);
	}
}

/*----------------------------------------------------------------------------*/
/* Bookmark dialog                                                            */
/* type : Define what text will displayed                                     */
/*        'BA' add bookmark                                                   */
/*        'BE' edit bookmark                                                  */
/*        'GA' add group                                                      */
/*        'GE' edit group                                                     */

static const char *
bookmark_editor (UWORD type, const char * text,
                 const char * id, const char * url)
{
	WORD title;
	WORD x, y, w, h, n;
	
	static OBJECT * tree = NULL;

	graf_mouse (hwWind_Mshape = ARROW, NULL);
	
	if (!tree) {
		rsrc_gaddr (R_TREE, BKMEDIT, &tree);
		title = tree[BKM_BG_TEXT].ob_head;
		for (n = title; n != BKM_BG_TEXT; n = tree[n].ob_next) {
			tree[n].ob_flags |= OF_HIDETREE;
			tree[n].ob_y     =  0;
		}
		n = tree[BKM_BG_TEXT].ob_height - tree[title].ob_height;
		tree[BKM_BG_TEXT].ob_height -= n;
		tree[BKM_BG_EDIT].ob_y      -= n;
		tree[ROOT].ob_height        -= n;
	}
	
	switch (type) {
		case ((UWORD)'B'<<8)|'A': title = BKM_TXT_B_ADD;  break;
		case ((UWORD)'B'<<8)|'E': title = BKM_TXT_B_EDIT; break;
		case ((UWORD)'G'<<8)|'A': title = BKM_TXT_G_ADD;  break;
		case ((UWORD)'G'<<8)|'E': title = BKM_TXT_G_EDIT; break;
		default:                  title = 0;
	}
	if (title) {
		tree[title].ob_flags &= ~OF_HIDETREE;
	}
	tree[BKM_VISITED].ob_flags |= OF_HIDETREE;
	
	if (url && (n = (WORD)strlen (url)) > 0) {
		char * t = tree[BKM_URL].ob_spec.tedinfo->te_ptext;
		size_t l = tree[BKM_URL].ob_spec.tedinfo->te_txtlen -1;
		if (n <= l) {
			memcpy (t,     url, n);
			memset (t + n, ' ', l - n);
			t[l] = '\0';
		} else {
			memcpy (t,        url, l -3);
			strcpy (t + l -3, "\372\372\257");
		}
		tree[BKM_URL].ob_flags &= ~OF_HIDETREE;
	} else {
		tree[BKM_URL].ob_flags |= OF_HIDETREE;
	}
	if (id && bookmark_id2date (id,tree[BKM_ADDED].ob_spec.tedinfo->te_ptext,
	                               tree[BKM_ADDED].ob_spec.tedinfo->te_txtlen)) {
		char * t = tree[BKM_ADDED].ob_spec.tedinfo->te_ptext;
		size_t l = tree[BKM_ADDED].ob_spec.tedinfo->te_txtlen -1;
		if ((n = (WORD)strlen (t)) < l) {
			memset (t + n, ' ', l - n);
			t[l] = '\0';
		}
		tree[BKM_ADDED].ob_flags &= ~OF_HIDETREE;
	} else {
		tree[BKM_ADDED].ob_flags |= OF_HIDETREE;
	}
	if (id && (n = (WORD)strlen (id)) > 0) {
		char * t = tree[BKM_ID].ob_spec.tedinfo->te_ptext;
		size_t l = tree[BKM_ID].ob_spec.tedinfo->te_txtlen -1;
		memcpy (t, id, min (l, n));
		t += n;
		while (n++ < l) *(t++) = ' ';
		*t = '\0';
		tree[BKM_ID].ob_flags &= ~OF_HIDETREE;
	} else {
		tree[BKM_ID].ob_flags |= OF_HIDETREE;
	}
	if (text && *text) {
		char * t = tree[BKM_EDIT].ob_spec.tedinfo->te_ptext;
		size_t l = tree[BKM_EDIT].ob_spec.tedinfo->te_txtlen -1;
		strncpy (t, text, l);
		text = t;
	} else {
		char * t = tree[BKM_EDIT].ob_spec.tedinfo->te_ptext;
		*t = '\0';
		text = t;
	}
	tree[BKM_OK].ob_state     &= ~OS_SELECTED;
	tree[BKM_CANCEL].ob_state &= ~OS_SELECTED;
	
	wind_update (BEG_MCTRL);
	form_center (tree, &x, &y, &w, &h);
	form_dial   (FMD_START, x,y,w,h, x,y,w,h);
	objc_draw   (tree, ROOT, MAX_DEPTH, x, y, w, h);
	if (form_do (tree, BKM_EDIT) != BKM_OK || !*text) {
		text = NULL;
	}
	form_dial   (FMD_FINISH, x,y,w,h, x,y,w,h);
	wind_update (END_MCTRL);

	if (title) {
		tree[title].ob_flags |= OF_HIDETREE;
	}
	return text;
}

/*============================================================================*/
void
menu_bookmark_url (LOCATION loc, const char * title)
{
	HwWIND bmrk = NULL;
	if (!loc) {
		HwWIND wind = hwWind_Top;
		if (wind && wind->Base.Ident == WIDENT_BMRK) {
			bmrk = wind;
			wind = hwWind_Next (bmrk);
		}
		if (wind) {
			loc   = wind->Location;
			title = wind->Base.Name;
		}
	}
	if (loc) {
		char url[2 * HW_PATH_MAX];
		location_FullName (loc, url, sizeof(url));
		if (!title || !*title) {
			title = bookmark_editor (((UWORD)'B'<<8)|'A', url, NULL, url);
		}
		if (title && add_bookmark (url, title)
		          && (bmrk
		              || (bmrk = (HwWIND)window_byIdent (WIDENT_BMRK)) != NULL)) {
			hwWind_history (bmrk, bmrk->HistMenu, TRUE);
		}
	}
}

/*============================================================================*/
/* Open bookmark window                                                       */

void
menu_openbookmarks (void)
{
	HwWIND wind = (HwWIND)window_byIdent (WIDENT_BMRK);
	if (wind) hwWind_raise (wind, TRUE);
	else      new_hwWind ("", bkm_File, TRUE);
}


/*============================================================================*/
#ifdef GEM_MENU
WORD
menu_history (HISTORY hist[], UWORD used, WORD check)
{
	static char  m_empty[] = "  (empty)";
	static WORD  m_checked = 0;
	static WORD  m_backgnd = 0;
	static UWORD m_max_len;
	static UWORD m_char_wd;
	
	UWORD width = 0;
	WORD  shift = 0;
	WORD  last;
	short i;
	
	if (!m_backgnd) {
		GRECT r;
		m_backgnd = menutree[M_HIST_END].ob_next;
		m_char_wd = menutree[m_backgnd].ob_width
		          / ((UWORD)strlen (menutree[M_HIST_BEG].ob_spec.free_string) +2);
		wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &r);
		objc_offset (menutree, m_backgnd, &r.g_x, &r.g_y);
		m_max_len = (r.g_w - r.g_x) / m_char_wd -1;
		if (m_max_len > sizeof(((HISTORY)0)->Text) -2) {
			 m_max_len = sizeof(((HISTORY)0)->Text) -2;
		}
		for (i = M_HIST_BEG +1; i <= M_HIST_END; i++) {
			menu_ienable (menutree, i, TRUE);
		}
	}
	
	if (m_checked) {
		menu_icheck (menutree, m_checked, FALSE);
	}
	
	wind_update (BEG_MCTRL);
	
	if (!hist || !used) {
		menu_ienable (menutree, M_HIST_BEG, FALSE);
		last = M_HIST_BEG;
		for (i = M_HIST_BEG; i <= M_HIST_END; i++) {
			menu_text (menutree, i, menutree[i].ob_spec.free_string = m_empty);
		}
		width = sizeof (m_empty) -1;
	
	} else {
		menu_ienable (menutree, M_HIST_BEG, TRUE);
		last = M_HIST_BEG -1;
		for (i = 0; i < used; i++) {
			++last;
			if (hist[i]->Length > m_max_len) {
				hist[i]->Text[m_max_len]    = '\257';
				hist[i]->Text[m_max_len +1] = '\0';
				hist[i]->Length = m_max_len;
				width = m_max_len;
			} else if (hist[i]->Length > width) {
				width = hist[i]->Length;
			}
			menu_text (menutree, last,
			           menutree[last].ob_spec.free_string = hist[i]->Text);
			menutree[last].ob_flags &= ~OF_HIDETREE;
		}
		
		if (check >= 0) {
			m_checked = check + M_HIST_BEG;
			menu_icheck (menutree, m_checked, TRUE);
		}
	}
	
	width = (width +2) * m_char_wd;
	menutree[m_backgnd].ob_width  = width;
	menutree[m_backgnd].ob_height = menutree[last].ob_y
	                              + menutree[last].ob_height;
	for (i = M_HIST_BEG; i <= last; i++) {
		menutree[i].ob_width  = width;
	}
	while (last < M_HIST_END) {
		menutree[++last].ob_flags |= OF_HIDETREE;
	}
	
	wind_update (END_MCTRL);
	
	return shift;
}
#endif


/*============================================================================*/
void
menu_cookies (int mode)
{
	if (mode < 0)  cfg_AllowCookies = !cfg_AllowCookies;
	else if (mode) cfg_AllowCookies = TRUE;
	else           cfg_AllowCookies = FALSE;
	if (mode < 0) {
		save_config ("COOKIES", (cfg_AllowCookies ? "1" : "0"));
	}
#ifdef GEM_MENU
	menu_icheck (menutree, M_COOKIES, cfg_AllowCookies);
#endif
}

/*============================================================================*/
void
menu_images (int mode)
{
	if (mode < 0)  cfg_ViewImages = !cfg_ViewImages;
	else if (mode) cfg_ViewImages = TRUE;
	else           cfg_ViewImages = FALSE;
	if (mode < 0) {
		save_config ("VIEW_IMAGES", (cfg_ViewImages ? "1" : "0"));
	}
#ifdef GEM_MENU
	menu_icheck (menutree, M_IMAGES, cfg_ViewImages);
#endif
}

/*============================================================================*/
void
menu_use_css (int mode)
{
	if (mode < 0)  cfg_UseCSS = !cfg_UseCSS;
	else if (mode) cfg_UseCSS = TRUE;
	else           cfg_UseCSS = FALSE;
	if (mode < 0) {
		save_config ("USE_CSS", (cfg_UseCSS ? "1" : "0"));
	}
#ifdef GEM_MENU
	menu_icheck (menutree, M_USE_CSS, cfg_UseCSS);
#endif
}

/*============================================================================*/
void
menu_frm_ctrl (int mode)
{
	if (mode < 0)  mode = !force_frame_controls;
	else if (mode) mode = TRUE;
	else           mode = FALSE;
	force_frame_controls = mode;
#ifdef GEM_MENU
	menu_icheck (menutree, M_FRM_CTRL, force_frame_controls);
#endif
}

/*============================================================================*/
void
menu_logging (int mode)
{
	if (mode < 0)  mode = !logging_is_on;
	else if (mode) mode = TRUE;
	else           mode = FALSE;
	logging_is_on = mode;
#ifdef GEM_MENU
	menu_icheck (menutree, M_LOGGING, logging_is_on);
#endif
}


/*----------------------------------------------------------------------------*/
#ifdef GEM_MENU
static void
handle_menu (WORD title, WORD item, UWORD state)
{
	if (title == M_HISTORY) {
		hwWind_history (hwWind_Top, item - M_HIST_BEG, FALSE);
	
	} else switch (item) {
		case M_ABOUT:     menu_about();                               break;
		case M_OPEN:      menu_open (!(state & (K_RSHIFT|K_LSHIFT))); break;
    case M_NEW:       new_hwWind ("", cfg_StartPage, TRUE);       break;
		case M_INFO:      menu_info();                                break;
		case M_QUIT:      menu_quit();                                break;
		case M_RELOAD:    menu_reload (ENCODING_Unknown);             break;
		case M_OPEN_BOOKMARKS: menu_openbookmarks();           break;
		case M_BOOKMARK_URL:   menu_bookmark_url (NULL, NULL); break;
#if (_HIGHWIRE_ENCMENU_ == 1)
		case M_W1252:     menu_reload (ENCODING_WINDOWS1252); break;
		case M_I8859_2:   menu_reload (ENCODING_ISO8859_2);   break;
		case M_I8859_15:  menu_reload (ENCODING_ISO8859_15);  break;
		case M_UTF8:      menu_reload (ENCODING_UTF8);        break;
		case M_UTF16:     menu_reload (ENCODING_UTF16);       break;
		case M_UTF16LE:   menu_reload (ENCODING_UTF16LE);     break;
		case M_MACINTOSH: menu_reload (ENCODING_MACINTOSH);   break;
		case M_ATARI_SYS: menu_reload (ENCODING_ATARIST);     break;
		case M_ATARINVDI: menu_reload (ENCODING_ATARINVDI);   break;
#endif
		case M_FONT_INC:  menu_fontsize ('+');  break;
		case M_FONT_DEC:  menu_fontsize ('-');  break;
		case M_COOKIES:   menu_cookies  (-1);   break;
		case M_IMAGES:    menu_images   (-1);   break;
		case M_USE_CSS:   menu_use_css  (-1);   break;
		case M_FRM_CTRL:  menu_frm_ctrl (-1);   break;
		case M_LOGGING:   menu_logging  (-1);   break;
		case M_FONTS:     fonts_setup   (NULL); break;
	}
	if (title > 0) {
		menu_tnormal (menutree, title, UNHIGHLIGHT);
	}
}
#endif


/*============================================================================*/
#if (_HIGHWIRE_ENCMENU_ == 1)
void
update_menu (ENCODING encoding, BOOL raw_text)
{
	short item;
	
	for (item = M_W1252; item <= M_ATARINVDI; item++) {
		menu_icheck (menutree, item, UNCHECK);
	}
	switch (encoding & 0x7Fu) {
		case ENCODING_WINDOWS1252: item = M_W1252;     break;
		case ENCODING_ISO8859_2:   item = M_I8859_2;   break;
		case ENCODING_ISO8859_15:  item = M_I8859_15;  break;
		case ENCODING_UTF8:        item = M_UTF8;      break;
		case ENCODING_UTF16:       item = M_UTF16;     break;
		case ENCODING_UTF16LE:     item = M_UTF16LE;   break;
		case ENCODING_MACINTOSH:   item = M_MACINTOSH; break;
		case ENCODING_ATARIST:     item = M_ATARI_SYS; break;
		case ENCODING_ATARINVDI:   item = M_ATARINVDI; break;
		default:                   item = -1;
	}
	if (item > 0) {
		menu_icheck (menutree, item, CHECK);
	}
	menu_ienable (menutree, M_UTF16,   raw_text);
	menu_ienable (menutree, M_UTF16LE, raw_text);
}
#endif


/*----------------------------------------------------------------------------*/
static WORD
rpop_do (OBJECT * rpopup, WORD tree, WORD mx, WORD my)
{
	WORD  which_obj;
	WORD x, y, w, h;
	GRECT desk;
	
	form_center (rpopup, &x, &y, &w, &h);
	x -= rpopup->ob_x;
	x += rpopup->ob_x = mx -2;
	y -= rpopup->ob_y;
	y += rpopup->ob_y = my -2;
	wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &desk);
	if ((desk.g_w += desk.g_x - (x + w)) < 0) {
		rpopup->ob_x += desk.g_w;
		x            += desk.g_w;
	}
	if ((desk.g_x -= x) > 0) {
		rpopup->ob_x += desk.g_x;
		x            += desk.g_x;
	}
	if ((desk.g_h += desk.g_y - (y + h)) < 0) {
		rpopup->ob_y += desk.g_h;
		y            += desk.g_h;
	}
	if ((desk.g_y -= y) > 0) {
		rpopup->ob_y += desk.g_y;
		y            += desk.g_y;
	}
	
	wind_update (BEG_MCTRL);
	form_dial   (FMD_START, x, y, w, h, x, y, w, h);
	objc_draw   (rpopup, ROOT, MAX_DEPTH, x, y, w, h);

	which_obj = HW_form_do (rpopup, 0);
	
	if (which_obj > 0) {
		objc_change (rpopup, which_obj, 0, 0,0,0,0, OS_NORMAL, 0);
	}
	form_dial   (FMD_FINISH, x, y, w, h, x, y, w, h);
	wind_update (END_MCTRL);
	
#	define _(idx, obj)   ((idx <<8) | obj)
	switch (_(tree,which_obj)) {
		case _(RPOPUP,  RPOP_INFO):
		case _(RLINKPOP,RLINK_INFO):
		case _(RIMGPOP, RIMG_INFO):
			menu_info();
			break;
		default:
			return which_obj; /* to be handled by the caller */
	}
#	undef _
	return -1; /* already handled by the switch() statement above */
}

/*----------------------------------------------------------------------------*/
static void
copy_url_2_scrap (LOCATION loc)
{
	FILE * file = open_scrap (FALSE);
	if (file) {
		char buf[2 * HW_PATH_MAX];
		location_FullName (loc, buf, sizeof(buf));
		fwrite (buf, 1, strlen(buf), file);
		fclose (file);
	}
}

/*============================================================================*/
#if defined(GEM_MENU) && (_HIGHWIRE_RPOP_ == 1)
void
rpopup_open (WORD mx, WORD my)
{
	extern OBJECT *rpopup;
	
	HwWIND wind  = hwWind_byCoord (mx, my);
	FRAME  frame = hwWind_ActiveFrame (wind);
	LOCATION loc = (frame ? frame->Location : NULL);

	if (!frame) {   /* empty window, nearly no action possible */
		short i;
		for (i = rpopup->ob_head; i > 0; i = rpopup[i].ob_next) {
			if (rpopup[i].ob_flags & OF_SELECTABLE && i != RPOP_INFO) {
				objc_change (rpopup, i, 0, 0,0,0,0, OS_DISABLED, 0);
			}
		}
	} else {
		short i;
		for (i = rpopup->ob_head; i > 0; i = rpopup[i].ob_next) {
			if (rpopup[i].ob_flags & OF_SELECTABLE) {
				objc_change (rpopup, i, 0, 0,0,0,0, OS_NORMAL, 0);
			}
		}
		if (wind->HistMenu <= 0) {
			objc_change (rpopup, RPOP_BACK,    0, 0,0,0,0, OS_DISABLED, 0);
		}
		if (wind->HistMenu >= wind->HistUsed -1) {
			objc_change (rpopup, RPOP_FORWARD, 0, 0,0,0,0, OS_DISABLED, 0);
		}
		if (loc->Proto != PROT_FILE && !cache_lookup (loc, 0, NULL)) {
			objc_change (rpopup, RPOP_VIEWSRC, 0, 0,0,0,0, OS_DISABLED, 0);
			objc_change (rpopup, RPOP_SAVE,    0, 0,0,0,0, OS_DISABLED, 0);
		}
	}
	
	switch (rpop_do (rpopup, RPOPUP, mx, my)) {
	
		case RPOP_BACK:
			hwWind_undo (wind, FALSE);
			break;
		
		case RPOP_FORWARD:
			hwWind_undo (wind, TRUE);
			break;
		
		case RPOP_RELOAD:
			hwWind_history (wind, wind->HistMenu, TRUE);
			break;
		
		case RPOP_VIEWSRC: {
			char buf[2 * HW_PATH_MAX];
			if (PROTO_isRemote (loc->Proto)) {
				CACHED cached = cache_lookup (loc, 0, NULL);
				if (cached) {
					union { CACHED c; LOCATION l; } u;
					u.c = cache_bound (cached, NULL);
					loc = u.l;
				}
			}
			location_FullName (loc, buf, sizeof(buf));
			launch_viewer (buf);
		}	break;
		
		case RPOP_SAVE: {
			CONTAINR cont = NULL;
			char buf[2 * HW_PATH_MAX];

			location_FullName (loc, buf, sizeof(buf));

			cont = new_hwWind (buf, NULL, TRUE)->Pane;
			if (cont) {
				LOADER ldr = start_objc_load (cont, buf,
				                              frame->BaseHref, saveas_job, NULL);
				if (ldr) {
					ldr->Encoding = frame->Encoding;
				}
			}
		}	break;

		case RPOP_COPY: 
			copy_url_2_scrap (loc);
			break;

		case RPOP_BOOKM: 
			if (!frame->Container->Parent) {
				menu_bookmark_url (NULL, NULL);
			} else {
				menu_bookmark_url (loc, frame->Container->Name);		
			}
			break;
	}
}

/*============================================================================*/
void
rpoplink_open (WORD mx, WORD my, CONTAINR current, void * hash)
{
	extern OBJECT *rpoplink;
	
	CONTAINR          cont = NULL;
	struct url_link * link = hash;
	const char      * addr = link->address;
	
	HwWIND wind  = hwWind_byContainr (current);
	FRAME  frame = hwWind_ActiveFrame (wind);
	LOCATION loc = new_location (addr, frame->BaseHref);
	
	if (loc->Proto != PROT_FILE && loc->Proto != PROT_HTTP) {
		objc_change (rpoplink, RLINK_SAVE, 0, 0,0,0,0, OS_DISABLED, 0);
	} else {
		objc_change (rpoplink, RLINK_SAVE, 0, 0,0,0,0, OS_NORMAL, 0);
	}
	
	switch (rpop_do (rpoplink, RLINKPOP, mx, my)) {
	
		case RLINK_OPEN:
			cont = (!link->u.target || stricmp (link->u.target, "_blank") != 0
			        ? containr_byName (current, link->u.target) : NULL);
			if (cont) {
				const char * p = strchr (addr, '#');
				FRAME  t_frame = containr_Frame (cont);
				if (p && t_frame) {
					const char * fname = t_frame->Location->File;
					if (strncmp (fname, addr, (p - addr)) == 0 && !fname[p - addr]) {
						addr = p;   /* content matches */
					}
				}
			}
			if (*addr == '#') {
				long dx, dy;
				if (containr_Anchor (current, addr, &dx, &dy)) {
					hwWind_scroll (wind, current, dx, dy);
					check_mouse_position (mx, my);
				}
				break;
			
			} else if (cont) {
				LOADER ldr = start_page_load (cont, NULL, loc, TRUE, NULL);
				if (ldr) {
					ldr->Encoding = link->encoding;
				}
				break;
			
			} /* else fall through */
			
		case RLINK_NEW:
			wind = new_hwWind (addr, NULL, TRUE);
			cont = (wind ? wind->Pane : NULL);
			if (cont) {
				LOADER ldr = start_page_load (cont, NULL, loc, TRUE, NULL);
				if (ldr) {
					ldr->Encoding = link->encoding;
				}
			}
			break;
		
		case RLINK_SAVE:
			dl_manager (loc, frame->Location);
			break;

		case RLINK_COPY: 
			copy_url_2_scrap (loc);
			break;

		case RLINK_BOOKM:
			/* we need a method to grab the name of the link */
			menu_bookmark_url (loc, NULL);
			break;
	}
	
	free_location (&loc);
}

/*----------------------------------------------------------------------------*/
static WORDITEM
find_word (FRAME frame, WORD x, WORD y)
{
	LRECT    rect;
	long     px  = (long)x - frame->clip.g_x + frame->h_bar.scroll;
	long     py  = (long)y - frame->clip.g_y + frame->v_bar.scroll;
	DOMBOX * box = dombox_byCoord  (&frame->Page, &rect, &px, &py);
	PARAGRPH par = dombox_Paragrph (box);
	rect.X = px + x - frame->clip.g_x;
	rect.Y = py + y - frame->clip.g_y;
	return paragrph_word (par, -px, -py, (LONG*)&rect);
}

/*============================================================================*/
void
rpopimg_open (WORD mx, WORD my, CONTAINR current)
{
	extern OBJECT *rpopimg;
	
	CONTAINR cont   = NULL;
	HwWIND   wind   = hwWind_byContainr (current);
	FRAME    frame  = hwWind_ActiveFrame (wind);
	WORDITEM word   = find_word (frame, mx, my);
	LOCATION imgloc = word->image->source;
	
	objc_change (rpopimg, RIMG_SAVE,  0, 0,0,0,0, OS_DISABLED, 0);
	objc_change (rpopimg, RIMG_BOOKM, 0, 0,0,0,0, OS_DISABLED, 0);
	objc_change (rpopimg, RIMG_COPY,  0, 0,0,0,0, OS_DISABLED, 0);
	
	switch (rpop_do (rpopimg, RIMGPOP, mx, my)) {
	
		case RIMG_OPEN: {
			start_page_load (current, NULL, imgloc, TRUE, NULL);

			} break;
						
		case RIMG_NEW: {
			char buf[2 * HW_PATH_MAX];

			location_FullName (imgloc, buf, sizeof(buf));

			wind = new_hwWind (buf, NULL, TRUE);
			cont = (wind ? wind->Pane : NULL);
			if (cont)
				start_page_load (cont, NULL, imgloc, TRUE, NULL);
				
			} break;

		case RIMG_SAVEIMG:
			dl_manager (imgloc, frame->Location);
			break;

		case RIMG_RELOAD:
			reload_image(&word->image);
			break;

		case RIMG_COPYIMGURL: 
			copy_url_2_scrap (imgloc);
			break;
	}
	
	objc_change (rpopimg, RIMG_SAVE,  0, 0,0,0,0, OS_NORMAL, 0);
	objc_change (rpopimg, RIMG_BOOKM, 0, 0,0,0,0, OS_NORMAL, 0);
	objc_change (rpopimg, RIMG_COPY,  0, 0,0,0,0, OS_NORMAL, 0);
}

/*============================================================================*/
void
rpopilink_open (WORD mx, WORD my, CONTAINR current, void * hash)
{
	extern OBJECT *rpopimg;
	
	CONTAINR          cont = NULL;
	struct url_link * link = hash;
	const char      * addr = link->address;
	
	HwWIND   wind   = hwWind_byContainr (current);
	FRAME    frame  = hwWind_ActiveFrame (wind);
	LOCATION loc    = new_location (addr, frame->BaseHref);
	WORDITEM word   = (link->start ? link->start : find_word (frame, mx, my));
	LOCATION imgloc = word->image->source;
	
	if (loc->Proto != PROT_FILE && loc->Proto != PROT_HTTP) {
		objc_change (rpopimg, RIMG_SAVE, 0, 0,0,0,0, OS_DISABLED, 0);
	} else {
		objc_change (rpopimg, RIMG_SAVE, 0, 0,0,0,0, OS_NORMAL, 0);
	}
	
	switch (rpop_do (rpopimg, RIMGPOP, mx, my)) {
	
		case RIMG_OPEN:
			cont = (!link->u.target || stricmp (link->u.target, "_blank") != 0
			        ? containr_byName (current, link->u.target) : NULL);
			if (cont) {
				const char * p = strchr (addr, '#');
				FRAME  t_frame = containr_Frame (cont);
				if (p && t_frame) {
					const char * fname = t_frame->Location->File;
					if (strncmp (fname, addr, (p - addr)) == 0 && !fname[p - addr]) {
						addr = p;   /* content matches */
					}
				}
			}
			if (*addr == '#') {
				long dx, dy;
				if (containr_Anchor (current, addr, &dx, &dy)) {
					hwWind_scroll (wind, current, dx, dy);
					check_mouse_position (mx, my);
				}
				break;
			
			} else if (cont) {
				LOADER ldr = start_page_load (cont, NULL, loc, TRUE, NULL);
				if (ldr) {
					ldr->Encoding = link->encoding;
				}
				break;
			
			} /* else fall through */
			
		case RIMG_NEW:
			wind = new_hwWind (addr, NULL, TRUE);
			cont = (wind ? wind->Pane : NULL);
			if (cont) {
				LOADER ldr = start_page_load (cont, NULL, loc, TRUE, NULL);
				if (ldr) {
					ldr->Encoding = link->encoding;
				}
			}
			break;
		
		case RIMG_SAVE:
			dl_manager (loc, frame->Location);
			break;

		case RIMG_COPY: 
			copy_url_2_scrap (loc);
			break;

		case RIMG_SAVEIMG:
			dl_manager (imgloc, frame->Location);
			break;

		case RIMG_COPYIMGURL: 
			copy_url_2_scrap (imgloc);
			break;

		case RIMG_RELOAD:
			reload_image(&word->image);
			break;

		case RIMG_BOOKM: 
			/* we need a method to grab the name of the link */
			menu_bookmark_url (loc, NULL);
			break;
	}
	
	free_location (&loc);
}

/*============================================================================*/
/* Open bookmark popup                                                        */

void
rpopbkm_open (WORD mx, WORD my, DOMBOX * box, WORDITEM word)
{
	extern OBJECT *rpopbkm;
	
	BOOL      reload = FALSE;
	const char * lnk = NULL;
	const char * grp = NULL;
	WORD         i;
	for (i = rpopbkm->ob_head; i > 0; i = rpopbkm[i].ob_next) {
		if (rpopbkm[i].ob_flags & OF_SELECTABLE && i != RBKM_IMPORT) {
			objc_change (rpopbkm, i, 0, 0,0,0,0, OS_DISABLED, 0);
		}
	}
	if (box) {
		if (*box->ClName == 'L') lnk = box->IdName;
		else               /*G*/ grp = box->IdName;
		if (!grp && box->Parent) {
			box = box->Parent;
			grp = (box->ClName ? box->ClName : NULL);
		}
		objc_change (rpopbkm, RBKM_UP,     0, 0,0,0,0, OS_NORMAL, 0);
		objc_change (rpopbkm, RBKM_DN,     0, 0,0,0,0, OS_NORMAL, 0);
		objc_change (rpopbkm, RBKM_EDIT,   0, 0,0,0,0, OS_NORMAL, 0);
		objc_change (rpopbkm, RBKM_REMOVE, 0, 0,0,0,0, OS_NORMAL, 0);
	}
	if (grp) {
		DOMBOX * next = box->Sibling;
		while (next) {
			if (next->HtmlCode == TAG_DL && strcmp (next->ClName, grp) == 0) {
				if (next->Hidden) {
					objc_change (rpopbkm, RBKM_EXPAND,  0, 0,0,0,0, OS_NORMAL, 0);
				} else {
					objc_change (rpopbkm, RBKM_COLLAPS, 0, 0,0,0,0, OS_NORMAL, 0);
				}
				break;
			}
			next = next->Sibling;
		}
	} else {
		objc_change (rpopbkm, RBKM_ADDGRP, 0, 0,0,0,0, OS_NORMAL, 0);
	}
	if (lnk) {
		if (word && word->link) {
			objc_change (rpopbkm, RBKM_COPY, 0, 0,0,0,0, OS_NORMAL, 0);
		}
	}
	i = rpopbkm[RBKM_UP].ob_y + rpopbkm[RBKM_UP].ob_height /2;
	switch (rpop_do (rpopbkm, RBKMPOP, mx +3, my - i)) {
		
		case RBKM_COLLAPS:
			reload = set_bookmark_group (grp, FALSE);
			break;
		
		case RBKM_EXPAND:
			reload = set_bookmark_group (grp, TRUE);
			break;
		
		case RBKM_UP:
			if (lnk) reload = pos_bookmark       (lnk, FALSE);
			else     reload = pos_bookmark_group (grp, FALSE);
			break;
		
		case RBKM_DN:
			if (lnk) reload = pos_bookmark       (lnk, TRUE);
			else     reload = pos_bookmark_group (grp, TRUE);
			break;
		
		case RBKM_REMOVE:
			if(lnk) reload = del_bookmark       (lnk);
			else    reload = del_bookmark_group (grp);
			break;
		
		case RBKM_EDIT: {
			UWORD        type;
			const char * text;
			const char * id, * url;
			char         buff[1024];
			if (lnk) {
				type = ((UWORD)'B'<<8)|'E';
				id   = lnk;
				url  = (word && word->link ? word->link->address : NULL);
			} else {
				type = ((UWORD)'G'<<8)|'E';
				id   = grp;
				url  = NULL;
			}
			txt_bookmark_entry (id, buff, sizeof(buff));
			if ((text = bookmark_editor (type, buff, id, url)) != NULL) {
				reload = txt_bookmark_entry (id, strcpy (buff, text), 0);
			}
		}	break;
		
		case RBKM_ADDGRP: {
			UWORD        type = ((UWORD)'G'<<8)|'A';
			const char * text = bookmark_editor (type, NULL, NULL, NULL);
			if (text)  reload = add_bookmark_group (lnk, text);
		}	break;
		
		case RBKM_COPY: {
			FILE * file = open_scrap (FALSE);
			if (file) {
				char * url = word->link->address;
				fwrite (url, 1, strlen(url), file);
				fclose (file);
			}
		}	break;
		
		case RBKM_IMPORT: {
			char file[HW_PATH_MAX] = "";
			const char * label = "HighWire: Import Links from";
			if (file_selector (label, NULL, NULL, file, sizeof(file)) && *file) {
				reload = pick_bookmarks (file, progress_bar);
			}
		}	break;
	}
	if (reload) {
		HwWIND wind = (HwWIND)window_byIdent (WIDENT_BMRK);
		if (wind) hwWind_history (wind, wind->HistMenu, TRUE);
	}
}
#endif

/*==============================================================================
 * Returns TRUE if HighWire shall exit.
 */
BOOL
process_messages (WORD msg[], PXY mouse, UWORD state)
{
	if (window_evMessage (msg, mouse, state)) {
		if (!hwWind_Top) return TRUE;
	
	} else switch (msg[0]) {
	
		case VA_PROTOSTATUS:
			Receive_AV(msg);
			break;
		case VA_DRAGACCWIND:
			if (cfg_AVWindow) {
/*			printf ("AEI.c - VA_DRAGACCWIND!\r\n"); */
			Receive_AV(msg); /* handle it in av_prot.c */
			}
			break;

		case AV_SENDKEY:
			if (cfg_AVWindow) {
			Receive_AV(msg); /* handle it in av_prot.c */
			}
			break;
		
		case WM_TOPPED: /* 0x15: * probably only an ugly hack */
			if (cfg_AVWindow) {
				window_raise (NULL, TRUE, NULL);
			}
			break;

		case AV_OPENWIND:
			state |= K_ALT;
		case VA_START:
			vastart(msg, mouse, state);
			break;
		
		case GS_REQUEST:
			{
			#if 0  /* an optimized version of the below original */
				const GS_INFO *sender;
				short answ[8], *p_answ = answ;
				long *gsi_ = (long *)gsi;

				*p_answ++              /*[0]*/ = GS_REPLY;
				*p_answ++              /*[1]*/ = gl_apid;
				*p_answ++              /*[2]*/ = 0;
				*((long **)p_answ)     /*[3..4]*/ = gsi_;
				p_answ += 2;
				*gsi_++ = sizeof(GS_INFO);
				*gsi_++ = (0x0120L << 16) | GSM_COMMAND;
				*gsi_   = 0L;
				*((long *)p_answ)      /*[5..6]*/ = 1;
				p_answ += 2;
				*p_answ                /*[7]*/ = msg[7];

				sender = *(GS_INFO **)&msg[3];
				if (sender) {
					if (sender->version >= 0x0070) {
						*--p_answ/*[6]*/ = 0;
						gsapp = msg[1];
					}
				}
			#else
				const GS_INFO *sender = *(GS_INFO **)&msg[3];
				const GS_INFO **pgsi;
				WORD answ[8];

				answ[0] = GS_REPLY;
				answ[1] = gl_apid;
				answ[2] = 0;
				answ[3] = 0;
				answ[4] = 0;
				answ[5] = 0;
				answ[6] = 1;
				answ[7] = msg[7];

				gsi->len = sizeof(GS_INFO);
				gsi->version = 0x0120;
				gsi->msgs = GSM_COMMAND;
				gsi->ext = 0L;

				pgsi = (const GS_INFO **)&answ[3];
				*pgsi = gsi;

				if (sender)
				{
					if (sender->version >= 0x0070)
					{
						answ[6] = 0;
						gsapp = msg[1];
					}
				}
			#endif

				appl_write(gsapp, 16, answ);
			}
			break;
		
		case GS_COMMAND:
			if (doGSCommand(msg))
				return TRUE;
			break;

#ifdef GEM_MENU
		case MN_SELECTED:
			handle_menu (msg[3], msg[4], state);
			break;
#endif
		case AP_DRAGDROP:
			rec_ddmsg (msg);
			break;

		case OLE_NEW:						/* Messages from OLGA	*/
		case OLE_EXIT:
		case OLGA_INIT:
		case OLGA_UPDATED:
			handle_olga ( msg );
		break;

		case 0x7A18/*FONT_CHANGED*/:
			fonts_setup (msg);
			break;
		
		case AP_TERM:
			return (TRUE);
	}
	return (FALSE);
}
