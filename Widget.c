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
#if 0 /***** REPLACED *****/
{
	WORD edit;
	WORD which, cont;
	WORD idx; 
	WORD       msg[8];
	EVMULT_OUT out;
	GRECT tree_area;
	
	EVMULT_IN multi_widget = {
		MU_MESAG|MU_BUTTON|MU_TIMER|MU_KEYBD|MU_M1,
		0x102, 3, 0,			/* mouses */
		MO_LEAVE,  {  0, 0, 0, 0 },   /* M1 */
		0,        {  0, 0, 0, 0 },   /* M2 */
		1, 0                       /* Timer */
	};

	tree_area.g_x = tree->ob_x;
	tree_area.g_y = tree->ob_y;
	tree_area.g_w = tree->ob_width;
	tree_area.g_h = tree->ob_height;

    multi_widget.emi_m1 = tree_area;
    
	edit = 0;
	cont = 1;
	while (cont)
	{
		/* position of the cursor on an editing field */

		if (next!=0 && edit !=next)
		{
			edit = next;
			next = 0;
			/*turn on the text cursor and initialise idx */
			objc_edit(tree, edit, 0, &idx, ED_INIT);
		}

		/* wait for mouse or key */

		which = evnt_multi_fast (&multi_widget, msg, &out);

		if (which & MU_KEYBD)
		{
			if (next!=0)
			{
				/* process the keystroke */

				cont = form_keybd(tree, edit, 0, out.emo_kreturn, &next, &out.emo_kreturn);

				if (out.emo_kreturn)
					/* if not special the edit the form */
					objc_edit(tree, edit, out.emo_kreturn, &idx, ED_CHAR);
			}
			else
			{
				/* handler for hot keys */

				cont = 0;
				next = out.emo_kreturn;
			}
		}

		if (which & MU_BUTTON)
		{
			/* find the object under the rodent */
			next = objc_find(tree, ROOT, MAX_DEPTH,
					 out.emo_mouse.p_x, out.emo_mouse.p_y);

			if (next == (int)NULL)
			{
				/* If no object then ring the bell */

				Bconout(2,'\a');
				next = 0;
			}
			else 
			{
				/* else process the button */
				cont = form_button(tree, next, out.emo_mbutton, &next);
			}
		}

		if (which & MU_M1)
			cont = 0;

		/* If finished or moving to a new object */

		if (!cont || (next!=0 && next != edit))
		{
			/* then hide the text cursor */
			objc_edit(tree, edit, 0, &idx, ED_END);
		}
	}

	return next;
}
#endif  /***** REPLACED *****/
