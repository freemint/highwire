#include <stdlib.h>

#ifdef __PUREC__
#include <tos.h>
#endif

#include "global.h"
#include "token.h"
#include "fontbase.h"


static WORDITEM paragraph_filter (PARAGRPH);


static void vTab_format (DOMBOX *, long width, BLOCKER);
static struct s_dombox_vtab paragraph_vTab = { 0, };

/*----------------------------------------------------------------------------*/
static void
vTab_delete (DOMBOX * This)
{
	PARAGRPH par = (PARAGRPH)This;
	WORDLINE line = par->Line;
	while (line) {
		WORDLINE n_ln = line->NextLine;
		free (line);
		line = n_ln;
	}
	if (par->item) {
		destroy_word_list (par->item, NULL);
	}
	DomBox_vTab.delete (This);
}


/*----------------------------------------------------------------------------*/
static LONG
vTab_MinWidth (DOMBOX * This)
{
	PARAGRPH paragraph = (PARAGRPH)This;
	WORDITEM word      = paragraph_filter (paragraph);
	BOOL      lbrk = TRUE;
	long wrd_width = (This->TextIndent > 0 ? +This->TextIndent : 0);
	long hanging   = (This->TextIndent < 0 ? -This->TextIndent : 0);
	LONG tempminwidth;

	tempminwidth = This->MinWidth;

	This->MinWidth = 0;
	while (word) {
		if (lbrk || !word->wrap) {
			if (word->image && word->image->set_w < 0) {
				wrd_width += 1 + word->image->hspace *2;
			} else {
				/* the next line causes problems with non broken text on some sites
				  This modification fixes the problems.  Not necessarily the ideal fix.*/
				wrd_width += word->word_width - (lbrk ? word->space_width :0);

				/* Ok with other changes in the code I needed to back this out
				   however I can't remember which sites realy brought out
				   the bug these were supposed to fix.  So I left them here
				   just in case we needed to bring them back   Dec 8, 2005 Dan
					if (word->word_width - (lbrk ? word->space_width :0) > wrd_width)
						wrd_width = word->word_width - (lbrk ? word->space_width :0); */
			}
			lbrk = word->line_brk;
			word = word->next_word;
			if (!lbrk) {
				continue;
			}
		} 
		
		if (This->MinWidth < wrd_width) {
			 This->MinWidth = wrd_width;
		}
		wrd_width = hanging;
		lbrk = TRUE;
	}
	if (This->MinWidth < wrd_width) {
		 This->MinWidth = wrd_width;
	}
	
	/* This tracks if the size has already been calculated and
	 * tries to keep the system from growing the DOMBOX unneccisarily
	 */
	if (tempminwidth != This->MinWidth) {
		This->MinWidth += dombox_LftDist (This) + dombox_RgtDist (This);
	}

	return This->MinWidth;
/*	return (This->MinWidth += dombox_LftDist (This) + dombox_RgtDist (This));*/
}

/*----------------------------------------------------------------------------*/
static LONG
vTab_MaxWidth (DOMBOX * This)
{
	PARAGRPH paragraph = (PARAGRPH)This;
	
	struct word_item * word = paragraph->item;
	long width   = (This->TextIndent > 0 ? +This->TextIndent : 0);
	long hanging = (This->TextIndent < 0 ? -This->TextIndent : 0);
	This->MaxWidth = 0;
	while (word) {
		BOOL ln_brk = word->line_brk;
		width += word->word_width;
		word = word->next_word;
		if (ln_brk || !word) {
			if (This->MaxWidth < width) {
				 This->MaxWidth = width;
			}
			width = hanging;
		}
	}
	
	return (This->MaxWidth += dombox_LftDist (This) + dombox_RgtDist (This));
}


/*----------------------------------------------------------------------------*/
static PARAGRPH
vTab_Paragrph (DOMBOX * This)
{
	return (PARAGRPH)This;
}


/*----------------------------------------------------------------------------*/
static void
vTab_draw (DOMBOX * This, long x, long y, const GRECT * clip, void * highlight)
{
	PARAGRPH paragraph = (PARAGRPH)This;
	draw_paragraph (paragraph, x ,y, clip, highlight);
}


/*==============================================================================
 */
PARAGRPH
new_paragraph (TEXTBUFF current)
{
	PARAGRPH paragraph = malloc (sizeof (struct paragraph_item));

	current->styles.italic     = 0;
	current->styles.bold       = 0;
	current->styles.underlined = 0;
	current->styles.strike     = 0;
	
	current->word = NULL;
	
	paragraph->item = new_word (current, FALSE);
	paragraph->Line = NULL;
	
	dombox_ctor (&paragraph->Box, current->parentbox, BC_TXTPAR);
	if (!*(long*)&paragraph_vTab) {
		paragraph_vTab          = DomBox_vTab;
		paragraph_vTab.delete   = vTab_delete;
		paragraph_vTab.MinWidth = vTab_MinWidth;
		paragraph_vTab.MaxWidth = vTab_MaxWidth;
		paragraph_vTab.Paragrph = vTab_Paragrph;
		paragraph_vTab.draw     = vTab_draw;
		paragraph_vTab.format   = vTab_format;
	}
	paragraph->Box._vtab = &paragraph_vTab;
	
	paragraph->Box.HtmlCode  = TAG_P;
	paragraph->Box.TextAlign = current->parentbox->TextAlign;
	
	paragraph->next_paragraph = NULL;
	
	current->prev_par  = NULL;
	current->paragraph = paragraph;
	
	return paragraph;
}


/*==============================================================================
 * add_paragraph()
 * 
 * Creates a new paragraph structure and links it into list
 * also creates a new word item and links it into the new paragraph
 *
 * 12/14/01 - modified to use frame_item info and directly modify frame - baldrick
 *
 * AltF4 - Jan. 20, 2002:  replaced malloc of struct word_item by new_word().
 *
*/
PARAGRPH
add_paragraph (TEXTBUFF current, short vspace)
{
	PARAGRPH paragraph = NULL;
	
	if (current->paragraph->Box.Backgnd >= 0) {
		DOMBOX * box = current->paragraph->Box.Parent;
		do if (box->Backgnd >= 0) {
			current->backgnd = box->Backgnd;
			break;
		} while ((box = box->Parent) != NULL);
	}
	
	if (current->text > current->buffer) {
		new_word (current, FALSE);
	
	} else if (current->prev_wrd) {
		current->prev_wrd->next_word = NULL; /*unlink from chain */
		current->prev_wrd            = NULL;
	
	} else {
		paragraph = current->paragraph; /* reuse empty paragraph */
	}
	
	if (!paragraph) {
		PARAGRPH copy_from = current->paragraph;
		
		paragraph = malloc (sizeof (struct paragraph_item));
		
		copy_from->next_paragraph = paragraph;
		current->prev_par         = copy_from;
		current->paragraph        = paragraph;
		paragraph->next_paragraph = NULL;
		
		paragraph->item = current->word;
		paragraph->Line = NULL;
		
		dombox_ctor (&paragraph->Box, current->parentbox, BC_TXTPAR);
		if (!*(long*)&paragraph_vTab) {
			paragraph_vTab          = DomBox_vTab;
			paragraph_vTab.delete   = vTab_delete;
			paragraph_vTab.MinWidth = vTab_MinWidth;
			paragraph_vTab.MaxWidth = vTab_MaxWidth;
			paragraph_vTab.Paragrph = vTab_Paragrph;
			paragraph_vTab.draw     = vTab_draw;
			paragraph_vTab.format   = vTab_format;
		}
		paragraph->Box._vtab = &paragraph_vTab;
		
		paragraph->Box.TextAlign = current->parentbox->TextAlign;
	}
	paragraph->Box.HtmlCode   = TAG_P;
	paragraph->Box.TextIndent = current->parentbox->TextIndent;
	
	if (vspace) {
		vspace = (vspace * current->word->word_height) /3;
		if (current->parentbox->ChildBeg != &paragraph->Box) {
			paragraph->Box.Margin.Top = vspace;
		} else if (current->parentbox->BoxClass == BC_GROUP &&
		           current->parentbox->Margin.Top < vspace) {
			current->parentbox->Margin.Top = vspace;
		}
	}
	
	return paragraph;
}


/*============================================================================*/
void paragrph_finish (TEXTBUFF current)
{
	while (current->lst_stack) {
		list_finish (current);
	}
	
	if (current->paragraph) {
		if (!current->word) {
			printf ("paragrph_finish() ERROR: no current->word!\n");
			return;
		}
		
		if (current->text > current->buffer) {
			word_store (current);
		
		} else if (current->prev_wrd) {
			if (current->prev_wrd->length == 1 && current->prev_wrd->space_width) {
				/* mark trailing space to be deleteable */
				destroy_word_list (current->word, NULL);
				current->prev_wrd->next_word = NULL;
			}
		} else if (current->prev_par) {
			current->prev_par->next_paragraph = NULL;
			Delete (&current->paragraph->Box);
		}
	}
	current->word      = current->prev_wrd = NULL;
	current->paragraph = current->prev_par = NULL;
	current->font      = fontstack_clear (&current->fnt_stack);
}


/*----------------------------------------------------------------------------*/
static void
vTab_format (DOMBOX * This, long width, BLOCKER blocker)
{
	PARAGRPH   par       = (PARAGRPH)This;
	WORDLINE * p_line    = &par->Line, line = NULL;
	WORDITEM   word      = par->item,  next;
	long       int_width;
	short      blocked   = 0x00;
	long       l_height  = 0;
	long       r_height  = 0;
	H_ALIGN    align;
	WORD hanging, hang_nxt;

	/* supress 'a name' tags from making empty lines */
	if ((word->link)&&(word->word_width == 0)&& !word->next_word) {
		return;
	}
	
	if (This->SetWidth) {
		if (This->SetWidth > 0) {
			width = This->SetWidth;
		} else if (This->SetWidth > -1024) {
			width = (-This->SetWidth * width +512) /1024;
		}
		if (width < This->MinWidth) {
			width = This->MinWidth;
		}
	}
	int_width    = width -= dombox_LftDist (This) + dombox_RgtDist (This);
	This->Rect.H = dombox_TopDist (This);
	
	if (par->Box.TextIndent < 0) {
		hanging  = 0;
		hang_nxt = -par->Box.TextIndent;
	} else {
		hanging  = +par->Box.TextIndent;
		hang_nxt = 0;
	}
	
	if (par->Box.HtmlCode == TAG_IMG) {
		align = ALN_LEFT;
	} else {
		align = This->TextAlign;
		if (blocker->L.bottom && blocker->L.width > This->Padding.Lft) {
			l_height   = blocker->L.bottom - This->Rect.Y;
			int_width -= blocker->L.width  - This->Padding.Lft;
			blocked   |= (BRK_LEFT & ~BRK_LN);
		}
		if (blocker->R.bottom && blocker->R.width > This->Padding.Rgt) {
			r_height   = blocker->R.bottom - This->Rect.Y;
			int_width -= blocker->R.width  - This->Padding.Rgt;
			blocked   |= (BRK_RIGHT & ~BRK_LN);
		}
	}
	
	do {
		short count = 1;
		short img_h = 0;
		short offset, justify;
		BOOL  ln_brk;
		
		WORDITEM w_beg = NULL;
		short    w_len = 0;
		short    w_cnt = 0;

		while (word) {
			if (word->image) {
				word->line = NULL;
				image_calculate (word->image, int_width);
				if (word->vertical_align == ALN_TOP) {
					img_h = word->word_height + word->word_tail_drop;
					word->word_tail_drop = 0;
				}
				break;
			}
			if (word->link || !word->space_width || word->length > 0) {
				break;
			}
			word = word->next_word;
		}
		if (!word) break;

		if ((line = *p_line) == NULL) {
			*p_line = line = malloc (sizeof(struct word_line));
			line->NextLine = NULL;
		}
		p_line = &line->NextLine;
		line->Paragraph = par;
		line->Word      = word;
		line->Ascend    = word->word_height;
		line->Descend   = word->word_tail_drop;
		line->Width     = width - blocker->R.width;
		offset = int_width - word->word_width + word->space_width - hanging;
		
		if (offset < 0) {
			This->Rect.H      = max (l_height, r_height);
			int_width        += blocker->L.width + blocker->R.width;
			offset           += blocker->L.width + blocker->R.width;
			blocker->L.bottom = blocker->L.width = l_height = 0;
			blocker->R.bottom = blocker->R.width = r_height = 0;
			blocked   = 0x00;
		}

		ln_brk = word->line_brk;

		while ((word = word->next_word) != NULL && !ln_brk) {
			if (word->image) {
				word->line = NULL;
				image_calculate (word->image, int_width);
				if (word->vertical_align == ALN_TOP) {
					img_h = max (img_h, word->word_height + word->word_tail_drop);
					word->word_tail_drop = 0;
				}
			}
			if (word->wrap) {
				w_beg = word;
				w_cnt = count;
				w_len = offset;
			}
			if (word->word_width > offset) {
				if (w_beg) {
					word   = w_beg;
					count  = w_cnt;
					offset = w_len;
				}
				break;
			}
			if (line->Ascend  < word->word_height)
				 line->Ascend  = word->word_height;
			if (line->Descend < word->word_tail_drop)
				 line->Descend = word->word_tail_drop;
			offset -= word->word_width;
			ln_brk  = word->line_brk;
			count++;
		}
		next        = word;
		word        = line->Word;
		line->Count = count;
		if (line->Descend < (img_h -= line->Ascend)) {
			line->Descend = img_h;
		}
	
		if (align == ALN_JUSTIFY) {
			justify = (!ln_brk && next ? offset : 0);
			offset  = 0;
		} else if (align) {
			justify = 0;
			if (align == ALN_CENTER) offset /= 2;
		} else {
			justify = 0;
			offset  = 0;
		}

		offset -= word->space_width;
		offset += hanging + dombox_LftDist (This);

		if (blocked)
			offset += blocker->L.width;
		
		while(1) {
			word->line = line;
			if (word->image) {
				/* Origin already set in new_image() */
				word->image->offset.X = offset;
				word->image->offset.Y = This->Rect.H;
				if (word->vertical_align != ALN_TOP) {
					word->image->offset.Y += line->Ascend - word->word_height;
				} else {
					word->word_height    = line->Ascend;
					word->word_tail_drop = img_h;
				}
			}
			if (word->link && !word->link->isHref) {
				/* Origin already set in new_named_location() */
				word->link->u.anchor->offset.X = -This->Rect.X;
				word->link->u.anchor->offset.Y =  This->Rect.H;
			}
			word->h_offset = offset;

			if (!--count) break;

			if (justify) {
				short w = justify / count;
				offset  += w;
				justify -= w;
			}
			offset += word->word_width;
			word    = word->next_word;
		}
		line->Word->h_offset += line->Word->space_width;
		
		This->Rect.H  += line->Ascend;
		line->OffsetY =  This->Rect.H;
		This->Rect.H  += line->Descend;
		
		if (blocked) {
			if (blocked & (BRK_LEFT & ~BRK_LN)) {
				if (ln_brk & (BRK_LEFT & ~BRK_LN)) {
					if (This->Rect.H < l_height) {
						line->Descend += l_height - This->Rect.H;
						This->Rect.H  =  l_height;
					}
				}
				if (This->Rect.H >= l_height) {
					blocked   &= ~(BRK_LEFT & ~BRK_LN);
					int_width += blocker->L.width;
					blocker->L.bottom = blocker->L.width = l_height = 0;
				}
			}
			if (blocked & (BRK_RIGHT & ~BRK_LN)) {
				if (ln_brk & (BRK_RIGHT & ~BRK_LN)) {
					if (This->Rect.H < r_height) {
						line->Descend += r_height - This->Rect.H;
						This->Rect.H  =  r_height;
					}
				}
				if (This->Rect.H >= r_height) {
					blocked   &= ~(BRK_RIGHT & ~BRK_LN);
					int_width += blocker->R.width;
					blocker->R.bottom = blocker->R.width = r_height = 0;
				}
			}
		}
		
		hanging = hang_nxt;
	
	} while ((word = next) != NULL);
	
	if (line && (word = line->Word) != NULL) do {
		if (word->length && !word->image && word->word_tail_drop >= line->Descend) {
			This->Rect.H++;
			break;
		}
	} while ((word = word->next_word) != NULL);

	if ((line = *p_line) != NULL) {
		WORDLINE next_line;
		do {
			next_line = line->NextLine;
			free (line);
		} while ((line = next_line) != NULL);
		*p_line = NULL;
	}

	if (par->Box.HtmlCode == TAG_IMG) {
		This->Rect.W = par->item->word_width;
	} else {
		This->Rect.W = width;
	}
	This->Rect.W += dombox_LftDist (This) + dombox_RgtDist (This);
	This->Rect.H += dombox_BotDist (This);
}


/*==============================================================================
 * paragrph_word()
 *
 * Returns the word item at the coordinates x/y, relative to the paragraph's
 * origin.  The result will be NULL if the coordinates are befor the first or
 * after the last word of a line, or between two words and the right one isn't
 * a link.  In any case the calculated extents are stored in the long array.  
*/
WORDITEM
paragrph_word (PARAGRPH par, long x, long y, long area[4])
{
	WORDITEM word = NULL;
	WORDLINE line = par->Line;
	long     bot  = 0;
	
	if (line && line->OffsetY - line->Ascend > y) {
		area[3] = line->OffsetY - line->Ascend;
		return NULL;
	
	} else while (line) {
		bot = line->OffsetY + line->Descend;
		if (bot <= y) {
			line = line->NextLine;
		} else {
			word    =  line->Word;
			area[1] += line->OffsetY - line->Ascend;
			area[3] =  line->Ascend + line->Descend;
			break;
		}
	}
	
	if (!word) {
		area[1] += bot;
		area[3] =  par->Box.Rect.H - bot;
	
	} else if (x < word->h_offset) {
		area[2] = word->h_offset;
		word = NULL;
	
	} else if (x >= line->Width) {
		area[0] += line->Width;
		area[2] =  par->Box.Rect.W - line->Width;
		word = NULL;
	
	} else {
		WORDITEM url = NULL;
		short num = line->Count;
		short spc = word->h_offset;
		short lft = spc - word->space_width;
		short rgt = lft + word->word_width;
		while (rgt <= x) {
			lft = rgt;
			if (!word->link || !word->link->isHref) {
				url = NULL;
			} else if (!url || url->link != word->link) {
				url = word;
			}
			if (--num) {
				word = word->next_word;
				spc  = word->h_offset + word->space_width;
				rgt  = word->h_offset + word->word_width;
			} else {
				word = NULL;
				break;
			}
		}
		if (!word) {
			area[0] += lft;
			area[2] =  line->Width - lft;
		
		} else if (url && url != word) {
			area[0] += lft;
			area[2] =  rgt - lft;
		
		} else if (x < spc) {
			area[0] += lft;
			area[2] =  spc - lft;
			word = NULL;
		
		} else {
			area[0] += spc;
			area[2] =  rgt - spc;
		}
	}
	
	return word;
}


/*==============================================================================
 * paragraph_extend()
 *
 * Calculates the whole extents of all words which points to the same link.
 * The resulting rectangle has it's coordinates set relative to that of the
 * given word.
*/
GRECT
paragraph_extend (WORDITEM word)
{
	void   * link = word->link;
	short    lft  = word->h_offset, rgt, x;
	WORDLINE line = word->link->start->line;
	short    num  = line->Count;
	GRECT    ext  = { 0,0,0,0 };
	
	if (line != word->line) {
		WORDLINE orig = word->line;
		ext.g_y = (line->OffsetY - line->Ascend) - (orig->OffsetY - orig->Ascend);
		if (line->Paragraph != orig->Paragraph) {
			ext.g_y += line->Paragraph->Box.Rect.Y - orig->Paragraph->Box.Rect.Y;
			ext.g_x =  line->Paragraph->Box.Rect.X - orig->Paragraph->Box.Rect.X;
			lft     -= ext.g_x;
		}
	}
	word = line->Word;
	
	while (--num && word->link != link) {
		word = word->next_word;
	}
	rgt = lft;
	x   = word->h_offset + (word == line->Word ? 0 : word->space_width);
	while(1) {
		if (lft > x) {
			ext.g_x += x - lft;
			lft     =  x;
		}
		if (num) {
			do {
				WORDITEM next = word->next_word;
				if (next->link != link) {
					break;
				} else {
					word = next;
				}
			} while (--num);
		}
		x = word->h_offset + word->word_width
		  - (word == line->Word ? word->space_width : 0);
		if (rgt < x) {
			rgt = x;
		}
		ext.g_h += line->Ascend + line->Descend;
		
		if (num) {
			break;
		} else if (line->NextLine) {
			line = line->NextLine;
		} else {
			PARAGRPH par      = line->Paragraph;
			PARAGRPH next_par = par->next_paragraph;
			if (!next_par || !next_par->Line) {
				break;
			} else {
				short d_x = next_par->Box.Rect.X - par->Box.Rect.X;
				lft     += d_x;
				rgt     += d_x;
				ext.g_h += par->Box.Margin.Bot + next_par->Box.Margin.Top;
				line = next_par->Line;
			}
		}
		word = line->Word;
		if (word->link != link) {
			break;
		}
		num = line->Count -1;
		x   = word->h_offset;
	}
	ext.g_w = rgt - lft +1;
	
	return ext;
}


/*----------------------------------------------------------------------------*/
static WORDITEM
paragraph_filter (PARAGRPH par)
{
	WORDITEM word = par->item, prev = NULL;
	BOOL     remv = FALSE;
	
	while (word) {
		if (word->length == 1) {
			if (word->space_width && (word->line_brk
			    || !word->next_word || word->next_word->space_width)) {
				remv = TRUE;
			}
		} else if (!word->length) {
			remv = (word->image == NULL);
		}
		if (remv) {
			if (prev) {
				prev->line_brk |= word->line_brk;
				prev->next_word = word->next_word;
			} else {
				par->item = word->next_word;
			}
			word = destroy_word_list (word, word);
			remv = FALSE;
			continue;
		}
		prev = word;
		word = word->next_word;
	}
	return par->item;
}
