/* @(#)highwire/Containr.c
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gem.h>

typedef struct s_history_priv * HISTORY;
#define HISTORY HISTORY_public
#ifdef __PUREC__
# define CONTAINR struct s_containr *
# undef  HISTORY
# define HISTORY struct s_history_priv *
#endif

#include "global.h"
#include "Form.h"
#include "schedule.h"
#include "Location.h"
#ifdef __PUREC__
# undef CONTAINR
# undef  HISTORY
# define HISTORY HISTORY_public
#endif
#include "Containr.h"


typedef struct {
	char      * Name;
	WORD        Size; /* width or height, depends on parent */
	WORD        BorderSize;
	WORD        BorderColor;
	SCROLL_CODE Scroll;
	union {
		WORD     ChildNum;
		ENCODING Encoding;
	}           u;
	LOCATION    Location;
} HISTITEM;

struct s_hist_title {
	UWORD _reffs;
	char  Text[1];
};

struct s_history_priv {
	struct s_history      Entry;
	struct s_hist_title * Title;
	UWORD    Count;
	UWORD    Frames;
	HISTITEM List[1];
};
#define HIST_MAX sizeof(((HISTORY)0)->Entry.Text)


#define PERCENTof(p,n) (((long)n * p + 512) /1024)

void containr_debug (CONTAINR);


/*==============================================================================
 */
CONTAINR
new_containr (CONTAINR parent)
{
	CONTAINR cont = calloc (1, sizeof (struct s_containr));

	if (parent == NULL) {
		cont->Base    = cont;
		cont->ColSize = -1024;
		cont->RowSize = -1024;
		cont->Name    = strdup ("_top");
		cont->BorderSize  = BORDER_SIZE;
		cont->BorderColor = BORDER_COLOR;
	} else {
		cont->Base    = parent->Base;
		cont->Parent  = parent;
		cont->BorderSize  = parent->BorderSize;
		cont->BorderColor = parent->BorderColor;
		cont->Handler = parent->Handler;
		cont->HdlrArg = parent->HdlrArg;
	}
	cont->Resize = TRUE;
	
	return cont;
}


/*============================================================================*/
void
delete_containr (CONTAINR *p_cont)
{
	CONTAINR cont = *p_cont;
	if (cont) {
		containr_clear(cont);
		if (cont->Name) {
			free (cont->Name);
		}
		free(cont);
		*p_cont = NULL;
	}
}


/*==============================================================================
 * create children
 */
void
containr_fillup (CONTAINR parent, const char * text, BOOL colsNrows)
{
	CONTAINR * ptr = &parent->u.Child;

	/* go through the text list and create children
	 */
	while (*text) {
		short val = -10001; /* for either a '*', '1*' or an inalid value */
		while (isspace(*text)) text++;
		if (*text != '*') {
			char * tail = "";
			long   tmp  = strtol (text, &tail, 10);
			if (tmp > 0 && tmp < 10000) {
				if (*tail == '%') {
					val = -tmp;
				} else if (*tail == '*') {
					val -= tmp -1;
				} else { /* absolute value */
					val = tmp + parent->BorderSize;
				}
			}
			text = tail;
		}
		*ptr = new_containr (parent);
		if (colsNrows) {
			(*ptr)->ColSize = val;
			(*ptr)->RowSize = (parent->RowSize > 0 ? parent->RowSize : -1024);
		} else {
			(*ptr)->ColSize = (parent->ColSize > 0 ? parent->ColSize : -1024);
			(*ptr)->RowSize = val;
		}
		ptr = &(*ptr)->Sibling;
		while (*text && *(text++) != ',');
	}
	
	if (parent->u.Child) {
		parent->Mode = (colsNrows ? CNT_CLD_H : CNT_CLD_V);
	}
}

/*==============================================================================
 */
CONTAINR
containr_resume (CONTAINR last)
{
	CONTAINR parent = last->Parent;
	CONTAINR cont;
	BOOL     colsNrows;
	
	short perc_cnt = 0; /* number of percent values */
	long  perc_sum = 0; /* sum of all percents */
	short frac_cnt = 0; /* number of all '*','2*',.. and invalid values */
	long  frac_sum = 0; /* sum of fractionals */
	
	if (!parent) {
		return NULL;
	
	} else if (last->Mode) {             /* find the first empty child, if any */
		while ((last = last->Sibling) != NULL && last->Mode);
	
	} else if (last == parent->u.Child) { /* all childs are empty */
		parent->Mode    = CNT_EMPTY;
		parent->u.Child = NULL;
	}
	
	colsNrows = (parent->Mode == CNT_CLD_H);
	cont      =  parent->u.Child;
	while (cont) {
		short val = -(colsNrows ? cont->ColSize : cont->RowSize);
		if (val > 0) {
			if (val >= 10000) {
				frac_sum += val - 10000;
				frac_cnt++;
			} else {
				perc_sum += val;
				perc_cnt++;
			}
		}
		if (cont->Sibling == last) {
			cont->Sibling = NULL;
		}
		cont = cont->Sibling;
	}
	while (last) {
		CONTAINR next = last->Sibling;
		delete_containr (&last);
		last = next;
	}
	
	/* post process the percent and fractional sizes */
	if (frac_cnt) {
		short frac;
		if (perc_sum <= 100/*%*/) {
			frac = 100 - perc_sum;
		} else {
			frac = (perc_sum * frac_cnt) / perc_cnt;
		}
		frac = (frac > frac_sum ? frac / frac_sum : 1);
		perc_sum += frac_sum * frac;
		perc_cnt += frac_cnt;
		frac_sum =  frac; /* is now multiplicator for fractionals */
	}
	if (perc_cnt) {
		long  half = (perc_sum /*-1*/) /2;
		short perc = 1024;
		cont = parent->u.Child;
		do {
			short * var = (colsNrows ? &cont->ColSize : &cont->RowSize);
			short   val = -*var;
			if (val >= 0) {
				if (!--perc_cnt) {
					*var = -perc;
					break;
				}
				if (val > 10000) {
					val = (val - 10000) * frac_sum;
				}
				perc += *var = -(((long)val * 1024 + half) / perc_sum);
			}
		} while ((cont = cont->Sibling) != NULL);
	}
	
	return parent;
}

/*==============================================================================
 */
void
containr_setup (CONTAINR cont, FRAME frame, const char * anchor)
{
	if (cont->Mode) {
		printf ("containr_setup (%p): has mode %i!\n", cont, cont->Mode);
	
	} else if (!frame) {
		printf ("containr_setup (%p): no frame!\n", cont);
	
	} else {
		GRECT area = cont->Area;
		cont->Mode    = CNT_FRAME;
		cont->u.Frame = frame;
		frame->Container = cont;
		frame->scroll    = cont->Scroll;
		if (cont->BorderSize && cont->Sibling) {
			if (cont->Parent->Mode == CNT_CLD_H) {
				area.g_w -= cont->BorderSize;
			} else {            /* == CNT_CLD_V */
				area.g_h -= cont->BorderSize;
			}
		}
		frame_calculate (frame, &area);
		if (anchor) {
			long dx, dy;
			if (containr_Anchor (cont, anchor, &dx, &dy)) {
				containr_shift (cont, &dx, &dy);
			}
		}
	}
}

/*==============================================================================
 */
void
containr_clear (CONTAINR cont)
{
	int depth = 0;
	
	while (cont) {
		if (cont->Mode == CNT_FRAME) {
			if (cont->u.Frame) {
				if (cont->Handler) {
					(*cont->Handler)(HW_PageCleared, cont->HdlrArg, cont, NULL);
				}
				delete_frame (&cont->u.Frame);
			}
		} else if (cont->Mode) {
			if (cont->u.Child) {
				cont = cont->u.Child;
				depth++;
				continue;
			}
		}
		sched_clear ((long)cont);
		
		if (!depth) {
			cont->Mode        = CNT_EMPTY;
			cont->BorderSize  = (cont->Parent
			                     ? cont->Parent->BorderSize  : BORDER_SIZE);
			cont->BorderColor = (cont->Parent
			                     ? cont->Parent->BorderColor : BORDER_COLOR);
			break;
		
		} else {
			CONTAINR next = cont->Sibling;
			if (cont->Parent) {
				cont->Parent->u.Child = next;
			}
			if (!next) {
				next = cont->Parent;
				depth--;
			}
			if (cont->Name) free (cont->Name);
			free (cont);
			cont = next;
		}
	}
}

/*==============================================================================
 */
void
containr_register (CONTAINR cont, CNTR_HDLR hdlr, long arg)
{
	if (cont) {
		cont->Handler = hdlr;
		cont->HdlrArg = arg;
	}
}

/*============================================================================*/
void *
containr_highlight (CONTAINR cont, void * highlight)
{
	CONTAINR base = cont->Base;
	void   * old  = base->HighLight;
	
	base->HighLight = highlight;
	
	return old;
}


/*----------------------------------------------------------------------------*/
static HISTITEM *
create_r (CONTAINR cont, HISTITEM * item, BOOL h_N_v)
{
	HISTITEM * next = item +1;
	item->Size        = (h_N_v ? cont->ColSize : cont->RowSize);
	item->BorderSize  = cont->BorderSize;
	item->BorderColor = cont->BorderColor;
	item->Scroll      = cont->Scroll;
	item->Location    = NULL;
	item->u.ChildNum  = 0;
	
	if (cont->Mode == CNT_FRAME) {
		if (cont->u.Frame) {
			item->Location   = location_share (cont->u.Frame->Location);
			item->u.Encoding = cont->u.Frame->Encoding;
		}
	
	} else if (cont->Mode && cont->u.Child) {
		CONTAINR chld = cont->u.Child;
		WORD     inc;
		h_N_v = (cont->Mode == CNT_CLD_H);
		inc   = (h_N_v ? +1 : -1);
		do {
			next->Name = (chld->Name ? strdup (chld->Name) : NULL);
			next = create_r (chld, next, h_N_v);
			item->u.ChildNum += inc;
		} while ((chld = chld->Sibling) != NULL);
	}
	
	return next;
}

/*----------------------------------------------------------------------------*/
static struct s_hist_title *
hist_title (const char * text)
{
	size_t                size  = strlen (text);
	struct s_hist_title * title = malloc (sizeof (struct s_hist_title) + size);
	title->_reffs = 1;
	memcpy (title->Text, text, size +1);
	
	return title;
}

/*============================================================================*/
HISTORY
history_create (CONTAINR cont, const char * name, HISTORY prev)
{
	HISTORY  hist;
	CONTAINR base = cont->Base;
	BOOL     sub  = (cont->Parent != NULL);
	UWORD cnt_all = 0;
	UWORD cnt_frm = 0;
	BOOL  b       = TRUE;
	
	if (!name || !*name) {
		FRAME frame = containr_Frame (cont);
		name = (frame ? frame->Location->FullName : NULL);
	}
	
	/* count number of containers */
	cont = cont->Base;
	b    = TRUE;
	do {
		if (b) {
			cnt_all++;
			if (cont->Mode == CNT_FRAME) {
				if (cont->u.Frame) {
					cnt_frm++;
				}
			} else if (cont->Mode && cont->u.Child) {
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
	} while (cont);
	
	hist = malloc (sizeof (struct s_history_priv) +
	               sizeof (HISTITEM) * (cnt_all -1));
	hist->Count      = cnt_all;
	hist->Frames     = cnt_frm;
	hist->List->Name = NULL;
	create_r (base, hist->List, TRUE);
	
	if (prev && prev->Title) {
		hist->Title = prev->Title;
		hist->Title->_reffs++;
	} else if (name) {
		hist->Title = hist_title (name);
	} else {
		hist->Title = NULL;
	}
	
	if (!name) {
		name = "???";
	}
	strcpy (hist->Entry.Text, "  ");
	hist->Entry.Length = strlen (name) +2;
	if (sub) {
		strcat (hist->Entry.Text, "ù ");
		hist->Entry.Length += 2;
	}
	if (hist->Entry.Length < HIST_MAX) {
		strcat (hist->Entry.Text, name);
	} else {
		size_t n = strlen (hist->Entry.Text);
		memcpy (hist->Entry.Text + n, name, HIST_MAX -1 - n);
		hist->Entry.Text[HIST_MAX -1] = '\0';
		hist->Entry.Length = HIST_MAX;
	}
	
	return hist;
}


/*============================================================================*/
void history_destroy (HISTORY * _hst)
{
	HISTORY hist = *_hst;
	if (hist) {
		HISTITEM * item  = hist->List;
		UWORD      count = hist->Count;
		do {
			if (item->Location) {
				free_location (&item->Location);
			}
			if (item->Name) {
				free (item->Name);
			}
			item++;
		} while (--count);
		if (hist->Title && !--hist->Title->_reffs) {
			free (hist->Title);
		}
		free (hist);
		*_hst = NULL;
	}
}


/*----------------------------------------------------------------------------*/
static HISTITEM *
process_r (CONTAINR cont, HISTITEM * item, HISTENTR ** entr, UWORD * count)
{
	HISTITEM * next = item +1;
	
	cont->BorderSize  = item->BorderSize;
	cont->BorderColor = item->BorderColor;
	cont->Scroll      = item->Scroll;
	
	if (item->Location) {
		(*entr)->Target   = cont;
		(*entr)->Location = item->Location;
		(*entr)->Encoding = item->u.Encoding;
		(*entr)++;
		(*count)--;
	
	} else if (item->u.ChildNum) {
		CONTAINR * ptr = &cont->u.Child;
		WORD num   = (item->u.ChildNum < 0 ? -item->u.ChildNum
		                                   : +item->u.ChildNum);
		BOOL h_N_v = (item->u.ChildNum == num);
		cont->Mode = (h_N_v ? CNT_CLD_H : CNT_CLD_V);
		do {
			*ptr = new_containr (cont);
			if (next->Name) {
				(*ptr)->Name = strdup (next->Name);
			}
			if (h_N_v) {
				(*ptr)->ColSize = next->Size;
				(*ptr)->RowSize = (cont->RowSize > 0 ? cont->RowSize : -1024);
			} else {
				(*ptr)->ColSize = (cont->ColSize > 0 ? cont->ColSize : -1024);
				(*ptr)->RowSize = next->Size;
			}
			next = process_r (*ptr, next, entr, count);
			ptr = &(*ptr)->Sibling;
		} while (--num);
	}
	
	return next;
}

/*============================================================================*/
UWORD
containr_process (CONTAINR cont, HISTORY hist, HISTORY prev,
                  HISTENTR entr[], UWORD max_num)
{
	UWORD count = max_num;
	
	cont = cont->Base;
	
	containr_clear  (cont);
	if (prev && prev->Title != hist->Title) {
		containr_notify (cont, HW_SetTitle,
		                 (hist->Title ? hist->Title->Text : hist->Entry.Text +2));
	}
	process_r (cont, hist->List, &entr, &count);
	containr_calculate (cont, NULL);
	containr_notify (cont, HW_PageUpdated, &cont->Area);
	
	return (max_num - count);
}


/*============================================================================*/
CONTAINR
containr_byName (CONTAINR cont, const char * name)
{
	BOOL b = TRUE;
	
	if (!cont || !name || !*name || stricmp (name, "_self") == 0) {
		return cont;
	
	} else if (stricmp (name, "_parent") == 0) {
		/*return (cont->Parent ? cont->Parent : cont);*/
		/*
		 * '_parent' means the parent of a whole frameset and not of this
		 * particular one.  this will fail if a frameset includes a file with
		 * another frameset -- but who cares...
		*/
		return cont->Base;
	}
	
	cont = cont->Base;
	do {
		if (b) {
			if (cont->Name && stricmp (name, cont->Name) == 0) {
				return cont;
			
			} else if (cont->Mode > CNT_FRAME && cont->u.Child) {
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
	} while (cont);
	
	return NULL;
}

/*============================================================================*/
CONTAINR
containr_byCoord (CONTAINR cont, short x, short y)
{
	if (!cont) {
		return NULL;
	
	} else {
		cont = cont->Base;
	}
	if (x < cont->Area.g_x || x >= cont->Area.g_x + cont->Area.g_w ||
	    y < cont->Area.g_y || y >= cont->Area.g_y + cont->Area.g_h) {
		return NULL;
	}
	while (cont->Mode > CNT_FRAME) {
		CONTAINR child = cont->u.Child;
		if (cont->Mode == CNT_CLD_H) {
			if (cont->BorderSize && cont->Sibling) {
				short bot = cont->Area.g_y + cont->Area.g_h;
				if (y < bot && y >= bot - cont->BorderSize) break;
			}
			while (child && x >= child->Area.g_x + child->Area.g_w) {
				child = child->Sibling;
			}
		} else {   /* == CNT_CLD_V */
			if (cont->BorderSize && cont->Sibling) {
				short rgt = cont->Area.g_x + cont->Area.g_w;
				if (x < rgt && x >= rgt - cont->BorderSize) break;
			}
			while (child && y >= child->Area.g_y + child->Area.g_h) {
				child = child->Sibling;
			}
		}
		if (child) cont = child;
		else       break;
	}
	
	return cont;
}

/*============================================================================*/
BOOL
containr_Anchor (CONTAINR cont, const char * anchor, long * dx, long * dy)
{
	FRAME frame = (cont && cont->Mode == CNT_FRAME ? cont->u.Frame : NULL);
	BOOL  rtn   = FALSE;
	
	if (frame) {
		OFFSET * offs = frame_anchor (frame, anchor);
		long     x    = 0;
		long     y    = 0;
		while (offs) {
			x += offs->X;
			y += offs->Y;
			offs = offs->Origin;
		}
		if ((*dx = x - frame->h_bar.scroll) != 0) rtn = TRUE;
		if ((*dy = y - frame->v_bar.scroll) != 0) rtn = TRUE;
	} else {
		*dx = 0;
		*dy = 0;
	}
	return rtn;
}

/*============================================================================*/
UWORD
containr_Element (CONTAINR *_cont, short x, short y,
                  GRECT * watch, GRECT * clip, void ** hash)
{
	CONTAINR cont = containr_byCoord (*_cont, x, y);
	UWORD    type = PE_NONE;
	FRAME    frame;
	short    area_rgt, area_bot;
	
	if (hash) *hash = NULL;
	
	if (!cont) {
		if (*_cont) {
			cont = (*_cont)->Base;
			*_cont = cont;
			*watch = cont->Area;
		} else {
			watch->g_x = x;
			watch->g_y = y;
			watch->g_w = watch->g_h = 1;
		}
		return PE_NONE;
	}
	
	*_cont = cont;
	
	area_rgt = cont->Area.g_x + cont->Area.g_w;
	area_bot = cont->Area.g_y + cont->Area.g_h;
	
	if (cont->BorderSize && cont->Sibling) {
		if (cont->Parent->Mode == CNT_CLD_H) {
			short rgt = area_rgt;
			area_rgt -= cont->BorderSize;
			if (x < rgt && x >= area_rgt) {
				*watch = cont->Area;
				watch->g_x = area_rgt;
				watch->g_w = cont->BorderSize;
				type = PE_BORDER_RT;
			}
		} else {            /* == CNT_CLD_V */
			short bot = area_bot;
			area_bot -= cont->BorderSize;
			if (y < bot && y >= area_bot) {
				*watch = cont->Area;
				watch->g_y = area_bot;
				watch->g_h = cont->BorderSize;
				type = PE_BORDER_DN;
			}
		}
		if (type) return (cont->Resize ? type : PE_FRAME);
	}
	
	if ((frame = containr_Frame (cont)) == NULL) {
		*watch = cont->Area;
		if (cont->BorderSize && cont->Sibling) {
			if (cont->Parent->Mode == CNT_CLD_H)  watch->g_w -= cont->BorderSize;
			else                /* == CNT_CLD_V*/ watch->g_h -= cont->BorderSize;
		}
		return PE_EMPTY;
	}
	
	if (x >= (watch->g_x = frame->clip.g_x + frame->clip.g_w)) {
		watch->g_w = area_rgt - watch->g_x;
	} else {
		watch->g_w = 0;
	}
	if (y >= (watch->g_y = frame->clip.g_y + frame->clip.g_h)) {
		watch->g_h = area_bot - watch->g_y;
	} else {
		watch->g_h = 0;
	}
	
	if (watch->g_w && watch->g_h) {
		type = PE_FRAME;
	
	} else if (watch->g_w && !watch->g_h) {
		watch->g_y = frame->clip.g_y;
		watch->g_h = frame->clip.g_h;
		type = PE_VBAR;
	
	} else if (!watch->g_w && watch->g_h) {
		watch->g_x = frame->clip.g_x;
		watch->g_w = frame->clip.g_w;
		type = PE_HBAR;
	
	} else {
		long     area[4];
		PARAGRPH par = frame_paragraph (frame, x, y, area);
		if (!par) {
			type = PE_FRAME;
		
		} else {
			WORDITEM word;
			
			x -= (long)frame->clip.g_x + area[0];
			y -= (long)frame->clip.g_y + area[1];
			if ((word = paragrph_word (par, x, y, area)) == NULL) {
				type = PE_PARAGRPH;
			
			} else if (word->input) {
				type = (input_isEdit (word->input) ? PE_EDITABLE : PE_INPUT);
				if (hash) {
					*hash = word->input;
				}
			
			} else if (!word->link || !word->link->isHref) {
				type = PE_TEXT;
			
			} else if (word->image) {
				type = PE_ILINK;
				if (hash) {
					*hash = word->link;
				}
			
			} else {
				type = PE_TLINK;
				if (hash) {
					*hash = word->link;
				}
				if (clip) {
					*clip = paragraph_extend (word);
					clip->g_x += area[0] + frame->clip.g_x;
					clip->g_y += area[1] + frame->clip.g_y;
					rc_intersect (&frame->clip, clip);
				}
			}
		}
		
		/* clip long array into struct of shorts */
		
		*(PXY*)watch = *(PXY*)&frame->clip; /*copy x/y at once */
		
		if (area[0] < 0) {
			area[2] += area[0];
		} else {
			watch->g_x += area[0];
		}
		if (watch->g_x + area[2] > frame->clip.g_x + frame->clip.g_w) {
			watch->g_w = frame->clip.g_x + frame->clip.g_w - watch->g_x;
		} else {
			watch->g_w = area[2];
		}
		
		if (area[1] < 0) {
			area[3] += area[1];
		} else {
			watch->g_y += area[1];
		}
		if (watch->g_y + area[3] > frame->clip.g_y + frame->clip.g_h) {
			watch->g_h = frame->clip.g_y + frame->clip.g_h - watch->g_y;
		} else {
			watch->g_h = area[3];
		}
		
		if (clip &&type != PE_TLINK) {
			*clip = *watch;
		}
	}
	
	return type;
}


/*============================================================================*/
BOOL
containr_relocate (CONTAINR cont, short new_x, short new_y)
{
	short dx, dy;
	BOOL  b = TRUE;
	
	if (!cont || cont->Parent) return FALSE;
	
	dx = new_x - cont->Area.g_x;
	dy = new_y - cont->Area.g_y;
	if (!dx && !dy) return FALSE;
	
	do {
		if (b) {
			cont->Area.g_x += dx;
			cont->Area.g_y += dy;
			if (cont->Mode == CNT_FRAME) {
				if (cont->u.Frame) {
					cont->u.Frame->clip.g_x += dx;
					cont->u.Frame->clip.g_y += dy;
				}
			} else if (cont->Mode && cont->u.Child) {
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
	} while (cont);
	
	return TRUE;
}

/*============================================================================*/
BOOL
containr_calculate (CONTAINR cont, const GRECT * p_rect)
{
	int  depth = 0;
	BOOL b     = TRUE;
	
	if (!cont) return FALSE;
	
	if (p_rect) {   /* we got a new area to set */
		
		if (cont->Parent) {
			/*
			 * absolute coordinates are allowed only for base container
			 */
			return FALSE;
		}
		cont->Area = *p_rect;
	
	} else while (cont->Area.g_w <= 0 || cont->Area.g_h <= 0) {
		/*
		 * find a container with a valid size
		 */
		cont = cont->Parent;
		if (!cont) return FALSE;
	}
	
	do {
		if (b) {
			if (cont->Mode == CNT_FRAME) {
				if (cont->u.Frame) {
					GRECT area = cont->Area;
					if (cont->BorderSize && cont->Sibling) {
						if (cont->Parent->Mode == CNT_CLD_H) {
							area.g_w -= cont->BorderSize;
						} else {            /* == CNT_CLD_V */
							area.g_h -= cont->BorderSize;
						}
					}
					frame_calculate (cont->u.Frame, &area);
				}
			} else if (cont->Mode && cont->u.Child) {
				CONTAINR cld = cont->u.Child;
				short    x   = cont->Area.g_x;
				short    y   = cont->Area.g_y;
				short    w   = cont->Area.g_w;
				short    h   = cont->Area.g_h;
				short    n   = 0;
				short    frc;
				if (cont->Mode == CNT_CLD_H) {
					if (cont->Sibling) {
						h -= cont->BorderSize;
					}
					do {
						if (cld->ColSize > 0) w -= cld->ColSize;
						else                  n++;
					} while ((cld = cld->Sibling) != NULL);
					frc = w;
				} else {
					if (cont->Sibling) {
						w -= cont->BorderSize;
					}
					do {
						if (cld->RowSize > 0) h -= cld->RowSize;
						else                  n++;
					} while ((cld = cld->Sibling) != NULL);
					frc = h;
				}
				cld = cont->u.Child;
				do {
					cld->Area.g_x = x;
					cld->Area.g_y = y;
					if (cont->Mode == CNT_CLD_H) {
						short s;
						if (cld->ColSize > 0) {
							s = cld->ColSize;
						} else if (!--n) {
							s = frc;
						} else {
							s   =  PERCENTof(-cld->ColSize, w);
							frc -= s;
						}
						cld->Area.g_w = s;
						cld->Area.g_h = h;
						x            += s;
					} else { /* CNT_CLD_V */
						short s;
						if (cld->RowSize > 0) {
							s = cld->RowSize;
						} else if (!--n) {
							s = frc;
						} else {
							s   =  PERCENTof(-cld->RowSize, h);
							frc -= s;
						}
						cld->Area.g_h = s;
						cld->Area.g_w = w;
						y            += s;
					}
				} while ((cld = cld->Sibling) != NULL);
				cont = cont->u.Child;
				depth++;
				continue;
			}
		}
		if (depth) {
			if (cont->Sibling) {
				cont = cont->Sibling;
				b    = TRUE;
				continue;
			}
			cont = cont->Parent;
			b    = FALSE;
			depth--;
		}
	} while (depth);
	
/*	containr_debug (cont);*/
	
	return TRUE;
}

/*============================================================================*/
BOOL
containr_shift (CONTAINR cont, long * dx, long * dy)
{
	FRAME frame = (cont && cont->Mode == CNT_FRAME ? cont->u.Frame : NULL);
	BOOL  rtn   = FALSE;
	
	if (frame) {
		if (!frame->h_bar.on) {
			*dx = 0;
		} else {
			long s = frame->Page.Width - frame->clip.g_w;
			long x = frame->h_bar.scroll + *dx;
			if (x > s) x = s;
			if (x < 0) x = 0;
			if ((*dx = x - frame->h_bar.scroll) != 0) {
				frame->h_bar.scroll = x;
				frame_slider (&frame->h_bar, s);
				rtn = TRUE;
			}
		}
		if (!frame->v_bar.on) {
			*dy = 0;
		} else {
			long s = frame->Page.Height - frame->clip.g_h;
			long y = frame->v_bar.scroll + *dy;
			if (y > s) y = s;
			if (y < 0) y = 0;
			if ((*dy = y - frame->v_bar.scroll) != 0) {
				frame->v_bar.scroll = y;
				frame_slider (&frame->v_bar, s);
				rtn = TRUE;
			}
		}
	} else {
		*dx = 0;
		*dy = 0;
	}
	
	return rtn;
}


/*============================================================================*/
void
containr_redraw (CONTAINR cont, const GRECT * p_clip)
{
	void * highlight;
	GRECT  clip;
	BOOL   mouse = FALSE;
	int    depth = 0;
	BOOL   b     = TRUE;

	if (!cont) return;
	
	highlight = cont->Base->HighLight;
	clip      = cont->Area;
	
	if (clip.g_w <= 0 || clip.g_h <= 0
	    || (p_clip && !rc_intersect (p_clip, &clip))) return;
	
	do {
		if (b) {
			GRECT area = cont->Area;
			if (rc_intersect (&clip, &area)) {
				BOOL clipped;
				PXY  p[4];
				WORD brd_w = 1, brd_h = 1;
				
				if (cont->BorderSize && cont->Sibling) {
					BOOL h_not_v = (cont->Mode <= CNT_FRAME
					                ? cont->Parent->Mode == CNT_CLD_V
					                : cont->Mode         == CNT_CLD_H);
					p[1].p_x = (p[0].p_x = area.g_x) + area.g_w -1;
					p[1].p_y = (p[0].p_y = area.g_y) + area.g_h -1;
					vs_clip_pxy (vdi_handle, p);
					clipped = TRUE;
					p[2].p_x = cont->Area.g_x + cont->Area.g_w -1;
					p[2].p_y = cont->Area.g_y + cont->Area.g_h -1;
					if (h_not_v) { /* horizontal border */
						p[3].p_x = p[1].p_x = cont->Area.g_x;
						p[0].p_y = p[1].p_y = p[2].p_y - cont->BorderSize +1;
						p[0].p_x = p[2].p_x;
						p[3].p_y = p[2].p_y;
						brd_h   += cont->BorderSize;
					} else {       /* vertical border */
						p[0].p_x = p[1].p_x = p[2].p_x - cont->BorderSize +1;
						p[3].p_y = p[1].p_y = cont->Area.g_y;
						p[3].p_x = p[2].p_x;
						p[0].p_y = p[2].p_y;
						brd_w   += cont->BorderSize;
					}
					if (!mouse) {
						v_hide_c (vdi_handle);
						mouse = TRUE;
					}
					vsf_color (vdi_handle, cont->BorderColor);
					v_bar (vdi_handle, (short*)(p +1));
					if (cont->BorderSize > 3) {
						vsl_color (vdi_handle, G_WHITE);
						v_pline (vdi_handle, 2, (short*)(p +0));
						vsl_color (vdi_handle, G_LBLACK);
						v_pline (vdi_handle, 2, (short*)(p +2));
					}
				} else {
					clipped = FALSE;
				}
				
				if (cont->Mode > CNT_FRAME && cont->u.Child) {
					cont = cont->u.Child;
					depth++;
					continue;
				}
				
				if (!clipped) {
					p[1].p_x = (p[0].p_x = area.g_x) + area.g_w -1;
					p[1].p_y = (p[0].p_y = area.g_y) + area.g_h -1;
					vs_clip_pxy (vdi_handle, p);
					if (!mouse) {
						v_hide_c (vdi_handle);
						mouse = TRUE;
					}
				}
				if (cont->Mode == CNT_FRAME && cont->u.Frame) {
					
					frame_draw (cont->u.Frame, &area, highlight);
					
				} else {
					GRECT r = cont->Area;
					
					r.g_w += r.g_x - brd_w;
					r.g_h += r.g_y - brd_h;
					
					vsf_perimeter (vdi_handle, PERIMETER_ON);
					vsf_interior  (vdi_handle, FIS_PATTERN);
					vsf_style     (vdi_handle, 16);
					vsf_color     (vdi_handle, (cont->Parent ? G_LWHITE : G_RED));
					vswr_mode     (vdi_handle, MD_REPLACE);
					
					v_bar (vdi_handle, (short*)&r);
					
					vsf_perimeter (vdi_handle, PERIMETER_OFF);
					vsf_interior  (vdi_handle, FIS_SOLID);
					vswr_mode     (vdi_handle, MD_TRANS);
				}
			}
		}
		if (depth) {
			if (cont->Sibling) {
				cont = cont->Sibling;
				b    = TRUE;
				continue;
			}
			cont = cont->Parent;
			b    = FALSE;
			depth--;
		}
	} while (depth);
	
	if (mouse) {
		v_show_c (vdi_handle, 1);
	}
	vs_clip_off (vdi_handle);
}

/*============================================================================*/
void
containr_scroll (CONTAINR cont, const GRECT * clip, long dx, long dy)
{
	void * highlight = cont->Base->HighLight;
	FRAME  frame     = cont->u.Frame;
	GRECT  rect      = frame->clip;
	BOOL   inside    = rc_intersect (clip, &rect);
	long   ax        = (dx < 0 ? -dx : +dx);
	long   ay        = (dy < 0 ? -dy : +dy);
	
	if (ax >= rect.g_w || ay >= rect.g_h) {
		rect = *clip;
		rect.g_w += rect.g_x -1;
		rect.g_h += rect.g_y -1;
		vs_clip_pxy (vdi_handle, (PXY*)&rect);
		frame_draw (frame, clip, highlight);
		
	} else {
		if (inside) {
			GRECT hbox, vbox;
			MFDB  mfdb = { NULL, };
			PXY src[4], * dst = src +2;
			src[1].p_x = (src[0].p_x = rect.g_x) + rect.g_w -1;
			src[1].p_y = (src[0].p_y = rect.g_y) + rect.g_h -1;
			dst[0] = src[0];
			dst[1] = src[1];
			
			if (ax) {
				hbox = rect;
				if (dx < 0) {              /* scroll right */
					src[1].p_x -= ax;
					dst[0].p_x += ax;
				} else {                   /* scroll left */
					src[0].p_x += ax;
					dst[1].p_x -= ax;
					hbox.g_x   =  dst[1].p_x +1;
				}
				hbox.g_w = ax;
			}
			if (ay) {
				vbox = rect;
				if (dy < 0) {              /* scroll down */
					src[1].p_y -= ay;
					dst[0].p_y += ay;
					hbox.g_y   =  dst[0].p_y;
				} else {                   /* scroll up */
					src[0].p_y += ay;
					dst[1].p_y -= ay;
					vbox.g_y   =  dst[1].p_y +1;
				}
				vbox.g_h =  ay;
				hbox.g_h -= ay;
			}
			v_hide_c  (vdi_handle);
			vro_cpyfm (vdi_handle, S_ONLY, (short*)src, &mfdb, &mfdb);
			v_show_c  (vdi_handle, 1);
			
			if (ax) {
				src[1].p_x = (src[0].p_x = hbox.g_x) + hbox.g_w -1;
				src[1].p_y = (src[0].p_y = hbox.g_y) + hbox.g_h -1;
				vs_clip_pxy (vdi_handle, src);
				frame_draw (frame, &hbox, highlight);
			}
			if (ay) {
				src[1].p_x = (src[0].p_x = vbox.g_x) + vbox.g_w -1;
				src[1].p_y = (src[0].p_y = vbox.g_y) + vbox.g_h -1;
				vs_clip_pxy (vdi_handle, src);
				frame_draw (frame, &vbox, highlight);
			}
		}
		if (dx) {
			rect = frame->clip;
			rect.g_y += frame->clip.g_h;
			if (rc_intersect (clip, &rect)) {
				rect.g_w += rect.g_x -1;
				rect.g_h += rect.g_y -1;
				vs_clip_pxy (vdi_handle, (PXY*)&rect);
				draw_hbar (frame, FALSE);
			}
		}
		if (dy) {
			rect = frame->clip;
			rect.g_x += frame->clip.g_w;
			if (rc_intersect (clip, &rect)) {
				rect.g_w += rect.g_x -1;
				rect.g_h += rect.g_y -1;
				vs_clip_pxy (vdi_handle, (PXY*)&rect);
				draw_vbar (frame, FALSE);
			}
		}
	}
	vs_clip_off (vdi_handle);
}


/*============================================================================*/
BOOL
containr_notify (CONTAINR cont, HW_EVENT event, const void * gen_ptr)
{
	if (cont && cont->Handler) {
		(*cont->Handler)(event, cont->HdlrArg, cont, gen_ptr);
		return TRUE;
	}
	return FALSE;
}


/*******************************************************************************
 *
 * Temporary methods
 *
 */

/*----------------------------------------------------------------------------*/
void
containr_debug (CONTAINR cont)
{
	int  depth = 0;
	BOOL b = TRUE;
	
#if 01
	if (cont) while (cont->Parent) cont = cont->Parent;
#endif
	
	while (cont) {
		if (b) {
			printf ("%*s%p = %i [%i,%i] [%i,%i,%i,%i] %s\n", depth *2, "",
			        cont, cont->Mode, cont->ColSize, cont->RowSize,
			        cont->Area.g_x,cont->Area.g_y,cont->Area.g_w,cont->Area.g_h,
			        (cont->Name ? cont->Name : "--"));
			if (cont->Mode == CNT_FRAME) {
			
			} else if (cont->Mode && cont->u.Child) {
				cont = cont->u.Child;
				depth++;
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
		depth--;
	}
}
