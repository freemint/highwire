#include <stdlib.h>
#include <string.h>
#include <stdio.h> /* debug */

#include <gem.h>

#include "global.h"


static void     vTab_delete   (DOMBOX *);
static LONG     vTab_MinWidth (DOMBOX *);
static LONG     vTab_MaxWidth (DOMBOX *);
static PARAGRPH vTab_Paragrph (DOMBOX *);
static DOMBOX * vTab_ChildAt  (DOMBOX *, LRECT *, long x, long y, long clip[4]);
static void     vTab_draw     (DOMBOX *, long x, long y, const GRECT *, void *);
static void     vTab_format   (DOMBOX *, long width, BLOCKER);
struct s_dombox_vtab DomBox_vTab = {
	vTab_delete,
	vTab_MinWidth,
	vTab_MaxWidth,
	vTab_Paragrph,
	vTab_ChildAt,
	vTab_draw,
	vTab_format
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
	This->Floating = ALN_NO_FLT;
	
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
		DOMBOX * box = This->Parent->ChildBeg;
		if (This == box) {
			if ((This->Parent->ChildBeg = This->Sibling) == NULL) {
				This->Parent->ChildEnd = NULL;
			}
		} else do {
			if (box->Sibling == This) {
				if ((box->Sibling = This->Sibling) == NULL) {
					This->Parent->ChildEnd = box;
				}
				break;
			}
		} while ((box = box->Sibling) != NULL);
		
		This->Parent = NULL;
	}
	if (This->IdName) {
		free (This->IdName);
		This->IdName = NULL;
	}
	if (This->ClName) {
		free (This->ClName);
		This->ClName = NULL;
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
static LONG
vTab_MinWidth (DOMBOX * This)
{
	DOMBOX * box = This->ChildBeg;
	
	This->MinWidth = (This->SetWidth > 0 ? This->SetWidth : 0);
	while (box) {
		long width = dombox_MinWidth (box);
		if (This->MinWidth < width) {
			 This->MinWidth = width;
		}
		box = box->Sibling;
	}
	This->MinWidth += dombox_LftDist (This) + dombox_RgtDist (This);
	
	if (This->SetWidth > 0 && This->SetWidth < This->MinWidth) {
		This->SetWidth = This->MinWidth;
	}
	return This->MinWidth;
}

/*----------------------------------------------------------------------------*/
static LONG
vTab_MaxWidth (DOMBOX * This)
{
	if (This->SetWidth > 0) {
		This->MaxWidth = This->SetWidth;
	
	} else {
		DOMBOX * box = This->ChildBeg;
		
		This->MaxWidth = 0;
		while (box) {
			long width = dombox_MaxWidth (box);
			if (This->MaxWidth < width) {
				 This->MaxWidth = width;
			}
			box = box->Sibling;
		}
		This->MaxWidth += dombox_LftDist (This) + dombox_RgtDist (This);
	}
	return This->MaxWidth;
}


/*----------------------------------------------------------------------------*/
static PARAGRPH
vTab_Paragrph (DOMBOX * This)
{
	(void)This;
	return NULL;
}


/*----------------------------------------------------------------------------*/
#define   c_lft clip[0]
#define   c_rgt clip[2]
#define   c_top clip[1]
#define   c_bot clip[3]
static DOMBOX *
vTab_ChildAt (DOMBOX * This, LRECT * r, long x, long y, long clip[4])
{
	DOMBOX * cld = This->ChildBeg;
	LONG     top = 0, bot;
	
	while (y >= (bot = cld->Rect.Y + cld->Rect.H)) {
		if (top < bot) {
			top = bot;
		}
		if ((cld = cld->Sibling) == NULL) { /* below the lowest box */
			break;
		}
	}
	c_top = (bot = top) + r->Y;
	while (cld) {
		if (y < cld->Rect.Y) { /* between two boxes */
			if (c_top < bot + r->Y) {
				c_top = r->Y + bot;
			}
			if (c_bot >= r->Y + cld->Rect.Y) {
				c_bot = r->Y + cld->Rect.Y -1;
			}
			cld = NULL;
			break;
		}
		if (x < cld->Rect.X) {
			if (y < cld->Rect.Y + cld->Rect.H) {
				long rgt = r->X + cld->Rect.X -1;
				if (rgt < c_rgt) {
					c_rgt = rgt;
				}
			}
		} else if (x >= cld->Rect.X + cld->Rect.W) {
			if (y < cld->Rect.Y + cld->Rect.H) {
				long lft = r->X + cld->Rect.X + cld->Rect.W -1 +1;
				if (lft > c_lft) {
					c_lft = lft;
				}
			}
		} else {
			if (y < (bot = cld->Rect.Y + cld->Rect.H -1 +1)) {
				break; /* inside the child box */
			} else if (c_top < bot + r->Y) {
				c_top = bot + r->Y;
			}
		}
		cld = cld->Sibling;
	}
	return cld;
}


/*============================================================================*/
DOMBOX *
dombox_Offset (DOMBOX * This, long * x, long * y)
{
	DOMBOX * box = This;
	if (!box) {
		*x = 0;
		*y = 0;
	
	} else {
		*x = box->Rect.X;
		*y = box->Rect.Y;
		while (box->Parent) {
			box = box->Parent;
			*x += box->Rect.X;
			*y += box->Rect.Y;
		}
	}
	return box;
}


/*==============================================================================
 * Returns the box that contains the coordinate px/py which must be relative to
 * the start box's origin.  The rectangle r is set to the extent of this box,
 * relative to px/py (resulting in negative X/Y).  The rectangle excludes all
 * possible overlapping areas of other boxes.  Then px/py will be set to the
 * origin of the box, also relative to the coordinate.
*/
DOMBOX * 
dombox_byCoord (DOMBOX * box, LRECT * r, long * px, long * py)
{
	DOMBOX * cld = box->ChildBeg;
	long     x   = *px;
	long     y   = *py;
	long     clip[4];
	
	c_rgt = (c_lft = r->X = -x) + box->Rect.W -1;
	c_bot = (c_top = r->Y = -y) + box->Rect.H -1;

	while (cld) {
		if (y < cld->Rect.Y) {
			c_bot = r->Y + cld->Rect.Y -1;
			break;
		}
		if (x < dombox_LftDist (box)) {
			c_rgt = r->X + dombox_LftDist (box) -1;
			break;
		}
		if (x >= box->Rect.W - dombox_RgtDist (box)) {
			c_lft = r->X + box->Rect.W - dombox_RgtDist (box) -1 +1;
			break;
		}
		if (y >= box->Rect.H - dombox_BotDist (box)) {
			c_top = r->Y + box->Rect.H - dombox_BotDist (box) -1 +1;
			break;
		}
		if ((cld = box->_vtab->ChildAt (box, r, x, y, clip)) != NULL) {
			r->X += cld->Rect.X;
			r->Y += cld->Rect.Y;
			box  =  cld;
			cld  =  box->ChildBeg;
			x    -= box->Rect.X;
			y    -= box->Rect.Y;
		}
	}
	*px = r->X;
	*py = r->Y;
	if (c_rgt < r->X + box->Rect.W -1) {
		r->W = c_rgt - r->X +1;
	} else {
		r->W = box->Rect.W;
	}
	if (c_lft > r->X) {
		r->W -= (c_lft - r->X);
		r->X =  c_lft;
	}
	if (c_bot < r->Y + box->Rect.H -1) {
		r->H = c_bot - r->Y +1;
	} else {
		r->H = box->Rect.H;
	}
	if (c_top > r->Y) {
		r->H -= (c_top - r->Y);
		r->Y =  c_top;
	}
	
	return box;
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


/*----------------------------------------------------------------------------*/
static char *
_set_text (char ** pstr, const char * text, BOOL force)
{
	char * str = (text && *text && (!*pstr || force) ? strdup (text) : NULL);
	if (str) {
		if (*pstr) free (*pstr);
		*pstr = str;
	}
	return str;
}

/*============================================================================*/
char *
dombox_setId (DOMBOX *This, const char * text, BOOL force)
{
	return _set_text (&This->IdName, text, force);
}

/*============================================================================*/
char *
dombox_setClass (DOMBOX *This, const char * text, BOOL force)
{
	return _set_text (&This->ClName, text, force);
}


/*============================================================================*/
void
dombox_reorder (DOMBOX * This, DOMBOX * behind)
{
	DOMBOX * parent = This->Parent;
	
	if (behind) {
		if (parent != behind->Parent) {
			puts ("dombox_reorder(): parents differ!");
			return;
		}
		if (This == behind->Sibling) {
			return; /* nothing to do, already at the right position */
		}
	}
	
	if (This == parent->ChildBeg) {
		if (!behind) return; /* nothing to do, already at the begin */
		parent->ChildBeg = This->Sibling;
	
	} else {
		DOMBOX * before = parent->ChildBeg;
		while (before->Sibling != This) {
			if ((before = before->Sibling) == NULL) {
				puts ("dombox_reorder(): not in chain!");
				return;
			}
		}
		if ((before->Sibling = This->Sibling) == NULL) {
			parent->ChildEnd = before;
		}
	}
	if (!behind) {
		This->Sibling    = parent->ChildBeg;
		parent->ChildBeg = This;
	
	} else {
		if ((This->Sibling = behind->Sibling) == NULL) {
			parent->ChildEnd = This;
		}
		behind->Sibling = This;
	}
}

/*============================================================================*/
void
dombox_adopt (DOMBOX * This, DOMBOX * stepchild)
{
	DOMBOX * parent = stepchild->Parent;
	if (stepchild == parent->ChildBeg) {
		if ((parent->ChildBeg = stepchild->Sibling) == NULL) {
			parent->ChildEnd = NULL;
		} else {
			stepchild->Sibling = NULL;
		}
	} else {
		DOMBOX * before = parent->ChildBeg;
		while (before->Sibling != stepchild) {
			if ((before = before->Sibling) == NULL) {
				puts ("dombox_adopt(): not in chain!");
				return;
			}
		}
		if ((before->Sibling = stepchild->Sibling) == NULL) {
			parent->ChildEnd = before;
		} else {
			stepchild->Sibling = NULL;
		}
	}
	if (This->ChildEnd) This->ChildEnd->Sibling = stepchild;
	else                This->ChildBeg          = stepchild;
	This->ChildEnd    = stepchild;
	stepchild->Parent = This;
}


/*----------------------------------------------------------------------------*/
static void
vTab_format (DOMBOX * This, long width, BLOCKER p_blocker)
{
	DOMBOX * box    = This->ChildBeg;
	long     height = This->Rect.H = dombox_TopDist (This);
	
	struct blocking_area t_blocker = *p_blocker;
	BLOCKER              blocker   = &t_blocker;
	if (blocker->L.bottom) blocker->L.bottom -= This->Rect.Y;
	if (blocker->R.bottom) blocker->R.bottom -= This->Rect.Y;
	
	if (This->BoxClass >= BC_GROUP && This->SetWidth) {
		long gap = width;
		if (This->SetWidth > 0) {
			width = This->SetWidth;
		} else if (This->SetWidth > -1024) {
			width = (-This->SetWidth * width +512) /1024;
			if (width < This->MinWidth) {
				width = This->MinWidth;
			}
		}
		if ((gap -= width) > 0) {
			switch (This->Floating) {
				case ALN_NO_FLT:
				case FLT_LEFT:
					if (blocker->R.bottom && (blocker->R.width -= gap) <= 0) {
						blocker->R.bottom = blocker->R.width = 0;
					}
					break;
				case FLT_RIGHT:
					if (blocker->L.bottom && (blocker->L.width -= gap) <= 0) {
						blocker->L.bottom = blocker->L.width = 0;
					}
					break;
				default: ;
			}
		}
	}
	This->Rect.W = width;
	width -= dombox_LftDist (This) + dombox_RgtDist (This);
	
	while (box) {
		long set_width = width;
		BOOL floating  = (box->Floating != ALN_NO_FLT);
		BOOL absolute  = (box->SetPosMsk > 0x100);
		
		box->Rect.X = dombox_LftDist (This);
		box->Rect.Y = height;
		
		if (absolute) {
			if (box->SetPosMsk & 0x01) box->Rect.X = box->SetPos.p_x;
			if (box->SetPosMsk & 0x02) box->Rect.Y = box->SetPos.p_y;
			floating = FALSE;
		
		} else if (box->ClearFlt) {
			L_BRK clear = box->ClearFlt & ~BRK_LN;
			if (blocker->L.bottom && (clear & BRK_LEFT)) {
				if (height < blocker->L.bottom) {
					height = blocker->L.bottom;
				}
				blocker->L.bottom = blocker->L.width = 0;
			}
			if (blocker->R.bottom && (clear & BRK_RIGHT)) {
				if (height < blocker->R.bottom) {
					height = blocker->R.bottom;
				}
				blocker->R.bottom = blocker->R.width = 0;
			}
			box->Rect.Y = height;
		}
		
		if (floating) {
			long blk_width = width - blocker->L.width - blocker->R.width;
			if (box->MinWidth > blk_width) {
				if (height < blocker->L.bottom) height = blocker->L.bottom;
				if (height < blocker->R.bottom) height = blocker->R.bottom;
				blocker->L.bottom = blocker->L.width =
				blocker->R.bottom = blocker->R.width = 0;
				box->Rect.Y = height;
			} else {
				set_width = blk_width;
			}
		}
		
		if ((box->Floating & FLT_MASK) && !box->SetWidth &&
		    (box->BoxClass >= BC_GROUP && box->BoxClass != BC_SINGLE)) {
			long max_width = dombox_MaxWidth (box);
			if (set_width > max_width) {
				set_width = max_width;
			}
		}
		
		if (floating || absolute) {
			struct blocking_area empty = { {0, 0}, {0, 0} };
			box->_vtab->format (box, set_width, &empty);
		} else {
			box->_vtab->format (box, set_width, blocker);
		}
		
		if (!absolute) switch (box->Floating) {
			case FLT_RIGHT:
				box->Rect.X += width - box->Rect.W;
				if (blocker->R.bottom < height + box->Rect.H)
					 blocker->R.bottom = height + box->Rect.H;
				blocker->R.width += box->Rect.W;
				goto case_FLT_MASK;
			case FLT_LEFT:
				box->Rect.X += blocker->L.width;
				if (blocker->L.bottom < height + box->Rect.H)
					 blocker->L.bottom = height + box->Rect.H;
				blocker->L.width += box->Rect.W;
				goto case_FLT_MASK;
			case_FLT_MASK:
				if (This->Rect.H < box->Rect.Y + box->Rect.H) {
					 This->Rect.H = box->Rect.Y + box->Rect.H;
				}
				break;
			
			case ALN_CENTER:
				box->Rect.X += blocker->L.width + (set_width - box->Rect.W) /2;
				goto case_ALN_NO_FLT;
			case ALN_RIGHT:
				box->Rect.X += blocker->L.width + set_width - box->Rect.W;
				goto case_ALN_NO_FLT;
			case ALN_LEFT:
				box->Rect.X += blocker->L.width;
				goto case_ALN_NO_FLT;
			default: ;
			case_ALN_NO_FLT:
				height += box->Rect.H;
				if (blocker->L.bottom && blocker->L.bottom <= height) {
					blocker->L.bottom = blocker->L.width = 0;
				}
				if (blocker->R.bottom && blocker->R.bottom <= height) {
					blocker->R.bottom = blocker->R.width = 0;
				}
		}
		
		box = box->Sibling;
	}
	if (This->Rect.H < height) {
		This->Rect.H = height;
	}
	if ((This->Rect.H += dombox_BotDist (This)) < This->SetHeight) {
		This->Rect.H = This->SetHeight;
	}
}

/*============================================================================*/
void
dombox_format (DOMBOX * This, long width)
{
	struct blocking_area blocker = { {0, 0}, {0, 0} };
	This->_vtab->format (This, width, &blocker);
	if (This->Rect.H < blocker.L.bottom - This->Rect.Y) {
		 This->Rect.H = blocker.L.bottom - This->Rect.Y;
	}
	if (This->Rect.H < blocker.R.bottom - This->Rect.Y) {
		 This->Rect.H = blocker.R.bottom - This->Rect.Y;
	}
}


/*============================================================================*/
void
dombox_stretch (DOMBOX * This, long height, V_ALIGN valign)
{
	long offset = height - This->Rect.H;
	if (offset > 0) {
		This->Rect.H = height;
		if (valign) {
			DOMBOX * box = This->ChildBeg;
			if (valign == ALN_MIDDLE) offset /= 2;
			while (box) {
				box->Rect.Y += offset;
				box = box->Sibling;
			}
		}
	}
}
