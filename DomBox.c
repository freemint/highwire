#include <stdlib.h>
#include <string.h>
#include <stdio.h> /* debug */

#include <gem.h>

#include "global.h"


static void vTab_delete (DOMBOX *);
static void vTab_draw   (DOMBOX *, long x, long y, const GRECT * clip, void *);
struct s_dombox_vtab DomBox_vTab = {
	vTab_delete,
	vTab_draw
};


/*============================================================================*/
DOMBOX *
dombox_ctor (DOMBOX * This, DOMBOX * parent, BOXCLASS class)
{
	memset (This, 0, sizeof (struct s_dombox));
	
	This->_vtab = &DomBox_vTab;
	
	if (This == parent) {
		puts ("dombox_ctor(): This and parent are equal!");
		return This;
	}
	
	if (parent) {
		if (!class) {
			puts ("dombox_ctor(): no class given.");
		}
		if (parent->ChildEnd) parent->ChildEnd->Sibling = This;
		else                  parent->ChildBeg          = This;
		parent->ChildEnd = This;
		This->Parent     = parent;
	} else {
		if (class) {
			puts ("dombox_ctor(): main got class.");
		}
	}
	This->BoxClass = class;
	This->Backgnd  = -1;
	
	return This;
}

/*============================================================================*/
DOMBOX *
dombox_dtor (DOMBOX * This)
{
	if ((int)This->BoxClass < 0) {
		puts ("dombox_dtor(): already destroyed!");
	}
	if (This->ChildBeg) {
		puts ("dombox_dtor(): has still children!");
	}
	if (This->Parent) {
		DOMBOX ** ptr = &This->Parent->ChildBeg;
		while (*ptr) {
			if (*ptr == This) {
				*ptr = This->Sibling;
				break;
			}
			ptr = &(*ptr)->Sibling;
		}
		This->Parent = NULL;
	}
	This->BoxClass = -1;
	
	return This;
}


/*============================================================================*/
void
dombox_draw (DOMBOX * This, long x, long y, const GRECT * clip, void * hl)
{
	long x1 = x + This->Margin.Lft;
	long x2 = x - This->Margin.Rgt + This->Rect.W -1;
	long y1 = y + This->Margin.Top;
	long y2 = y - This->Margin.Bot + This->Rect.H -1;
	
	if (This->BorderWidth) {
		if (This->BorderColor >= 0) {
			short n = This->BorderWidth;
			PXY b[5];
			b[3].p_x = b[0].p_x = x1;
			b[1].p_x = b[2].p_x = x2;
			b[1].p_y = b[0].p_y = y1;
			b[3].p_y = b[2].p_y = y2;
			vsl_color (vdi_handle, This->BorderColor);
			while(1) {
				b[4] = b[0];
				v_pline (vdi_handle, 5, (short*)b);
				if (!--n) break;
				b[3].p_x = ++b[0].p_x;  b[1].p_x = --b[2].p_x;
				b[1].p_y = ++b[0].p_y;  b[3].p_y = --b[2].p_y;
			}
		} else { /* 3D */
			GRECT b;
			b.g_x = x1;
			b.g_y = y1;
			b.g_w = x2 - x1 +1;
			b.g_h = y2 - y1 +1;
			if (This->BorderColor == -1) { /* outset */
				draw_border (&b, G_WHITE, G_LBLACK, This->BorderWidth);
			} else { /* inset */
				draw_border (&b, G_LBLACK, G_WHITE, This->BorderWidth);
			}
		}
		x1 += This->BorderWidth;
		x2 -= This->BorderWidth;
		y1 += This->BorderWidth;
		y2 -= This->BorderWidth;
	}
	if (This->Backgnd >= 0) {
		PXY p[2];
		p[0].p_x = ((long)clip->g_x >= x1 ? clip->g_x : x1);
		p[0].p_y = ((long)clip->g_y >= y1 ? clip->g_y : y1);
		p[1].p_x = (x2 <= 0x7FFFL ? x2 : 0x7FFF);
		p[1].p_y = (y2 <= 0x7FFFL ? y2 : 0x7FFF);
		if (p[0].p_x > p[1].p_x || p[0].p_y > p[1].p_y) {
			return;
		}
		vsf_color (vdi_handle, This->Backgnd);
		v_bar     (vdi_handle, (short*)p);
	}
	
	(*This->_vtab->draw)(This, x, y, clip, hl);
}

/*----------------------------------------------------------------------------*/
static void
vTab_delete (DOMBOX * This)
{
	while (This->ChildBeg) {
		Delete (This->ChildBeg);
	}
	free (dombox_dtor (This));
}

/*----------------------------------------------------------------------------*/
static void
vTab_draw (DOMBOX * This, long x, long y, const GRECT * clip, void * highlight)
{
	DOMBOX * box    = This->ChildBeg;
	long     clip_y = (long)clip->g_y - y;
	
	while (box && box->Rect.Y + box->Rect.H <= clip_y) {
		box = box->Sibling;
	}
	clip_y = clip->g_y + clip->g_h -1;

	while (box) {
		long b_x = x + box->Rect.X;
		long b_y = y + box->Rect.Y;

		if (b_y > clip_y) break;
		
		dombox_draw (box, b_x, b_y, clip, highlight);
		
		box = box->Sibling;
	}
}
