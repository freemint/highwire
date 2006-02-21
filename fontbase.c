#include <stdlib.h>
#include <stdio.h>

#include <gemx.h>

#include "global.h"
#include "fontbase.h"


/*============================================================================*/
void
font_setup (UWORD id, WORD type, BOOL bold, BOOL italic)
{
	if (vst_font (vdi_handle, id) != id) {
		printf ("font #%i for '%s%s%s' not available.\n", id,
		        (type == normal_font ? "normal" :
		         type == header_font ? "header" : "pre"),
		         (bold ? ",bold" : ""), (italic ? ",italic" : ""));
		id = 0;
	
	} else if (type == pre_font) { /* check for monospaced */
		WORD w1, w2, pts[8];
		vst_point (vdi_handle, 24, pts,pts,pts,pts);
		vqt_extent (vdi_handle, ".", pts);   w1 = pts[2] - pts[0];
		vqt_extent (vdi_handle, "W", pts);   w2 = pts[2] - pts[0];
		if (w1 != w2) {
			printf ("font #%i for 'pre%s%s' is not monospace (%i/%i), ignored.\n",
			        id, (bold ? ",bold" : ""), (italic ? ",italic" : ""), w1, w2);
			id = 0;
		}
	}
	if (id) {
		fonts[type][bold ? 1 : 0][italic ? 1 : 0] = id;
	}
}


/*----------------------------------------------------------------------------*/
static FONTBASE
font_base (WORD id, UWORD style)
{
	static FONTBASE base = NULL;
	
	FONTBASE found;
	if (!id) {
		found = NULL;
	
	} else {
		found = base;
		
		while (found && (found->Id != id || found->Style != style)) {
			found = found->Next;
		}
		if (!found) {
			XFNT_INFO info = { sizeof(XFNT_INFO), };
			if (vqt_xfntinfo (vdi_handle, 0x0001, id, 0, &info)) {
				found = malloc (sizeof(struct s_fontbase));
				found->List    = NULL;
				found->Id      = id;
				found->RealId  = info.id;
				found->Style   = style;
				found->Effects = 0x0000;
				switch (info.format) {
					case 2:
						found->Mapping   = MAP_BITSTREAM;
						found->SpaceCode = SPACE_BICS;
						found->NobrkCode = NOBRK_BICS;
						break;
					case 4: case 8:
						found->Mapping   = MAP_UNICODE;
						found->SpaceCode = SPACE_UNI;
						found->NobrkCode = NOBRK_UNI;
						break;
					case 1: default:
						found->Mapping   = MAP_ATARI;
						found->SpaceCode = SPACE_ATARI;
						found->NobrkCode = NOBRK_ATARI;
				}
				found->Next  = base;
				base         = found;
			}
		}
	}
	return found;
}


/*============================================================================*/
FONT
font_byType (WORD type, WORD style, WORD points, WORDITEM word)
{
	FONT     prev = word->font;
	FONT     same = NULL, font;
	FONTBASE base;
	UWORD    effect;
	BOOL     condns;
	WORD     set_id;
	
	if (type < 0) {
		type = TAgetFont (word->attr);
	} else {
		TAsetFont (word->attr, type);
	}
	if (points < 0) {
		points = TA_Size (word->attr);
	} else {
		TA_Size(word->attr) = points;
	}
	if (style < 0) {
		effect = (TAgetBold   (word->attr) ? FNT_BOLD   : 0x0000)
		       | (TAgetItalic (word->attr) ? FNT_ITALIC : 0x0000);
		condns = (TAgetCondns (word->attr) != 0);
	} else {
		effect =   style & (FNT_BOLD|FNT_ITALIC);
		condns = ((style &  FNT_CONDNS) != 0);
		TAsetBold   (word->attr, (style & FNT_BOLD));
		TAsetItalic (word->attr, (style & FNT_ITALIC));
		TAsetCondns (word->attr, condns);
	}
	set_id = fonts[type][effect & FNT_BOLD ? 1 : 0][effect & FNT_ITALIC ? 1 : 0];
	
	if ((base = font_base (set_id, effect)) == NULL) {
		
		/* if no matching fontbase was found get the most near that exists.
		*/
		FONTBASE tmp;
		if (!effect
		    || (effect == (FNT_BOLD|FNT_ITALIC)
		        && ((tmp = font_base (fonts[type][0][1], FNT_ITALIC)) == NULL)
		        && ((tmp = font_base (fonts[type][1][0], FNT_BOLD))   == NULL))
		    || ((tmp = font_base (fonts[type][0][0], 0)) == NULL)) {
			tmp = font_base (1, 0);
		}
		/* clone the found fontbase
		*/
		*(base = malloc (sizeof(struct s_fontbase))) = *tmp;
		base->List    = NULL;
		base->Id      = set_id;
		base->Style   = effect;
		base->Effects = effect & ~(tmp->Style & ~tmp->Effects);
		
		tmp->Next = base; /* insert the clone in the list */
	}
	
	/* search in the fontbase's list for a matching entry
	*/
	font = base->List;
	while (font) {
		if (font->Points == points) {
			if ((font->Condensed != 0) == condns) {
				break;
			} else {
				same = font; /* same size but condese differs */
			}
		}
		font = font->Next;
	}
	if (!font) {
		WORD dist[5], u[3];
		LONG cellwd = 0;
		
		if (!prev || prev->Base != base) {
			vst_font     (vdi_handle, base->RealId);
			vst_map_mode (vdi_handle, base->Mapping);
			vst_effects  (vdi_handle, base->Effects);
		}
		vst_arbpt (vdi_handle, points, u,u, (WORD*)&cellwd, u);
		
		font = malloc (sizeof(struct s_font));
		if (same) {
			*font = *same;
			same->Next = font;
		
		} else {
			font->Base = base;
			font->Next = base->List;
			base->List = font;
			vqt_fontinfo (vdi_handle, u,u, dist, u,u);
			font->Points  = points;
			if (base->Mapping == MAP_ATARI) {
				font->Ascend  = dist[4];
				font->Descend = dist[0];
			} else {
				font->Ascend  = dist[3];
				font->Descend = dist[1];
			}
		}
		if (!condns) {
			font->Condensed = 0;
		} else {
			font->Condensed = (cellwd *8 +5) /11;
			vst_setsize32 (vdi_handle, font->Condensed, u,u,u,u);
		}
		vqt_advance (vdi_handle, base->SpaceCode, &font->SpaceWidth, u,u,u);
		font->SpaceWidth++;
		
	} else {
		if (font_switch (font, prev)) {
			vst_effects (vdi_handle, font->Base->Effects);
		}
	}
	
	word->font           = font;
	word->word_height    = font->Ascend;
	word->word_tail_drop = font->Descend;
		
	return font;
}


/*============================================================================*/
BOOL
font_switch (FONT actual, FONT previous)
{
	FONTBASE base = actual->Base;
	FONTBASE prev = (previous ? previous->Base : NULL);
	BOOL effects;
	WORD u;
	
	if (base != prev) {
		vst_font     (vdi_handle, base->RealId);
		vst_map_mode (vdi_handle, base->Mapping);
		effects = (!prev || prev->Effects != base->Effects);
	} else {
		effects = FALSE;
	}
	vst_arbpt   (vdi_handle, actual->Points, &u,&u,&u,&u);
	if (actual->Condensed) {
		vst_setsize32 (vdi_handle, actual->Condensed, &u,&u,&u,&u);
	}
	
	return effects;
}


/*============================================================================*/
WORD
font_step2size (WORD step)
{
	static WORD size[8] = { 0, 0, 0, 0, };
	
	if (size[3] != font_size) {
		WORD i;
		WORD s = font_size;
#if 1
		size[0] = max (s - s / 3, font_minsize);    /*  7  8  8  9 10 10 11 12 */
		size[1] = max (s - s / 5, font_minsize);    /*  8  9 10 11 12 12 13 15 */
		size[2] = max (s - s / 8, font_minsize);    /*  9 10 11 12 13 14 14 16 */
		size[3] = max (s,         font_minsize);    /* 10 11 12 13 14 15 16 18 */
		for (i = 4; i <= 7; i++) {
			s += s / 6;  /* < 18: 2-point step, ò 18: 3-point step */
			size[i] = max (s, font_minsize);
		}
#else
		size[0] = max (s - (s / 2), font_minsize);  /*  5  6  7  7  7  8  8  9 */
		size[1] = max (s - (s / 3), font_minsize);  /*  7  8  8  9 10 10 11 12 */
	/*	size[2] = max (s - (s / 6), font_minsize);*//*  9 10 10 11 12 13 14 15 */
		size[2] = max ((s+size[1]) /2, font_minsize);/* 8  9 10 11 12 12 13 15 */
		size[3] = max (s,           font_minsize);  /* 10 11 12 13 14 15 16 18 */
		for (i = 4; i <= 7; i++)
			s += 2;  /* 2-point step */
			size[i] = max (s, font_minsize);
		}
#endif
	}
	
	if (step < 0) {
		WORD s = -step;
		step   = 7;
		while (size[step] > s && --step);
		
		return step;
	}

	return size[step >= 7 ? 7 : step];
}


/*============================================================================*/
FNTSTACK
fontstack_setup (FNTSBASE * base, WORD color)
{
	base->Prev = base->Next = NULL;
	base->Color     = (color >= 0 ? color : G_BLACK);
	base->Type      = normal_font;
	base->Size      = font_size;
	base->Step      = 3;
	base->setBold   = FALSE;
	base->setItalic = FALSE;
	base->setUndrln = FALSE;
	base->setStrike = FALSE;
	
	return base;
}

/*============================================================================*/
FNTSTACK
fontstack_clear (FNTSBASE * base)
{
	FNTSTACK fstk = base->Next;
	while (fstk) {
		FNTSTACK next = fstk->Next;
		free (fstk);
		fstk = next;
	}
	base->Prev = base->Next = NULL;
	
	return NULL;
}

/*============================================================================*/
FNTSTACK
fontstack_push (TEXTBUFF current, WORD step)
{
	FNTSTACK fstk = current->font;
	if (fstk->Next) {
		fstk = fstk->Next;
	} else if ((fstk = malloc (sizeof(FNTSBASE))) != NULL) {
		fstk->Next          = NULL;
		fstk->Prev          = current->font;
		current->font->Next = fstk;
	}
	fstk->Color     = current->font->Color;
	fstk->Type      = current->font->Type;
	if (step < 0) {
		fstk->Size   = current->font->Size;
		fstk->Step   = current->font->Step;
	} else {
		fstk->Size   = font_step2size (step);
		fstk->Step   = step;
		word_set_point (current, fstk->Size);
	}
	fstk->setBold   = FALSE;
	fstk->setItalic = FALSE;
	fstk->setUndrln = FALSE;
	fstk->setStrike = FALSE;
	fstk->setCondns = FALSE;
	fstk->setNoWrap = FALSE;
	
	return (current->font = fstk);
}

/*============================================================================*/
FNTSTACK
fontstack_pop (TEXTBUFF current)
{
	FNTSTACK fstk = current->font;
	if (fstk->setBold)   word_set_bold      (current, fstk->setBold   = FALSE);
	if (fstk->setItalic) word_set_italic    (current, fstk->setItalic = FALSE);
	if (fstk->setStrike) word_set_strike    (current, fstk->setStrike = FALSE);
	if (fstk->setUndrln) word_set_underline (current, fstk->setUndrln = FALSE);
	if (fstk->setCondns) TAsetCondns        (current->word->attr,       FALSE);
	if (fstk->setNoWrap && current->nowrap)  current->nowrap--;
	if (fstk->Prev) {
		current->font = fstk = fstk->Prev;
		word_set_font  (current, fstk->Type);
		word_set_point (current, fstk->Size);
		if (!current->word->link) word_set_color (current, fstk->Color);
	}
	return fstk;
}

/*============================================================================*/
void
fontstack_setSize (TEXTBUFF current, WORD size)
{
	FNTSTACK fstk = current->font;

	/* keep nasty sites from making the font size too small */
	if (size < font_minsize) size = font_minsize;

	fstk->Size = size;
	fstk->Step = font_step2size (-size);
	word_set_point (current, size);
}
