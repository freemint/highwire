/* @(#)highwire/Widget.c
 *
 * handles HighWire Widgets and their controls
 * eventually ....
 * baldrick August 9, 2002
 */
#include <stdlib.h>
#include <string.h>

#ifdef __PUREC__
#include <tos.h>

#else /* LATTICE || __GNUC__ */
#include <osbind.h>
#endif

#include <gem.h>

#include "global.h"
#include "Containr.h"

#include "Loader.h"
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
				if (tree[which].ob_state & OS_DISABLED) {
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
	                     "---------------------------------------";
	static OBJECT * o_tree = NULL;
	static short    o_pbeg = 1, o_pend;
	static short    chr_w, chr_h;
	static GRECT    clip;
	
	WORD    ret = -1;
	char ** str = tab;
	short   num = 0;
	short   len = 0;
	short   i;
	
	if (!o_tree) {
		OBJECT root = { -1,-1,-1, G_BOX, OF_FL3DBAK, OS_OUTLINED,
		                { (long)0xFE1100L }, 4,1, 1,1 };
		short n = 0;
		
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
	
	i = o_pbeg -1;
	while (*str && ++i <= o_pend) {
		size_t l = strlen (*str);
		if (l >= 80) (*str)[l = 79] = '\0';
		if (**str == '-') {
			o_tree[i].ob_flags &= ~OF_SELECTABLE;
			o_tree[i].ob_state = OS_DISABLED;
			if (strspn ((*str) +1, "-") == l) {
				o_tree[i].ob_spec.free_string = sepr;
				l = 0;
			} else {
				o_tree[i].ob_spec.free_string = *str;
				while ((*str)[l -1] == '-' && (*str)[l -2] == '-') l--;
			}
		} else {
			o_tree[i].ob_flags |= OF_SELECTABLE;
			if (**str == '!') {
				**str = ' ';
				o_tree[i].ob_state = OS_DISABLED;
			} else {
				o_tree[i].ob_state = OS_NORMAL;
			}
			o_tree[i].ob_spec.free_string = *str;
			if ((*str)[l -1] != ' ') l++;
		}
		if (l > len) len = l;
		str++;
		num++;
	}
	wind_update (BEG_MCTRL);
	if (len) {
		WORD cx, cy, cw, ch, n;
		o_tree->ob_tail   = (num += o_pbeg -1);
		o_tree->ob_width  = len * chr_w;
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
		
		form_dial (FMD_START, cx, cy, cw, ch, cx, cy, cw, ch);
		objc_draw (o_tree, ROOT, MAX_DEPTH, cx, cy, cw, ch);
		if ((n = HW_form_do (o_tree, 0)) > 0) {
			ret = n - o_pbeg;
		}
		form_dial (FMD_FINISH, cx, cy, cw, ch, cx, cy, cw, ch);
		
		for (i = o_pbeg; i <= num; i++) {
			if (o_tree[i].ob_spec.free_string[0] == ' ' &&
			    o_tree[i].ob_state & OS_DISABLED) {
				o_tree[i].ob_spec.free_string[0] = '!';
			}
		}
		if (num < o_pend) {
			o_tree[num].ob_next  =  num +1;
			o_tree[num].ob_flags &= ~OF_LASTOB;
			o_tree->ob_tail      =  o_pend;
		}
	}
	wind_update (END_MCTRL);
	
	return ret;
}
