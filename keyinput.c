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
#include "Loader.h"
#include "Containr.h"
#include "Location.h"
#include "Logging.h"
#include "hwWind.h"

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

void
key_pressed (WORD key, UWORD state)
{
	FRAME active = hwWind_ActiveFrame (hwWind_Top);
	
	if (!hwWind_keybrd (key, state)) {
		return;
	}
	
	switch (key & 0xFF00)  /* scan code */
	{
	case 0x0F00:  /* Tab: change active frame */
		if ((active = frame_next (active)) != NULL) {
			hwWind_setActive (hwWind_Top, active->Container);
		#if defined(GEM_MENU) && (_HIGHWIRE_ENCMENU_ == 1)
			update_menu (active->Encoding, (active->MimeType == MIME_TXT_PLAIN));
		#endif
		} else {
			hwWind_setActive (hwWind_Top, NULL);
		}
		key = 0;  /* this key has character HT */
		break;
	case 0x3F00:  /* F5 (Internet Explorer), CTRL+R: reload */
		menu_reload (ENCODING_Unknown);
		break;
	case 0x4100:  /* F7: toggle logging */
		menu_logging();
		break;
	case 0x4200:  /* F8: toggle pictures or alternative text */
		menu_alt_text();
		break;
	case 0x4700:  /* home */
		if (!(state & (K_RSHIFT|K_LSHIFT))) {
			hwWind_scroll (hwWind_Top, active->Container,
			               -active->Page.Width, -active->Page.Height);
			break;
		}
		/* else fall through */
	case 0x4F00:  /* end */
		hwWind_scroll (hwWind_Top, active->Container,
		               -active->Page.Width, +active->Page.Height);
		break;
	case 0x4800:  /* /|\ */
		if (!(state & (K_RSHIFT|K_LSHIFT))) {
			hwWind_scroll (hwWind_Top, active->Container,
			               0, -scroll_step);
			break;
		}
		/* else fall through */
	case 0x4900:  /* page up */
		hwWind_scroll (hwWind_Top, active->Container,
		               0, -(active->clip.g_h - scroll_step));
		break;
	case 0x5000:  /* \|/ */
		key = 0;  /* this key has character '2' */
		if (!(state & (K_RSHIFT|K_LSHIFT))) {
			hwWind_scroll (hwWind_Top, active->Container,
			               0, +scroll_step);
			break;
		}
		/* else fall through */
	case 0x5100:  /* page down */
		hwWind_scroll (hwWind_Top, active->Container,
		               0, +(active->clip.g_h - scroll_step));
		break;
	case 0x4B00:  /* <- */
		hwWind_scroll (hwWind_Top, active->Container,
		               -(state & (K_RSHIFT|K_LSHIFT)
		                 ? active->clip.g_w - scroll_step : scroll_step), 0);
		break;
	case 0x4D00:  /* -> */
		hwWind_scroll (hwWind_Top, active->Container,
		               +(state & (K_RSHIFT|K_LSHIFT)
		                 ? active->clip.g_w - scroll_step : scroll_step), 0);
		break;
	case 0x6100:  /* Undo */
		hwWind_undo (hwWind_Top, (state & (K_RSHIFT|K_LSHIFT)));
		break;
	case 0x3B00:  /* F1 (defined in DIN 2137-6, Nr 6.2.4 (ISO/IEC 9995-6?)) */
	case 0x6200:  /* Help */
		start_cont_load (containr_Base (active), help_file, NULL, TRUE);
		break;
	}

	switch (toupper(key & 0xFF))  /* character */
	{
	case '+':  /* +: increase font size and reload */
	case '-':  /* -: decrease font size and reload */
		menu_fontsize (key & 0xFF);
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
	case 0x0012:  /* CTRL+R, F5 (Internet Explorer): reload */
		menu_reload (ENCODING_Unknown);
		break;
	case 0x000E:  /* CTRL+N */
		new_hwWind ("HighWire", NULL, NULL);
		break;
	case 0x000F:  /* CTRL+O */
		menu_open (!(state & (K_RSHIFT|K_LSHIFT)));
		break;
	case 0x0009:  /* CTRL+I */
		menu_info();
		break;
	case 0x0011:  /* CTRL+Q */
		menu_quit();
		break;
	case 0x0017:{ /* CTRL+W */
		WORD   mx, my, u;
		HwWIND wind = hwWind_Top;
		while (wind->Next) wind = wind->Next;
		hwWind_raise (wind, TRUE);
		graf_mkstate (&mx, &my, &u,&u);
		check_mouse_position (mx, my);
	}	break;
	case 0x0015:  /* CTRL+U */
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
