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

#ifdef __PUREC__
# define CONTAINR struct s_containr *
# define HISTORY  struct s_history *
#endif

#include "av_comm.h"
#include "global.h"
#include "Location.h"
#include "Containr.h"
#include "hwWind.h"
#include "Loader.h"
#include "Logging.h"
#include "hwWind.h"
#ifdef GEM_MENU
# include "highwire.h"
#endif


#if (__GEMLIB_MINOR__<42)||((__GEMLIB_MINOR__==42)&&(__GEMLIB_REVISION__<2))
/* Checks if appl_getinfo() exists, and, if possible, calls it.
 * WINX has appl_getinfo() since 2.2, appl_find("?AGI    ") since 2.3c beta.
 */
WORD appl_xgetinfo(WORD type, WORD *out1, WORD *out2, WORD *out3, WORD *out4)
{
	static BOOL has_agi = -1;
	short u;

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


#ifdef GEM_MENU
extern/*static*/ OBJECT * menutree;
#endif


/*------------------------------------------------------------------------------
 * rewrote by Rainer  May 2002 
*/
static void
vastart (const WORD msg[8], PXY mouse, UWORD state)
{
	short answ[8];
	char filename[HW_PATH_MAX];
	const char *cmd = *(const char **)&msg[3];
	/* if a parameter contains spaces then
	 *    it is enclosed with '...', and a ' is encoded as '' */
	const BOOL quoted = cmd[0] == '\'' && cmd[1] != '\0'
	                    && strrchr(&cmd[2], '\'') != NULL && strrchr(&cmd[2], '\'')[1] == '\0';
	
	HwWIND wind = (state && K_ALT ? NULL : hwWind_byCoord (mouse.p_x, mouse.p_y));
	
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
		new_loader_job (cmd, NULL, wind->Pane);
	} else {
		new_hwWind ("", cmd, NULL);
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
	const char *cmd = *(const char **)&msg[3];
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
		if (!stricmp(cmd, "AppGetLongName"))
		{
			*(char **)&answ[5] = strcpy (gslongname, _HIGHWIRE_FULLNAME_);
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "CheckCommand"))
		{
			cmd = nextToken(cmd);
			if (!stricmp(cmd, "AppGetLongName") || !stricmp(cmd, "CheckCommand")
			    || !stricmp(cmd, "Close") || !stricmp(cmd, "GetAllCommands")
			    || !stricmp(cmd, "Open") || !stricmp(cmd, "Quit")) {
				*(char **)&answ[5] = memcpy(gsanswer, "1\0", 3);
			}
				/* else answ[5..6] is initialized with NULL */
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "GetAllCommands"))
		{
			#define ALL "AppGetLongName\0CheckCommand\0Close\0GetAllCommands\0Open <file>\0Quit\0"
			#ifdef __PUREC__
				#if sizeof(ALL) + 1 > HW_PATH_MAX
					#error gsanswer[HW_PATH_MAX] too small for GetAllCommands!
				#endif
			#endif
			*(char **)&answ[5] = memcpy(gsanswer, ALL, sizeof(ALL) + 1);
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
			new_hwWind ("", cmd, NULL);
			answ[7] = GSACK_OK;
		}
		else if (!stricmp(cmd, "AppGetLongName"))
		{
			strcpy(gslongname, _HIGHWIRE_FULLNAME_);
			*(char **)&answ[5] = gslongname;
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
	               "|Core     v. " _HIGHWIRE_VERSION_ " (" _HIGHWIRE_BETATAG_ ")"
	               "|http://highwire.atari-users.net/]"
	               "[  OK  ]");
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
		short x, y, w, h, n;
		char ptext[MAX_LEN] = "", ptmplt[MAX_LEN], pvalid[MAX_LEN];
		if (!form) {
			rsrc_gaddr (R_TREE, URLINPUT, &form);
			form[URL_EDIT].ob_spec.tedinfo->te_txtlen = MAX_LEN;
			form[URL_EDIT].ob_width  = form[URL_ED_BG].ob_width
			                         = (MAX_LEN -1) * 8;
		} else {
			form[URL_OK    ].ob_state &= ~OS_SELECTED;
			form[URL_CANCEL].ob_state &= ~OS_SELECTED;
			form[URL_FILE  ].ob_state &= ~OS_SELECTED;
		}
		((char*)memset(ptmplt, '_', MAX_LEN-1))[MAX_LEN-1] = '\0';
		((char*)memset(pvalid, 'X', MAX_LEN-1))[MAX_LEN-1] = '\0';
		form[URL_EDIT].ob_spec.tedinfo->te_ptext  = ptext;
		form[URL_EDIT].ob_spec.tedinfo->te_ptmplt = ptmplt;
		form[URL_EDIT].ob_spec.tedinfo->te_pvalid = pvalid;
		form_center (form, &x, &y, &w, &h);
		form_dial   (FMD_START, x, y, w, h, x, y, w, h);
		objc_draw   (form, ROOT, MAX_DEPTH, x, y, w, h);
		n = form_do (form, URL_EDIT);
		form_dial   (FMD_FINISH, x, y, w, h, x, y, w, h);
		if (n == URL_OK && ptext[0]) {
			new_loader_job (ptext, NULL, hwWind_Top->Pane);
		} else if (n == URL_FILE) {
			fsel = TRUE;
		}
	}
	if (fsel) {
#endif
		page_load();
	}
}


/*============================================================================*/
void
menu_reload (ENCODING encoding)
{
	if (encoding > ENCODING_Unknown) {
		FRAME  frame  = hwWind_ActiveFrame (NULL);
		LOADER loader = new_loader_job (NULL, frame->Location, frame->Container);
		
		/* if an encoding is given (!=ENCODING_Unknown) set bit 7 to instruct
		 * the parser not to switch it.
		 */
		loader_setParams (loader, (encoding | 0x80u),
		                  frame->Page.MarginLft, frame->Page.MarginTop);
	} else {
		hwWind_history (hwWind_Top, hwWind_Top->HistMenu);
	}
}


/*============================================================================*/
void
menu_info (void)
{
	FRAME    frame = hwWind_ActiveFrame (NULL);
	CONTAINR cont  = frame->Container;
	LOCATION loc   = frame->Location;
	const char * file_ptr = (*loc->File ? loc->File : ".");
	int          file_ln  = (int)strlen (file_ptr);
	const char * file_pp  = "";
	UWORD        dir_ln;
	const char * dir_ptr  = location_Path (loc, &dir_ln);
	const char * dir_pp   = "";
	char       * enc_ptr;
	int          enc_ln;
	char       * frm_ptr  = (cont->Name ? cont->Name : "");
	int          frm_ln   = (int)strlen (frm_ptr);
	char       * frm_pp   = "";
	
	char text[1000];
	
	if (file_ln > 32) {
		file_ln = 31;
		file_pp = "¯";
	}
	if (dir_ln > 40) {
		dir_ln = 39;
		dir_pp = "¯";
	}
	switch (frame->Encoding) {
		case ENCODING_WINDOWS1252: enc_ptr =   "Windows-1252"; break;
		case ENCODING_ISO8859_2:   enc_ptr =      "ISO8859-2"; break;
		case ENCODING_ISO8859_15:  enc_ptr =     "ISO8859-15"; break;
		case ENCODING_UTF8:        enc_ptr =   "Unicode-UTF8"; break;
		case ENCODING_UTF16:       enc_ptr =  "Unicode-UTF16"; break;
		case ENCODING_MACINTOSH:   enc_ptr = "MacintoshRoman"; break;
		case ENCODING_ATARIST:     enc_ptr =      "AtariFont"; break;
		default:                   enc_ptr =      "<infalid>";
	}
	if (frm_ln > 37 - (enc_ln = (int)strlen (enc_ptr))) {
		frm_ln = 37 - enc_ln;
		frm_pp = "¯";
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
			new_hwWind ("About", "about:", NULL);
			break;
		case 3:
			menu_about();
			break;
	}
}


/*============================================================================*/
void
menu_fontsize (char plus_minus)
{
	if (plus_minus == '+') {
		if (font_size < 18)
			font_size++;
		else
			font_size += 3;
		logprintf(LOG_BLACK, "+font_size %d\n", (int)font_size);
		menu_reload (ENCODING_Unknown);
		
	} else if (font_size > 2) {
		if (font_size < 20)
			font_size--;
		else
			font_size -= 3;
		logprintf(LOG_BLACK, "-font_size %d\n", (int)font_size);
		menu_reload (ENCODING_Unknown);
	}
}


/*============================================================================*/
void
menu_logging (void)
{
	logging_is_on = !logging_is_on;
#ifdef GEM_MENU
	menu_icheck(menutree, M_LOGGING, logging_is_on);
#endif
}


/*============================================================================*/
void
menu_alt_text (void)
{
	alternative_text_is_on = !alternative_text_is_on;
#ifdef GEM_MENU
	menu_icheck(menutree, M_ALT_TEXT, alternative_text_is_on);
#endif
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
				hist[i]->Text[m_max_len]    = '¯';
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


/*----------------------------------------------------------------------------*/
#ifdef GEM_MENU
static void
handle_menu (WORD title, WORD item, UWORD state)
{
	if (title == M_HISTORY) {
		hwWind_history (hwWind_Top, item - M_HIST_BEG);
	
	} else switch (item) {
		case M_ABOUT:     menu_about(); break;
		case M_OPEN:      menu_open (!(state & (K_RSHIFT|K_LSHIFT))); break;
		case M_INFO:      menu_info();  break;
		case M_QUIT:      menu_quit();  break;
		case M_RELOAD:    menu_reload (ENCODING_Unknown);     break;
#if (_HIGHWIRE_ENCMENU_==TRUE)
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
		case M_FONT_INC:  menu_fontsize ('+'); break;
		case M_FONT_DEC:  menu_fontsize ('-'); break;
		case M_ALT_TEXT:  menu_alt_text();     break;
		case M_LOGGING:   menu_logging();      break;
	}
	if (title > 0) {
		menu_tnormal (menutree, title, UNHIGHLIGHT);
	}
}


/*============================================================================*/
#if (_HIGHWIRE_ENCMENU_==TRUE)
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

#endif


/*============================================================================*/
/* rpopup_open
 * 
 * I would much rather do this in a non modal manner, however
 * at the moment we haven't seperated the parsing/display from
 * the UI to a satisfying level.  So we have this cheap routine
 *
 * also there are several bits of experimentation going on in here
 *
 * baldrick August 9, 2002
 */
#ifdef GEM_MENU
#if (_HIGHWIRE_RPOP_==TRUE)
void
rpopup_open (WORD mx, WORD my)
{
	extern OBJECT *rpopup;
	
	HwWIND wind  = hwWind_byCoord (mx, my);
	FRAME  frame = hwWind_ActiveFrame (wind);
	short x, y, w, h, which_obj;
	
	if (!wind->HistUsed) {
		objc_change (rpopup, RPOP_BACK,    0, 0,0,0,0, OS_DISABLED, 0);
		objc_change (rpopup, RPOP_FORWARD, 0, 0,0,0,0, OS_DISABLED, 0);
	} else {
		UWORD u = wind->HistUsed -1;
		objc_change (rpopup, RPOP_BACK,    0, 0,0,0,0,
		             (wind->HistMenu > 0 ? OS_NORMAL : OS_DISABLED), 0);
		objc_change (rpopup, RPOP_FORWARD, 0, 0,0,0,0,
		             (wind->HistMenu < u ? OS_NORMAL : OS_DISABLED), 0);
	}
	
	form_center (rpopup, &x, &y, &w, &h);
	x -= rpopup->ob_x;
	x += rpopup->ob_x = mx -2;
	y -= rpopup->ob_y;
	y += rpopup->ob_y = my -2;
	
	wind_update (BEG_MCTRL);
	form_dial   (FMD_START, x, y, w, h, x, y, w, h);
	objc_draw   (rpopup, ROOT, MAX_DEPTH, x, y, w, h);

	which_obj = HW_form_do (rpopup, 0);
/*	which_obj = form_do (rpopup, 0);*/
/*	which_obj = form_popup (rpopup, 0,0);*/
	
	if (which_obj > 0) {
		objc_change (rpopup, which_obj, 0, 0,0,0,0, OS_NORMAL, 0);
	}
	form_dial   (FMD_FINISH, x, y, w, h, x, y, w, h);
	wind_update (END_MCTRL);
	
	switch(which_obj)
	{
		case RPOP_BACK:
			hwWind_undo (wind, FALSE);
			break;
		case RPOP_FORWARD:
			hwWind_undo (wind, TRUE);
			break;
		case RPOP_RELOAD:
			hwWind_history (wind, wind->HistMenu);
			break;
		case RPOP_VIEWSRC:
			launch_viewer(frame->Location->FullName);
			break;
		case RPOP_INFO:
			menu_info();
			break;
	}
}
#endif
#endif

/*==============================================================================
 * Returns TRUE if HighWire shall exit.
 */
BOOL
process_messages (WORD msg[], PXY mouse, UWORD state)
{
	switch (msg[0])
	{
		case AV_PROTOKOLL:
		case VA_PROTOSTATUS:
			Receive_AV(msg);
			break;
		case AV_SENDKEY:
			key_pressed(msg[4], msg[3]);
			break;
		case AV_OPENWIND:
			state |= K_ALT;
		case VA_START:
			vastart(msg, mouse, state);
			break;
		
		case GS_REQUEST:
			{
			#if 1  /* an optimized version of the below original */
				const GS_INFO *sender;
				short answ[8], *p_answ = answ;
				long *gsi_ = (long *)gsi;

				*p_answ++              /*[0]*/ = GS_REPLY;
				*p_answ++              /*[1]*/ = gl_apid;
				*p_answ++              /*[2]*/ = 0;
				*((long **)p_answ)++/*[3..4]*/ = gsi_;
				*gsi_++ = sizeof(GS_INFO);
				*gsi_++ = (0x0120L << 16) | GSM_COMMAND;
				*gsi_   = 0L;
				*((long *)p_answ)++ /*[5..6]*/ = 1;
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

				*(const GS_INFO **)&answ[3] = gsi;

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
		{
			#define DD_NAK 1
			static char pipename[] = "U:\\pipe\\DRAGDROP.AA";
			long fd;

			pipename[18] = msg[7] & 0x00FF;
			pipename[17] = (msg[7] & 0xFF00) >> 8;

			fd = Fopen(pipename, 2);
			if (fd >= 0) {
				char c = DD_NAK;

				Fwrite((short)fd, 1, &c);
				Fclose((short)fd);
			}
			break;
		}
		
		case AP_TERM:
			return (TRUE);
		
		default: {
			HwWIND wind  = hwWind_byHandle (msg[3]);
			BOOL   check = FALSE;
			if (wind) switch (msg[0]) {
				case WM_REDRAW:    hwWind_redraw  (wind, (GRECT*)(msg + 4)); break;
				case WM_MOVED:     hwWind_move    (wind,  *(PXY*)(msg + 4)); break;
				case WM_SIZED:     hwWind_resize  (wind, (GRECT*)(msg + 4)); break;
				case WM_FULLED:    hwWind_full    (wind);                    break;
				case WM_ICONIFY:
					hwWind_iconify (wind, (GRECT*)(msg + 4));
					check = TRUE;
					break;
				case WM_UNICONIFY:
					hwWind_iconify (wind, NULL);
					check = TRUE;
					break;
				case WM_TOPPED:
					hwWind_raise (wind, TRUE);
					check = TRUE;
					break;
				case WM_BOTTOMED:
					hwWind_raise (wind, FALSE);
					check = TRUE;
					break;
				case WM_ARROWED:
					switch(msg[4])
					{
						case WA_LFLINE:
							hwWind_undo (wind, ((state & (K_RSHIFT|K_LSHIFT)) != 0));
							break;
					}
					break;
				case WM_CLOSED:
					delete_hwWind (wind);
					check = TRUE;
					if (!hwWind_Top) return TRUE;
			}
			if (check) {
				check_mouse_position (mouse.p_x, mouse.p_y);
			}
		}
	}
	return (FALSE);
}
