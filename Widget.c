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
HW_form_do(OBJECT *tree, WORD next)
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
