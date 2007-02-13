/* @(#)highwire/keyinput.c
 *
 * This file should contain the keyboard handler
 * routines.  ie. Input to forms, URL address etc
 *
 * baldrick July 10, 2001
 */
#include <ctype.h>
#include <stdlib.h>

#include <gem.h>

#include "global.h"
#include "bookmark.h"
#include "Loader.h"
#include "Containr.h"
#include "Location.h"
#include "Logging.h"
#include "hwWind.h"


/*----------------------------------------------------------------------------*/
static FRAME
frame_next (FRAME frame)
{
	CONTAINR cont = (frame ? frame->Container : NULL);
	BOOL     b    = FALSE;

	while (cont) {
		if (b) {
			if (cont->Mode == CNT_FRAME) {
				if (cont->u.Frame) {
					return cont->u.Frame;
				}
			} else if (cont->Mode) {
				cont = cont->u.Child;
				continue;
			}
		}
		if (cont->Sibling) {
			cont = cont->Sibling;
			b    = TRUE;
			continue;
		}
		cont = cont->Parent;
		b    = FALSE;
	}
	/* After the last frame start from the first. */
	cont = containr_Base(frame);
	while (cont) {
		if (cont->Mode == CNT_FRAME) {
			if (cont->u.Frame) {
				return cont->u.Frame;
			}
		} else if (cont->Mode) {
			cont = cont->u.Child;
		}
	}
	/* If nothing found return the same frame. */
	return frame;
}

/*============================================================================*/
void
key_pressed (WORD scan, WORD ascii, UWORD state)
{
	FRAME active = hwWind_ActiveFrame (hwWind_Top);
	long  sx = 0, sy = 0;
	char buf[2 * HW_PATH_MAX];
	
	switch (scan) {
	
	case 0x0F:  /* Tab: change active frame */
		if (active && (active = frame_next (active)) != NULL) {
			hwWind_setActive (hwWind_Top, active->Container, NULL);
		#if defined(GEM_MENU) && (_HIGHWIRE_ENCMENU_ == 1)
			update_menu (active->Encoding, (active->MimeType == MIME_TXT_PLAIN));
		#endif
		} else {
			hwWind_setActive (hwWind_Top, NULL, NULL);
		}
		ascii = 0;  /* this key has character HT */
		break;
	case 0x3F:  /* F5 (Internet Explorer), CTRL+R: reload */
		menu_reload (ENCODING_Unknown);
		break;
	case 0x41:  /* F7: toggle logging */
		menu_logging (-1);
		break;
	case 0x42:  /* F8: toggle pictures or alternative text */
		cfg_DropImages = !cfg_DropImages;
		break;
	case 0x47:  /* home */
		if (active && !(state & (K_RSHIFT|K_LSHIFT))) {
			sx = -active->Page.Rect.W;
			sy = -active->Page.Rect.H;
			break;
		} /* else fall through */
	case 0x4F:  /* end */
		if (active) {
			sx = -active->Page.Rect.W;
			sy = +active->Page.Rect.H;
		}
		break;
	case 0x48:  /* /|\ */
		if (!(state & (K_RSHIFT|K_LSHIFT))) {
			sy = -scroll_step;
			break;
		} /* else fall through */
	case 0x49:  /* page up */
		if (active) {
			sy = -(active->clip.g_h - scroll_step);
		}
		break;
	case 0x50:  /* \|/ */
		ascii = 0;  /* this key has character '2' */
		if (!(state & (K_RSHIFT|K_LSHIFT))) {
			sy = +scroll_step;
			break;
		} /* else fall through */
	case 0x51:  /* page down */
		if (active) {
			sy = +(active->clip.g_h - scroll_step);
		}
		break;
	case 0x4B:  /* <- */
		if (active) {
			sx = -(state & (K_RSHIFT|K_LSHIFT)
			       ? active->clip.g_w - scroll_step : scroll_step);
		}
		break;
	case 0x4D:  /* -> */
		if (active) {
			sx = +(state & (K_RSHIFT|K_LSHIFT)
			       ? active->clip.g_w - scroll_step : scroll_step);
		}
		break;
	case 0x61:  /* Undo */
		hwWind_undo (hwWind_Top, (state & (K_RSHIFT|K_LSHIFT)));
		break;
	case 0x3B:  /* F1 (defined in DIN 2137-6, Nr 6.2.4 (ISO/IEC 9995-6?)) */
	case 0x62:  /* Help */
		start_cont_load ((active ? containr_Base (active) : hwWind_Top->Pane),
		                 help_file, NULL, TRUE, TRUE);
		break;
	}
	if (sx || sy) {
		hwWind_scroll (hwWind_Top, active->Container, sx, sy);
	}

	switch (toupper (ascii)) {
	
	case '+':  /* +: increase font size and reload */
	case '-':  /* -: decrease font size and reload */
		menu_fontsize (ascii);
		break;
	case '1':  /* 1: reload with default encoding windows-1252 */
		menu_reload (ENCODING_WINDOWS1252);
		break;
	case '2':  /* 2: reload with default encoding ISO-8859-1 */
		menu_reload (ENCODING_ISO8859_2);
		break;
	case 'A':  /* A: reload with default encoding ISO-8859-2 */
		menu_reload (ENCODING_ATARIST);
		break;
	case 'F':  /* F: reload with default encoding ISO-8859-15 */
		menu_reload (ENCODING_ISO8859_15);
		break;
	case 'M':  /* M: reload with default encoding Apple Macintosh Roman */
		menu_reload (ENCODING_MACINTOSH);
		break;
	case 'U':  /* U: reload with default encoding UTF-8 */
		menu_reload (ENCODING_UTF8);
		break;
	case 0x0002: { /* CTRL+B, Open Bookmarks */
		HwWIND wind = (HwWIND)window_byIdent (WINDOW_IDENT('B','M','R','K'));
		if (wind) hwWind_raise (wind, TRUE);
		else      new_hwWind ("", bkm_File);
	}	break;
	case 0x0004: { /* CTRL+D, Add page to bookmarks */
		HwWIND wind = (HwWIND)window_byIdent (WINDOW_IDENT('B','M','R','K'));
		location_FullName (active->Location, buf, sizeof(buf));
		add_bookmark (buf,hwWind_Top->Base.Name);
		if (wind) hwWind_history (wind, wind->HistMenu, TRUE);
	}	break;
	case 0x0006:  /* CTRL+F */
		fonts_setup (NULL);
		break;
	case 0x0012:  /* CTRL+R, F5 (Internet Explorer): reload */
		menu_reload (ENCODING_Unknown);
		break;
	case 0x000F:  /* CTRL+O */
		menu_open (!(state & (K_RSHIFT|K_LSHIFT)));
		break;
	case 0x0009:  /* CTRL+I */
		menu_info();
		break;
	case 0x000E:  /* CTRL+N */
		if (state & (K_RSHIFT|K_LSHIFT)) {
			new_hwWind ("HighWire", NULL);
		} else {
			new_hwWind ("", cfg_StartPage);
		}
		break;
	case 0x0015:  /* CTRL+U */
		if (cfg_AVWindow) {
			send_avwinclose((&(hwWind_Top)->Base)->Handle);
		}
		delete_hwWind (hwWind_Top);
		if (hwWind_Top) {
			WORD mx, my, u;
			graf_mkstate (&mx, &my, &u,&u);
			check_mouse_position (mx, my);
		} else {
			exit (EXIT_SUCCESS);
		}
		break;
	}
}
