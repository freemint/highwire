/* @(#)highwire/keyinput.c
 *
 * This file should contain the keyboard handler
 * routines.  ie. Input to forms, URL address etc
 *
 * baldrick July 10, 2001
 *============================================================================
 ** Changes
 ** Author         Date           Desription
 ** P Slegg        11-Jan-2010    key_pressed: Utilise NKCC from cflib to handle the various control keys in text fields.
 **
 */
#include <ctype.h>
#include <stdlib.h>

#include <gem.h>
#include <cflib.h>

#include "global.h"
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

/*============================================================================
 ** Changes
 ** Author         Date           Desription
 ** P Slegg        11-Jan-2010    Utilise NKCC from cflib to handle the various control keys in text fields.
 **
 */
void
key_pressed (WORD scan, WORD ascii, UWORD kstate)
{
	FRAME active = hwWind_ActiveFrame (hwWind_Top);
	long  sx = 0, sy = 0;
	WORD     ascii_code;
	UWORD    nkey;
	BOOL     shift, ctrl, alt;
	WORD     key  = (scan << 8) | ascii;

	/* Convert the GEM key code to the "standard" */
	nkey = gem_to_norm ((short)kstate, (short)key);

	/* Remove the unwanted flags */
	nkey &= ~(NKF_RESVD|NKF_SHIFT|NKF_CTRL|NKF_CAPS|NKF_NUM);

	ascii_code =  nkey & 0x00FF;

	shift = (kstate & (K_RSHIFT|K_LSHIFT)) != 0;
	ctrl  = (kstate & K_CTRL) != 0;
	alt   = (kstate & K_ALT) != 0;

	nkey &= ~NKF_FUNC;

	switch (nkey)
	{
	case NK_TAB:  /* 0x0F:  Tab: change active frame */
		if (active && (active = frame_next (active)) != NULL)
		{
			hwWind_setActive (hwWind_Top, active->Container, NULL);
			#if defined(GEM_MENU) && (_HIGHWIRE_ENCMENU_ == 1)
				update_menu (active->Encoding, (active->MimeType == MIME_TXT_PLAIN));
			#endif
		}
		else
		{
			hwWind_setActive (hwWind_Top, NULL, NULL);
		}
		ascii = 0;  /* this key has character HT */
		break;

	case NK_F5:  /* 0x3F:  F5 (Internet Explorer), CTRL+R: reload */
		menu_reload (ENCODING_Unknown);
		break;

	case NK_F7:  /* 0x41:  F7: toggle logging */
		menu_logging (-1);
		break;

	case NK_F8:  /* 0x42:  F8: toggle pictures or alternative text */
		cfg_DropImages = !cfg_DropImages;
		break;

	case NK_CLRHOME: /* 0x47:  home */
		if (active && !(shift))
		{
			sx = -active->Page.Rect.W;
			sy = -active->Page.Rect.H;
			break;
		} /* else fall through */

	case NK_M_END:  /* 0x4F:  end */
		if (active)
		{
			sx = -active->Page.Rect.W;
			sy = +active->Page.Rect.H;
		}
		break;

	case NK_UP:  /* 0x48:  cursor up  /|\ */
		if (!(shift))
		{
			sy = -scroll_step;
			break;
		} /* else fall through */

	case NK_M_PGUP:  /* 0x49:  page up */
		if (active)
			sy = -(active->clip.g_h - scroll_step);
		break;

	case NK_DOWN:  /* 0x50: cursor down  \|/ */
		ascii = 0;  /* this key has character '2' */
		if (!(shift))
		{
			sy = +scroll_step;
			break;
		} /* else fall through */

	case NK_M_PGDOWN:  /* 0x51:  page down */
		if (active)
			sy = +(active->clip.g_h - scroll_step);
		break;

	case NK_LEFT:  /* 0x4B: cursor left <- */
		if (active)
		{
			sx = -(shift  ? active->clip.g_w - scroll_step : scroll_step);
		}
		break;

	case NK_RIGHT:  /* 0x4D: cursor right -> */
		if (active)
		{
			sx = +(shift  ? active->clip.g_w - scroll_step : scroll_step);
		}
		break;

	case NK_UNDO:  /* 0x61:  Undo */
		hwWind_undo (hwWind_Top, (shift));
		break;

	case NK_F1:    /* 0x3B:  F1 (defined in DIN 2137-6, Nr 6.2.4 (ISO/IEC 9995-6?)) */
	case NK_HELP:  /* 0x62:  Help */
		start_cont_load ((active ? containr_Base (active) : hwWind_Top->Pane),
		                 help_file, NULL, TRUE, TRUE);
		break;

	case '+':  /* +: increase font size and reload */
	case '=':  /* allow the unshifted += key as well as the numeric keypad + key */
		if (ctrl)  menu_fontsize ('+');
		break;

	case '-':  /* -: decrease font size and reload */
		if (ctrl)  menu_fontsize ('-');
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
		if (ctrl)  /* 0x0006: CTRL+F */
		  fonts_setup (NULL);
		else
			menu_reload (ENCODING_ISO8859_15);
		break;

		break;

	case 'M':  /* M: reload with default encoding Apple Macintosh Roman */
		menu_reload (ENCODING_MACINTOSH);
		break;

	case 'U':
		if (ctrl)  /* 0x0015:  CTRL+U */
		{
			if (cfg_AVWindow)
				send_avwinclose((&(hwWind_Top)->Base)->Handle);

			delete_hwWind (hwWind_Top);
			if (hwWind_Top)
			{
				WORD mx, my, u;
				graf_mkstate (&mx, &my, &u,&u);
				check_mouse_position (mx, my);
			}
			else
				exit (EXIT_SUCCESS);
		}
		else  /* U: reload with default encoding UTF-8 */
			menu_reload (ENCODING_UTF8);
		break;

	case 'B':  /* 0x0002: CTRL+B, Open Bookmarks */
		if (ctrl)  menu_openbookmarks();
		break;

	case 'D':  /* 0x0004: CTRL+D, Add page to bookmarks */
		if (ctrl)  menu_bookmark_url (NULL, NULL);
		break;

	case 'R':  /* 0x0012:  CTRL+R, F5 (Internet Explorer): reload */
		if (ctrl)  menu_reload (ENCODING_Unknown);
		break;

	case 'O':  /* 0x000F:  CTRL+O */
		if (ctrl)  menu_open (!(shift));
		break;

	case 'I':  /* 0x0009:  CTRL+I */
		if (ctrl)  menu_info();
		break;

	case 'N':
		if (ctrl)  /* 0x000E:  CTRL+N, Open new window on home page */
		{
			if (shift)  /* SHIFT+CTRL+N Open new window on blank page */
				new_hwWind ("HighWire", NULL, TRUE);
			else
				new_hwWind ("", cfg_StartPage, TRUE);
		}
		break;

	}  /* switch */

	if (sx || sy)
	{
		hwWind_scroll (hwWind_Top, active->Container, sx, sy);
	}


}
