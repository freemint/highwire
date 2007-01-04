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

#define NO_SET_POSITION -2000

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

	/* init the position */
	This->ConBlock = FALSE; /* Set to true if it's a containg block */	
	This->SetPos.Lft = This->SetPos.Rgt = This->SetPos.Top = This->SetPos.Bot = NO_SET_POSITION;

	/* init the border */
	This->HasBorder = FALSE;
	This->BorderWidth.Top = This->BorderWidth.Bot = This->BorderWidth.Lft = This->BorderWidth.Rgt = 0;

	if (parent && parent->FontStk)	{
		This->BorderColor.Top = This->BorderColor.Bot = This->BorderColor.Lft = This->BorderColor.Rgt = parent->FontStk->Color;
	} else {
		This->BorderColor.Top = This->BorderColor.Bot = This->BorderColor.Lft = This->BorderColor.Rgt = G_BLACK;
	}
	This->BorderStyle.Top = This->BorderStyle.Bot = This->BorderStyle.Lft = This->BorderStyle.Rgt = BORDER_NONE;
	
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
	WORD realbkg = This->Backgnd;

	/* style display: none */
	if (This->Hidden) return;
			
	vsl_type (vdi_handle, 0); /* reset the line type */

	if (This->HasBorder) {
		short n;
		PXY p[2];
		short dark1, dark2, light;
		BOOL FlatTop = FALSE;
		BOOL FlatBot = FALSE;

		/* The second half of this sould always happen */
		if ((This->Backgnd == -1)&&(This->Parent)) {
			/* Transparent so find real background */
			DOMBOX *temp_box = This->Parent;
			
			while (temp_box) {
				if (temp_box->Backgnd == -1) {
					temp_box = temp_box->Parent;
				} else {
					realbkg = temp_box->Backgnd;
					break;
				}
			}
		} 

		if (This->BorderStyle.Top > BORDER_HIDDEN) {
			n = This->BorderWidth.Top;

			if (n > 0) {
				vsl_color (vdi_handle, This->BorderColor.Top);

				switch (This->BorderStyle.Top) {
					case BORDER_DOTTED:
						vsl_type (vdi_handle, 3);
						break;
					case BORDER_DASHED:
						vsl_type (vdi_handle, 5);
						break;
					case BORDER_DOUBLE:
						if (n < 3) {
							This->BorderStyle.Top = BORDER_SOLID;
						}
						break;
					case BORDER_INSET:
						vsl_color (vdi_handle, G_LBLACK);
						break;
					case BORDER_OUTSET:
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
						break;
					default:
						vsl_type (vdi_handle, 0);
				}

				p[0].p_x = x1; p[1].p_x = x2;
				p[0].p_y = y1; p[1].p_y = p[0].p_y;

				if (This->BorderStyle.Top == BORDER_DOUBLE) {
					dark1 = (n+1)/3;
					light = n - (dark1 * 2);
					dark2 = dark1;

					while(1) {
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark1) break;
						p[0].p_y++; p[1].p_y++;
					}
					while(1) {
						p[0].p_y++; p[1].p_y++;
						if (!--light) break;
					}		
					while(1) {
						p[0].p_y++; p[1].p_y++;
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark2) break;
					}
				} else {
					if ((This->BoxClass == BC_STRUCT) && (This->BorderStyle.Top == BORDER_INSET)) {
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
						v_pline (vdi_handle, 2, (short*)p);
						n--;
						p[0].p_y++;  p[1].p_y++;
						vsl_color (vdi_handle, G_LBLACK);
					}
										
					while(1) {
						v_pline (vdi_handle, 2, (short*)p);
						if (!--n) break;
						/*b[1].p_y = ++b[0].p_y;  b[3].p_y = --b[2].p_y; these were rounded */
						p[0].p_y++;  p[1].p_y++;
					}
				}
			} else {
				FlatTop = TRUE;
			}
		} else {
			FlatTop = TRUE;
		}

		if (This->BorderStyle.Bot > BORDER_HIDDEN) {
			n = This->BorderWidth.Bot;

			if (n > 0) {
				vsl_color (vdi_handle, This->BorderColor.Bot);

				switch (This->BorderStyle.Bot) {
					case BORDER_DOTTED:
						vsl_type (vdi_handle, 3);
						break;
					case BORDER_DASHED:
						vsl_type (vdi_handle, 5);
						break;
					case BORDER_DOUBLE:
						if (n < 3) {
							This->BorderStyle.Bot = BORDER_SOLID;
						}
						break;
					case BORDER_INSET:
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
						break;
					case BORDER_OUTSET:
						vsl_color (vdi_handle, G_LBLACK);
						break;
					default:
						vsl_type (vdi_handle, 0);
				}

				p[0].p_x = x1; p[1].p_x = x2;
				p[0].p_y = y2; p[1].p_y = p[0].p_y;

				if (This->BorderStyle.Bot == BORDER_DOUBLE) {
					dark1 = (n+1)/3;
					light = n - (dark1 * 2);
					dark2 = dark1;

					while(1) {
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark1) break;
						p[0].p_y--;  p[1].p_y--;
					}
					while(1) {
						p[0].p_y--;  p[1].p_y--;
						if (!--light) break;
					}		
					while(1) {
						p[0].p_y--;  p[1].p_y--;
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark2) break;
					}
				} else {
					if ((This->BoxClass == BC_STRUCT) && (This->BorderStyle.Bot == BORDER_INSET)) {
						vsl_color (vdi_handle, G_LBLACK);
						v_pline (vdi_handle, 2, (short*)p);
						n--;
						p[0].p_y--;  p[1].p_y--;
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
					} 
					
					while(1) {
						v_pline (vdi_handle, 2, (short*)p);
						if (!--n) break;
						/*b[1].p_y = ++b[0].p_y;  b[3].p_y = --b[2].p_y; these were rounded */
						p[0].p_y--;  p[1].p_y--;
					}
				}
			} else {
				FlatBot = TRUE;
			}
		} else {
			FlatBot = TRUE;
		}

		if (This->BorderStyle.Lft > BORDER_HIDDEN) {
			n = This->BorderWidth.Lft;

			if (n > 0) {
				vsl_color (vdi_handle, This->BorderColor.Lft);

				switch (This->BorderStyle.Lft) {
					case BORDER_DOTTED:
						vsl_type (vdi_handle, 3);
						break;
					case BORDER_DASHED:
						vsl_type (vdi_handle, 5);
						break;
					case BORDER_DOUBLE:
						if (n < 3) {
							This->BorderStyle.Lft = BORDER_SOLID;
						}
						break;
					case BORDER_INSET:
						vsl_color (vdi_handle, G_LBLACK);
						break;
					case BORDER_OUTSET:
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
						break;
					default:
						vsl_type (vdi_handle, 0);
				}

				p[0].p_x = x1; p[1].p_x = p[0].p_x;
				p[0].p_y = y1; p[1].p_y = y2;

				if (This->BorderStyle.Bot == BORDER_DOUBLE) {
					dark1 = (n+1)/3;
					light = n - (dark1 * 2);
					dark2 = dark1;

					while(1) {
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark1) break;
						p[0].p_x++;  p[1].p_x++;

						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
					}

					/* be nice to find the parent background color for this */
					/*	vsl_color (vdi_handle, G_WHITE);*/
					vsl_color (vdi_handle, This->Parent->Backgnd);

					while(1) {
						p[0].p_x++;  p[1].p_x++;
						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
						v_pline(vdi_handle, 2, (short*)p);

						if (!--light) break;
					}		

					vsl_color (vdi_handle, This->BorderColor.Lft);

					while(1) {
						p[0].p_x++;  p[1].p_x++;
						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark2) break;
					}
				} else {
					if ((This->BoxClass == BC_STRUCT) && (This->BorderStyle.Lft == BORDER_INSET)) {
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
						v_pline (vdi_handle, 2, (short*)p);
						n--;
						p[0].p_x++;  p[1].p_x++;
						vsl_color (vdi_handle, G_LBLACK);
					}

					while(1) {
						v_pline (vdi_handle, 2, (short*)p);
						if (!--n) break;
						/*b[3].p_x = ++b[0].p_x;  b[1].p_x = --b[2].p_x; these were rounded */
						p[0].p_x++;  p[1].p_x++;
					
						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
					}
				}
			}
		}

		if (This->BorderStyle.Rgt > BORDER_HIDDEN) {
			n = This->BorderWidth.Rgt;

			if (n > 0) {
				vsl_color (vdi_handle, This->BorderColor.Rgt);
	
				switch (This->BorderStyle.Rgt) {
					case BORDER_DOTTED:
						vsl_type (vdi_handle, 3);
						break;
					case BORDER_DASHED:
						vsl_type (vdi_handle, 5);
						break;
					case BORDER_DOUBLE:
						if (n < 3) {
							This->BorderStyle.Rgt = BORDER_SOLID;
						}
						break;
					case BORDER_INSET:
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
						break;
					case BORDER_OUTSET:
						vsl_color (vdi_handle, G_LBLACK);
						break;
					default:
						vsl_type (vdi_handle, 0);
				}

				p[0].p_x = x2; p[1].p_x = p[0].p_x;
				p[0].p_y = y1; p[1].p_y = y2;
	
				if (This->BorderStyle.Bot == BORDER_DOUBLE) {
					dark1 = (n+1)/3;
					light = n - (dark1 * 2);
					dark2 = dark1;

					while(1) {
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark1) break;
						p[0].p_x--;  p[1].p_x--;

						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
					}

					/* be nice to find the parent background color for this */
					/*vsl_color (vdi_handle, G_WHITE);*/
					vsl_color (vdi_handle, This->Parent->Backgnd);

					while(1) {
						p[0].p_x--;  p[1].p_x--;
						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
						v_pline(vdi_handle, 2, (short*)p);
						if (!--light) break;
					}		

					vsl_color (vdi_handle, This->BorderColor.Lft);

					while(1) {
						p[0].p_x--;  p[1].p_x--;
						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
						v_pline(vdi_handle, 2, (short*)p);
						if (!--dark2) break;
					}
				} else {
					if ((This->BoxClass == BC_STRUCT) && (This->BorderStyle.Rgt == BORDER_INSET)) {
						vsl_color (vdi_handle, G_LBLACK);
						v_pline (vdi_handle, 2, (short*)p);
						n--;
						p[0].p_x--;  p[1].p_x--;
						if (realbkg == G_WHITE) {
							vsl_color (vdi_handle, G_LWHITE);
						} else {
							vsl_color (vdi_handle, G_WHITE);
						}
					}

					while(1) {
						v_pline (vdi_handle, 2, (short*)p);
						if (!--n) break;
						/*b[3].p_x = ++b[0].p_x;  b[1].p_x = --b[2].p_x; these were rounded */
						p[0].p_x--;  p[1].p_x--;
						if (!FlatTop) p[0].p_y++;  
						if (!FlatBot) p[1].p_y--;
					}
				}
			}
		}

		x1 += This->BorderWidth.Lft;
		x2 -= This->BorderWidth.Rgt;
		y1 += This->BorderWidth.Top;
		y2 -= This->BorderWidth.Bot;
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

	vsl_type (vdi_handle, 0); /* reset the line type */
	
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
	LONG tempminwidth;

	tempminwidth = This->MinWidth;

	This->MinWidth = (This->SetWidth > 0 ? This->SetWidth : 0);

	/* style display: none */
	if (This->Hidden)
			return This->MinWidth;

	while (box) {
		long width = dombox_MinWidth (box);

		if (This->MinWidth < width) {
			 This->MinWidth = width;
		}
		box = box->Sibling;
	}

	/* This tracks if the size has already been calculated and
	 * tries to keep the system from growing the DOMBOX unnecisarily
	 */
	if (tempminwidth != This->MinWidth) {
		This->MinWidth += dombox_LftDist (This) + dombox_RgtDist (This);
	}
	
	if (This->SetWidth > 0 && This->SetWidth < This->MinWidth) {
		This->SetWidth = This->MinWidth;
	}

/*	if ((This->BoxClass == BC_GROUP) && (This->HtmlCode == 19))
		printf("Min %ld   \r\n",This->MinWidth);
*/

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
	
	/*** devl stuff, activate in cfg: DEVL_FLAGS = CssPosition */
	static int _CssPosition = -1;
	if (_CssPosition < 0) {
		_CssPosition = (devl_flag ("CssPosition") ? TRUE : FALSE);
	}
	
	if (!_CssPosition || (_CssPosition && !cfg_UseCSS)) { /*** old method */
	
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
	
	} else if (_CssPosition && cfg_UseCSS){ /*** new method for CssPosition */
	
	DOMBOX * fnd = NULL;
	
	while (cld) {
		long lft  = cld->Rect.X                  + cld->Margin.Lft;
		long rgt  = cld->Rect.X + cld->Rect.W -1 - cld->Margin.Rgt;
		BOOL in_x = (lft <= x && x <= rgt);
		long top  = cld->Rect.Y                  + cld->Margin.Top;
		long bot  = cld->Rect.Y + cld->Rect.H -1 - cld->Margin.Bot;
		BOOL in_y = (top <= y && y <= bot);
		
		if (in_x && in_y) {
			c_lft = lft + r->X;
			c_rgt = rgt + r->X;
			c_top = top + r->Y;
			c_bot = bot + r->Y;
			fnd   = cld;
		
		} else {
			if (!in_x) {
				if        (x  <  lft) {
					if (c_rgt >= (lft += r->X)) {
						if (in_y) c_rgt = lft -1;
						else      in_x  = TRUE;
					}
				} else  /* x  >  rgt */{
					if (c_lft <= (rgt += r->X)) {
						if (in_y) c_lft = rgt +1;
						else      in_x  = TRUE;
					}
				}
			}
			if (in_x) {
				if        (y  <  top) {
					if (c_bot >= (top += r->Y)) c_bot = top -1;
				} else if (y  >  bot) {
					if (c_top <= (bot += r->Y)) c_top = bot +1;
				}
			}
		}
		cld = cld->Sibling;
	}
	cld = fnd;
	
	} /*** new method */
	
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
	
	c_rgt = (c_lft = r->X = 0) + box->Rect.W -1;
	c_bot = (c_top = r->Y = 0) + box->Rect.H -1;

	while (cld) {
		if (!(box->SetPosCld > 0x100)) {
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
		} /* !(box->SetPosCld > 0x100) */

		if ((cld = box->_vtab->ChildAt (box, r, x, y, clip)) != NULL) {
			r->X += cld->Rect.X;
			r->Y += cld->Rect.Y;
			box  =  cld;
			cld  =  box->ChildBeg;
			x    -= box->Rect.X;
			y    -= box->Rect.Y;
		} 
	}
/*	printf ("%4li,%4li -> %4li,%4li / %4li,%4li ",
	        *px, *py, c_lft, c_rgt, c_top, c_bot);
	if (box->IdName) printf ("%12s", box->IdName);
	else             printf ("%12p", box);
*/
	*px = r->X - (x = *px);
	*py = r->Y - (y = *py);
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
	r->X -= x;
	r->Y -= y;
/*	printf ("   [%li,%li/%li,%li]\n", r->X, r->Y, r->W, r->H);*/
	
	return box;
}


/*----------------------------------------------------------------------------*/
static void
vTab_draw (DOMBOX * This, long x, long y, const GRECT * clip, void * highlight)
{
	DOMBOX * box    = This->ChildBeg;
	long     clip_y = (long)clip->g_y - y;
	
	/* style display: none */
	if (This->Hidden)
			return;

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

	/* style display: none */
	if (This->Hidden)	return;

	if (blocker->L.bottom) {
		if ((blocker->L.width -= This->Margin.Lft) <= 0) {
			blocker->L.bottom = blocker->L.width = 0;
		} else {
			blocker->L.bottom -= This->Rect.Y;
		}
	}

	if (blocker->R.bottom) {
		if ((blocker->R.width -= This->Margin.Rgt) <= 0) {
			blocker->R.bottom = blocker->R.width = 0;
		} else {
			blocker->R.bottom -= This->Rect.Y;
		}
	}
	
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
		BOOL fixed 	   = (box->SetPosMsk > 0x200);
		DOMBOX *parent_box = NULL;

		if (fixed) absolute = FALSE;
		
		box->Rect.X = dombox_LftDist (This);
		box->Rect.Y = height;

		if (absolute) {

			/* I think there needs to be a determination
			 * on if the parent meets certain criteria
			 * If met then we bound on those values
			 * If not then we bound on the page
			 */

			/* containing box for offset calculations */
			if (box->Parent) {
				parent_box = box->Parent;

				while (TRUE) {
					if (parent_box->ConBlock) {
						break;
					} else {
						if (parent_box->Parent) {
							parent_box = parent_box->Parent;
						} else {
							parent_box = NULL;
							break;
						}
					}
				}
				/* we should check for
				 * if (box->real_parent) {
				 * as well, but that was returning
				 * bad values too often 
				 * So we will need to debug that code
				 */		
			} else {
				printf("ERROR: THIS SHOULD NOT HAPPEN !parent_box\r\n");
				parent_box = NULL;
			}

			/*	if (box->SetPosMsk & 0x01)*/
			if (box->SetPos.Lft > NO_SET_POSITION) {
				/* Negative 0x010 or Percentage */
				if (box->SetPos.Lft < 0) {
					if (parent_box) {
						if (box->SetPosMsk & 0x010) {
							box->Rect.X = parent_box->Rect.X + box->SetPos.Lft;
						} else {
							box->Rect.X = (parent_box->Rect.W * -box->SetPos.Lft +512) /1024;				
							set_width = parent_box->Rect.W - box->Rect.X;
						}
					} else {
						if (box->SetPosMsk & 0x010) {
							box->Rect.X = box->SetPos.Lft;
						} else {
							box->Rect.X = (set_width * -box->SetPos.Lft +512) /1024;
						}
					}
				} else {
					if (parent_box) {
						/*box->Rect.X = parent_box->Rect.X + box->SetPos.Lft;*/
						box->Rect.X = parent_box->Padding.Lft + parent_box->Margin.Lft + box->SetPos.Lft;
						set_width = parent_box->Rect.W - box->Rect.X; 
					} else {
						box->Rect.X = box->SetPos.Lft;
					}				
				} 
			}


			/*	if (box->SetPosMsk & 0x02)*/
			if (box->SetPos.Top > NO_SET_POSITION ) {
				if (box->SetPos.Top < 0) {
					if (box->SetPosMsk & 0x030) {
/*printf("negative top %d\r\n",box->SetPos.Top);*/
						box->Rect.Y -= box->SetPos.Top; /* Not exactly correct */
					} else {
						/* There should be an else to handle percentage
						 * values, but that will take some work to implement
						 */
						 ;
/*printf("percentage top %d\r\n",box->SetPos.Top);*/
						box->Rect.Y = parent_box->Rect.Y + (parent_box->Rect.H * -box->SetPos.Top +512) /1024;				
					}
				} else {
					if (parent_box) {
						box->Rect.Y = parent_box->Padding.Top + parent_box->Margin.Top + box->SetPos.Top;
/*printf("parent M Top %d P Top %d   \r\n",parent_box->Margin.Top,parent_box->Padding.Top);*/
					} else {
						box->Rect.Y = box->SetPos.Top;
					}
				}
			}

			floating = FALSE;
		} else if (fixed) {
			/* These need to be offset on the viewport
			 * or in laymans terms the window borders
			 * At the moment they are not
			 *
			 * We need a way to find the window that we are working
			 * in and at the moment I haven't found any easy way of
			 * doing that.
			 */

			if(box->SetPos.Lft > NO_SET_POSITION) {
				box->Rect.X = box->SetPos.Lft;
			}	

			if(box->SetPos.Rgt > NO_SET_POSITION) {
				box->Rect.X = box->SetPos.Rgt;
			}
			
			if (box->SetPos.Top > NO_SET_POSITION ) {
				box->Rect.Y = box->SetPos.Top;
			}

			if (box->SetPos.Bot > NO_SET_POSITION ) {
				box->Rect.Y = box->SetPos.Bot;
			}
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
		} else {
			/* catch relative positions */
			if(box->SetPos.Lft > NO_SET_POSITION) {
				if (box->SetPos.Lft < 0) {
					if (box->SetPosMsk & 0x010) {
						box->Rect.X += box->SetPos.Lft;
					} else {
						box->Rect.X += (box->Rect.W * -box->SetPos.Lft +512) /1024;				
					}
				} else {
					box->Rect.X += box->SetPos.Lft;
				}
			}	

			if(box->SetPos.Rgt > NO_SET_POSITION) {
				if (box->SetPos.Rgt < 0) {
					if (box->SetPosMsk & 0x020) {
						box->Rect.X -= box->SetPos.Rgt;
					} else {
						box->Rect.X -= (box->Rect.W * -box->SetPos.Rgt +512) /1024;				
					}
				} else {
					box->Rect.X -= box->SetPos.Rgt;
				}
			}	

			if(box->SetPos.Top > NO_SET_POSITION) {
				if (box->SetPos.Top < 0) {
					if (box->SetPosMsk & 0x030) {
						box->Rect.Y += box->SetPos.Top;
					} else {
						box->Rect.Y += (box->Rect.H * -box->SetPos.Top +512) /1024;				
					}
				} else {
					box->Rect.Y += box->SetPos.Top;
				}
			}	

			if(box->SetPos.Bot > NO_SET_POSITION) {
				if (box->SetPos.Bot < 0) {
					if (box->SetPosMsk & 0x040) {
						box->Rect.Y -= box->SetPos.Bot;
					} else {
						box->Rect.Y -= (box->Rect.H * -box->SetPos.Bot +512) /1024;				
					}
				} else {
					box->Rect.Y -= box->SetPos.Bot;
				}
			}	
				
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
			/* BUS errors happen sometimes on next line */
			box->_vtab->format (box, set_width, blocker);
		}

		/* these could probably be simplified */
		if (absolute) {
			/* containing box for offset calculations */
			if (box->Parent) {
				parent_box = box->Parent;

				while (TRUE) {
					if (parent_box->ConBlock) {
						break;
					} else {
						if (parent_box->Parent) {
							parent_box = parent_box->Parent;
						} else {
							parent_box = NULL;
							break;
						}
					}
				}

				/* we should check for
				 * if (box->real_parent) {
				 * as well, but that was returning
				 * bad values too often 
				 * So we will need to debug that code
				 */		
			} else {
				parent_box = NULL;
			}

			if (box->SetPos.Rgt > NO_SET_POSITION) {	
				if (box->SetPos.Lft == NO_SET_POSITION) {
					 /* This isn't right.  If we have a Lft then we should extend the width of the object */

					/* Negative 0x020 or Percentage */
					if (box->SetPos.Rgt < 0) {
						if (parent_box) {
							if (box->SetPosMsk & 0x020) {
								box->Rect.X = parent_box->Rect.W + box->SetPos.Rgt;
							} else {
/* printf("Right: Percentage value\r\n"); */
								box->Rect.X = parent_box->Rect.W - ((parent_box->Rect.W * -box->SetPos.Rgt +512) /1024);
							}
						} else {
							if (box->SetPosMsk & 0x020) {
								box->Rect.X = set_width + box->SetPos.Rgt;
							} else {
								box->Rect.X = set_width - ((set_width * -box->SetPos.Rgt +512) /1024);
							}
						}
					} else {
						if (parent_box) {
							/* If we move this to the top then it needs
							 * to watch parent_box.Rect.W
							 */
						 	box->Rect.X = set_width - (box->Rect.W + box->SetPos.Rgt);
							/* box->Rect.X = parent_box->Rect.W - (box->Rect.W + box->SetPos.Rgt);*/
						} else {
							box->Rect.X = set_width - (box->Rect.W + box->SetPos.Rgt);
						}				
					} 
				}
			}

			if (box->SetPos.Bot > NO_SET_POSITION ) {
				if (box->SetPos.Top == NO_SET_POSITION) {
					 /* This isn't right.  If we have a top then we should extend the height of the object */
					/*box->Rect.Y -= (box->SetPos.Bot + box->Rect.H);*/
					box->Rect.Y -= (box->SetPos.Bot + parent_box->Padding.Bot + parent_box->Margin.Bot + box->Rect.H);
				}
			} 
		}
		
		if (!absolute) switch (box->Floating) {
			case FLT_RIGHT:
				box->Rect.X += width - box->Rect.W;

				if (blocker->R.bottom < height + box->Rect.H)
					 blocker->R.bottom = height + box->Rect.H;

				blocker->R.width += box->Rect.W;

				/* The following is bad but stored here as
				 * a clue to the real culprit
				 
				if (box->BoxClass == BC_TABLE) {
					height += box->Rect.H;	
				}
				*/
				goto case_FLT_MASK;
			case FLT_LEFT:
				box->Rect.X += blocker->L.width;
				if (blocker->L.bottom < height + box->Rect.H)
					 blocker->L.bottom = height + box->Rect.H;
				blocker->L.width += box->Rect.W;

				/* The following is bad but stored here as
				 * a clue to the real culprit
				 
				if (box->BoxClass == BC_TABLE) {
					height += box->Rect.H;	
				}
				*/
				
				goto case_FLT_MASK;
			case_FLT_MASK:
				if (This->Rect.H < box->Rect.Y + box->Rect.H) {
					 This->Rect.H = box->Rect.Y + box->Rect.H;
				}

				/* Bug note for links that aren't accessible
				 * for example atari-forums list
				 *
				 * Here we need code similar to what is in 
				 * case_ALN_NO_FLT, but not the same
				 */
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
		} else {
			if (This->Rect.H < box->Rect.Y + box->Rect.H) {
				 This->Rect.H = box->Rect.Y + box->Rect.H;
			}

		#if 0
			/* override the page minwidth in absolute positions */
			if (This->MinWidth < box->Rect.X + box->Rect.W) {
				This->MinWidth = box->Rect.X + box->Rect.W;
			}
		#endif
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

	/* style display: none */
	if (This->Hidden) return;

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

	/* style display: none */
	if (This->Hidden)
			return;

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
