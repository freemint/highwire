/* @(#)highwire/Widget.c
 *
 * handles HighWire Widgets and their controls
 * eventually ....
 * baldrick August 9, 2002
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef __PUREC__
#include <tos.h>

#else /* LATTICE || __GNUC__ */
#include <osbind.h>
#endif

#include <gem.h>

#include "global.h"
#include "Containr.h"

#include "Location.h"
#include "Containr.h"
#include "Logging.h"


/*============================================================================*/
WORD
HW_form_do (OBJECT *tree, WORD next)
{
	WORD which = 0;
	BOOL ready = FALSE;
	
	EVMULT_IN in = {
		MU_BUTTON|MU_M1,
		0x102, 3, 0,      /* mouse buttons     */
		MO_ENTER, { 0, }, /* popup enter/leave */
		MO_LEAVE, { 0, }, /* entry leave       */
	};
	in.emi_m1 = *(GRECT*)&tree->ob_x;
	
	(void)next;
	
	do {
		WORD       msg[8];
		EVMULT_OUT out;
		WORD       event = evnt_multi_fast (&in, msg, &out);
		
		if (event & MU_M1) {
			if (!in.emi_m1leave) {        /* entered the popup */
				in.emi_m1leave = MO_LEAVE;
				in.emi_flags |= MU_M2;
			} else if (out.emo_mbutton) { /* left, but still button pressed */
				in.emi_m1leave = MO_ENTER;
				in.emi_flags &= ~MU_M2;
			} else {
				ready = TRUE;
			}
			event |= MU_M2;
		}
		if (event & MU_BUTTON) {
			if (out.emo_mbutton) {
				in.emi_bmask   = out.emo_mbutton;
				in.emi_bclicks = 1;
				in.emi_bstate  = 0; /* always wait for button released */
				which =  0;
				event |= MU_M2;
			} else if (which > 0 && tree[which].ob_type == G_BUTTON
				                  && tree[which].ob_state & OS_DISABLED) {
				in.emi_bmask   = 3;
				in.emi_bclicks = 0x102;
				in.emi_bstate  = 0;
				which = 0;
			} else {
				ready = TRUE;
			}
		}
		if (event & MU_M2) {
			if (which > 0) {
				tree[which].ob_state &= ~OS_SELECTED;
				objc_draw (tree, ROOT, MAX_DEPTH,
				           in.emi_m2.g_x, in.emi_m2.g_y,
				           in.emi_m2.g_w, in.emi_m2.g_h);
			}
			which = objc_find (tree, ROOT, MAX_DEPTH,
			                   out.emo_mouse.p_x, out.emo_mouse.p_y);
			if (which > 0) {
				objc_offset (tree, which, &in.emi_m2.g_x, &in.emi_m2.g_y);
				in.emi_m2.g_w = tree[which].ob_width;
				in.emi_m2.g_h = tree[which].ob_height;
				if (tree[which].ob_type == G_BUTTON) {
					in.emi_m2.g_x -= 2;
					in.emi_m2.g_y -= 2;
					in.emi_m2.g_w += 4;
					in.emi_m2.g_h += 4;
					if (out.emo_mbutton && !(tree[which].ob_state & OS_DISABLED)) {
						objc_change (tree, which, 0,
						             in.emi_m2.g_x, in.emi_m2.g_y,
						             in.emi_m2.g_w, in.emi_m2.g_h, OS_SELECTED, 1);
					}
				} else if (tree[which].ob_state & OS_DISABLED) {
					which = 0;
				} else {
					WORD col_tab[] = { G_BLACK, G_WHITE, G_LBLACK },
					   * color = col_tab;
					if (out.emo_mbutton) {
						objc_change (tree, which, 0,
						             in.emi_m2.g_x, in.emi_m2.g_y,
						             in.emi_m2.g_w, in.emi_m2.g_h, OS_SELECTED, 1);
					} else {
						color++;
					}
					v_hide_c (vdi_handle);
					draw_border (&in.emi_m2, color[0], color[1], 1);
					v_show_c (vdi_handle, 1);
				}
			}
		}
	} while (!ready);
	
	return which;
}


/*============================================================================*/
WORD
HW_form_popup (char * tab[], WORD x, WORD y, BOOL popNmenu)
{
	static char sepr[] = "----------------------------------------"
	                     "---------------------------------------",
	            t_dn[] = "", t_up[] = "";
	static OBJECT * o_tree = NULL;
	static short    o_pbeg = 1, o_pend;
	static short    chr_w, chr_h;
	static GRECT    clip;
	
	short tab_num = 0; /* number of lines in tab     */
	short tab_len = 0; /* maximum line length in tab */
	WORD  ret     = -1;
	
	if (!o_tree) {
		OBJECT root = { -1,-1,-1, G_BOX, OF_FL3DBAK, OS_OUTLINED,
		                { (long)0xFE1100L }, 4,1, 1,1 };
		short n = 0, i;
		
		rsrc_obfix (&root, ROOT);
		chr_w = root.ob_width;
		chr_h = root.ob_height;
		wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &clip);
		i = (clip.g_h -4) / chr_h;
		o_pend = o_pbeg + i -1;
		o_tree = malloc (sizeof(OBJECT) * (1 + i));
		for (i = o_pbeg;;) {
			o_tree[i].ob_type  = G_STRING;
			o_tree[i].ob_flags = OF_SELECTABLE|OF_TOUCHEXIT|OF_FL3DBAK;
			o_tree[i].ob_state = OS_NORMAL;
			o_tree[i].ob_spec.free_string = sepr;
			o_tree[i].ob_x      = 0;
			o_tree[i].ob_y      = n;
			o_tree[i].ob_width  = chr_w;
			o_tree[i].ob_height = chr_h;
			n += chr_h;
			o_tree[i].ob_head = o_tree[i].ob_tail = -1;
			if (i < o_pend) {
				o_tree[i].ob_next = i +1;
				i++;
			} else {
				o_tree[i].ob_next  =  0;
				o_tree[i].ob_flags |= OF_LASTOB;
				break;
			}
		}
		o_tree[0]         = root;
		o_tree->ob_head   = o_pbeg;
		o_tree->ob_tail   = o_pend;
		o_tree->ob_height = n;
	}
	
	/* count number of lines in tab[] and calculate the maximum line length
	*/
	while (tab[tab_num]) {
		char * str = tab[tab_num++];
		size_t len = strlen (str);
		if (len >= 80) str[len = 79] = '\0';
		while (len && isspace (str[len -1])) len--;
		if (str[0] == '-') {
			while (str[len -1] == '-' && str[len -2] == '-') len--;
		} else {
			len++;
		}
		if (len > tab_len) tab_len = len;
	}
	
	wind_update (BEG_MCTRL);
	if (tab_num) {
		WORD off = 0;
		WORD num = min (tab_num, o_pend - o_pbeg +1);
		WORD beg = o_pbeg;
		WORD end = o_pbeg + num -1;
		WORD cx, cy, cw, ch, n, i;
		o_tree->ob_tail   = num;
		o_tree->ob_width  = tab_len * chr_w;
		o_tree->ob_height = o_tree[num].ob_y + o_tree[num].ob_height;
		o_tree[num].ob_next  =  0;
		o_tree[num].ob_flags |= OF_LASTOB;
		for (i = o_pbeg; i <= num; o_tree[i++].ob_width = o_tree->ob_width);
		if (popNmenu) {
			o_tree->ob_flags = OF_FL3DBAK;
			o_tree->ob_state = OS_OUTLINED;
			o_tree->ob_spec.obspec.framesize = -2;
			i = 0;
		} else {
			o_tree->ob_flags = OF_FL3DIND;
			o_tree->ob_state = OS_SHADOWED;
			o_tree->ob_spec.obspec.framesize = -1;
			i = 3;
		}
		form_center (o_tree, &cx, &cy, &cw, &ch);
		o_tree->ob_x -= cx - i;
		o_tree->ob_y -= cy - i;
		cx = (x + (cw += i *2) <= (n = clip.g_x + clip.g_w) ? x : n - cw);
		if (cx < clip.g_x) cx = clip.g_x;
		cy = (y + (ch += i *2) <= (n = clip.g_y + clip.g_h) ? y : n - ch);
		if (cy < clip.g_y) cy = clip.g_y;
		o_tree->ob_x += cx;
		o_tree->ob_y += cy;
		
		if (num < tab_num) {
			num -= 2;
			o_tree[beg].ob_type   = G_BUTTON;
			o_tree[beg].ob_flags |= OF_SELECTABLE|OF_FL3DIND;
			o_tree[beg].ob_state  = OS_NORMAL;
			o_tree[beg].ob_height = chr_h -2;
			o_tree[beg].ob_spec.free_string = t_up;
			beg++;
			o_tree[end].ob_type   = G_BUTTON;
			o_tree[end].ob_flags |= OF_SELECTABLE|OF_FL3DIND;
			o_tree[end].ob_state  = OS_NORMAL;
			o_tree[end].ob_height = chr_h -2;
			o_tree[end].ob_y      = o_tree->ob_height - o_tree[end].ob_height;
			o_tree[end].ob_spec.free_string = t_dn;
			end--;
		}
		
		form_dial (FMD_START, cx, cy, cw, ch, cx, cy, cw, ch);
		while (TRUE) {
			if (num < tab_num) {
				o_tree[beg -1].ob_state = (off > 0 ? OS_NORMAL : OS_DISABLED);
				o_tree[end +1].ob_state = (off < tab_num - num
				                                   ? OS_NORMAL : OS_DISABLED);
			}
			for (n = off, i = beg; i <= end; i++) {
				char * str = tab[n++];
				if (*str == '-') {
					char * p = str;
					while (*(++p) == '-');
					o_tree[i].ob_flags &= ~OF_SELECTABLE;
					o_tree[i].ob_state  = OS_DISABLED;
					o_tree[i].ob_spec.free_string = (*p ? str : sepr);
				} else {
					o_tree[i].ob_flags |= OF_SELECTABLE;
					if (*str == '!') {
						*str = ' ';
						o_tree[i].ob_state = OS_DISABLED;
					} else {
						o_tree[i].ob_state = OS_NORMAL;
					}
					o_tree[i].ob_spec.free_string = str;
				}
			}
			objc_draw (o_tree, ROOT, MAX_DEPTH, cx, cy, cw, ch);
			if ((n = HW_form_do (o_tree, 0)) > 0) {
				ret = n - beg + off;
			}
			for (i = beg; i <= end; i++) {
				if (o_tree[i].ob_spec.free_string[0] == ' ' &&
				    o_tree[i].ob_state & OS_DISABLED) {
					o_tree[i].ob_spec.free_string[0] = '!';
				}
			}
			if (num < tab_num) {
				if (n == beg -1) {
					ret = 0;
					off = max (off - num, 0);
					continue;
				} else if (n == end +1) {
					ret = 0;
					off = min (off + num, tab_num - num);
					continue;
				}
			}
			break;
		}
		form_dial (FMD_FINISH, cx, cy, cw, ch, cx, cy, cw, ch);
		
		if (tab_num < o_pend) {
			o_tree[tab_num].ob_next  =  tab_num +1;
			o_tree[tab_num].ob_flags &= ~OF_LASTOB|OF_FL3DIND;
			o_tree->ob_tail          =  o_pend;
		} else if (num < tab_num) {
			beg--;
			o_tree[beg].ob_type   = G_STRING;
			o_tree[beg].ob_flags &= ~OF_FL3DIND;
			o_tree[beg].ob_state  = OS_NORMAL;
			o_tree[beg].ob_height = chr_h;
			end++;
			o_tree[end].ob_type   = G_STRING;
			o_tree[end].ob_flags &= ~OF_FL3DIND;
			o_tree[end].ob_height = chr_h;
			o_tree[end].ob_y      = o_tree->ob_height - o_tree[end].ob_height;
		}
	}
	wind_update (END_MCTRL);
	
	return ret;
}


/*******************************************************************************
 *
 * User Interface functions.  Here they are realized by form_alerts but could
 * also be done by shell IO.
*/

/*----------------------------------------------------------------------------*/
static WORD
hwUi_box (WORD icon, const char * bttn,
          const char * hint, const char * text, va_list valist)
{
	/* form_alert
	 * [n]                              =   3
	 * + 5 lines   40 character (= 200) = 203
	 * + '[||||]'                       = 209
	 * + 3 buttons   20 + '[||] (= 64)  = 273
	*/
	char buf[1000], * p = buf;
	
	{	/* icon + head line */
		const char head[] = "["_HIGHWIRE_FULLNAME_": ";
		*(p++) = '[';
		*(p++) = icon + '0';
		*(p++) = ']';
		memcpy (p, head, sizeof(head) -1);
		p += sizeof(head) -1;
		
		if (hint && *hint) {
			p = strchr (strcpy (p, hint), '\0');
			if (p - buf > 40 +4) {
				p = buf +39 +4;
				*(p++) = '¯';
			}
		}
		*(p++) = '|';
	}
	
	{	/* critical part, check for array boundary overwritten */
		size_t fill = p - buf, left = sizeof(buf) - fill;
		buf[sizeof(buf)-1] = '\0';
		if (left <= vsprintf (p, text, valist) || buf[sizeof(buf)-1] != '\0') {
			buf[1] = '3'; /*Stop icon */
			strcpy (buf + fill, " | -- Stack destroyed! -- | ][ABORT]");
			form_alert (1, buf);
			abort();
		}
	}
	
	{	/* format the text */
		short  cnt = 4;
		char * beg = p;
		char * spc = NULL;
		BOOL  done = FALSE;
		do switch (*p) {
			case '\0': done = TRUE; break;
			
			/* special characters */
			case '[':  *p = '{'; goto case_dflt;
			case '|':  *p = '/'; goto case_dflt;
			case ']':  *p = '}'; goto case_dflt;
			
			case '\r':
				if (p[1] == '\n') {
					memmove (p +1, p +2, (sizeof(buf) - (p - buf)) -2);
					goto case_crlf;
				}
			default:
			case_dflt:
				if (p - beg >= 40) {
					if (cnt == 1) { /* last line */
						p[-1] = '¯';
						goto case_crlf;
					}
					if (!spc) { /* line too long */
						memmove (p +1, p, (sizeof(buf) - (p - buf)) -2);
						goto case_crlf;
					}
					*spc = '|';
					beg  = spc +1;
					spc  = NULL;
					done = !--cnt;
				
				} else {
					if (*p <= ' ') {
						if (p > beg) {
							spc = p;
						}
						*p  = ' ';
					}
					p++;
				}
				break;
			
			case '\n':
			case_crlf:
				if ((done = !--cnt) == FALSE) {
					*(p++) = '|';
					spc    = NULL;
					beg    = p;
				}
				break;
		} while (!done);
		*(p++) = ']';
	}
	strcat (buf, bttn);
	
	return form_alert (1, buf);
}

/*============================================================================*/
void
hwUi_fatal (const char * hint, const char * text, ...)
{
	va_list valist;
	va_start (valist, text);
	hwUi_box (3, "[Exit]", hint, text, valist);
	exit (EXIT_FAILURE);
}

/*============================================================================*/
void
hwUi_error (const char * hint, const char * text, ...)
{
	va_list valist;
	va_start (valist, text);
	if (hwUi_box (1, "[Exit|Continue]", hint, text, valist) == 1) {
		exit (EXIT_FAILURE);
	}
	va_end (valist);
}

/*============================================================================*/
void
hwUi_warn (const char * hint, const char * text, ...)
{
	va_list valist;
	va_start (valist, text);
	if (hwUi_box (1, "[Continue|Exit]", hint, text, valist) == 2) {
		exit (EXIT_SUCCESS);
	}
	va_end (valist);
}

/*============================================================================*/
void
hwUi_info (const char * hint, const char * text, ...)
{
	va_list valist;
	va_start (valist, text);
	if (hwUi_box (0, "[Continue|Exit]", hint, text, valist) == 2) {
		exit (EXIT_SUCCESS);
	}
	va_end (valist);
}
