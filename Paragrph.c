#include <stdlib.h>

#ifdef __PUREC__
#include <tos.h>
#endif

#include "global.h"
#include "Table.h"
#include "fontbase.h"


static WORDITEM paragraph_filter (PARAGRPH);
static void     paragraph_calc   (PARAGRPH, long, struct blocking_area *);


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
	if (par->Table) {
		delete_table (&par->Table);
	}
	DomBox_vTab.delete (This);
}


/*----------------------------------------------------------------------------*/
static LONG
vTab_MinWidth (DOMBOX * This)
{
	PARAGRPH paragraph = (PARAGRPH)This;
	
	if (paragraph->paragraph_code == PAR_HR) {
		This->MinWidth = 2;
	
	} else {
		WORDITEM  word = paragraph_filter (paragraph);
		BOOL      lbrk = TRUE;
		long wrd_width = (paragraph->Hanging > 0 ? +paragraph->Hanging : 0);
		long hanging   = (paragraph->Hanging < 0 ? -paragraph->Hanging : 0);
		This->MinWidth = 0;
		while (word) {
			if (lbrk || !word->wrap) {
				if (word->image && word->image->set_w < 0) {
					wrd_width += 1 + word->image->hspace *2;
				} else {
					wrd_width += word->word_width - (lbrk ? word->space_width :0);
				}
				lbrk = word->line_brk;
				word = word->next_word;
				if (!lbrk) {
					continue;
				}
			} /* else wrap || ln_brk */
			
			if (This->MinWidth < wrd_width) {
				 This->MinWidth = wrd_width;
			}
			wrd_width = hanging;
			lbrk = TRUE;
		}
		if (This->MinWidth < wrd_width) {
			 This->MinWidth = wrd_width;
		}
	}
	This->MinWidth += dombox_LftDist (This) + dombox_RgtDist (This)
	                + paragraph->Indent + paragraph->Rindent;
	return This->MinWidth;
}

/*----------------------------------------------------------------------------*/
static LONG
vTab_MaxWidth (DOMBOX * This)
{
	PARAGRPH paragraph = (PARAGRPH)This;
	
	struct word_item * word = paragraph->item;
	long width = 0;
	This->MaxWidth = 0;
	while (word) {
		BOOL ln_brk = word->line_brk;
		width += word->word_width;
		word = word->next_word;
		if (ln_brk || !word) {
			if (This->MaxWidth < width) {
				 This->MaxWidth = width;
			}
			width = paragraph->Indent;
		}
	}
	This->MaxWidth += dombox_LftDist (This) + dombox_RgtDist (This)
	                + paragraph->Indent + paragraph->Rindent;
	return This->MaxWidth;
}

/*----------------------------------------------------------------------------*/
static void
vTab_draw (DOMBOX * This, long x, long y, const GRECT * clip, void * highlight)
{
	PARAGRPH paragraph = (PARAGRPH)This;
	if (paragraph->paragraph_code == PAR_HR) {
		draw_hr (paragraph, x, y);
	} else {
		draw_paragraph (paragraph, x ,y, clip, highlight);
	}
}

/*----------------------------------------------------------------------------*/
static void
vTab_format (DOMBOX * This, long width, BLOCKER blocker)
{
	PARAGRPH par = (PARAGRPH)This;
	
	This->Rect.X += par->Indent;
	width        -= par->Indent + par->Rindent;
	
	if (par->paragraph_code == PAR_HR) {
		struct word_item * word = par->item;
		long blk_width = width - (blocker->L.width + blocker->R.width);
		if (word->space_width < 0) {
			word->word_width = (blk_width * -word->space_width + 512) / 1024;
		} else {
			word->word_width = word->space_width;
		}
		if (word->word_width >= blk_width) {
			word->word_width = blk_width;
			word->h_offset   = 0;
		} else {
			if (word->word_width < 2) {
				word->word_width = 2;
			}
			if (par->alignment <= ALN_JUSTIFY) {
				word->h_offset   = 0;
			} else {
				word->h_offset   = blk_width - word->word_width;
				if (par->alignment == ALN_CENTER) {
					word->h_offset /= 2;
				}
			}
		}
		This->Rect.X += blocker->L.width;
		This->Rect.W = width;
		This->Rect.H = word->word_height *2
		             + (word->word_tail_drop > 0
		                ? +word->word_tail_drop : -word->word_tail_drop);
	
	} else {   /* normal text or image */
		if (par->paragraph_code == PAR_IMG) {
			par->Box.Rect.X += blocker->L.width;
		}
		paragraph_calc (par, width, blocker);
	}
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
	
	paragraph->item    = new_word (current, FALSE);
	paragraph->Table   = NULL;
	paragraph->Line    = NULL;
	paragraph->Indent  = 0;
	paragraph->Rindent = 0;
	paragraph->Hanging = 0;
	paragraph->paragraph_code = PAR_NONE;
	paragraph->alignment      = ALN_LEFT;
	
	dombox_ctor (&paragraph->Box, current->parentbox, BC_TXTPAR);
	if (!*(long*)&paragraph_vTab) {
		paragraph_vTab          = DomBox_vTab;
		paragraph_vTab.delete   = vTab_delete;
		paragraph_vTab.MinWidth = vTab_MinWidth;
		paragraph_vTab.MaxWidth = vTab_MaxWidth;
		paragraph_vTab.draw     = vTab_draw;
		paragraph_vTab.format   = vTab_format;
	}
	paragraph->Box._vtab = &paragraph_vTab;

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
	
	if (current->text > current->buffer
	    || current->paragraph->paragraph_code >= PAR_TABLE) {
		new_word (current, FALSE);
	
	} else if (current->prev_wrd) {
		current->prev_wrd->next_word = NULL; /*unlink from chain */
		current->prev_wrd            = NULL;
	
	} else {
		paragraph = current->paragraph; /* reuse empty paragraph */
	}
	
	if (!paragraph) {
		PARAGRPH copy_from = current->paragraph;
		WORD     indent    = copy_from->Indent;
		
		if (copy_from->paragraph_code == PAR_LI && copy_from->Hanging < 0) {
			indent -= copy_from->Hanging;
		}
		
		paragraph = malloc (sizeof (struct paragraph_item));
		
		copy_from->next_paragraph = paragraph;
		current->prev_par         = copy_from;
		current->paragraph        = paragraph;
		paragraph->next_paragraph = NULL;
		
		paragraph->item    = current->word;
		paragraph->Table   = NULL;
		paragraph->Line    = NULL;
		paragraph->Indent  = indent;
		paragraph->Rindent = copy_from->Rindent;
		paragraph->alignment = copy_from->alignment;
		dombox_ctor (&paragraph->Box, current->parentbox, BC_TXTPAR);
		if (!*(long*)&paragraph_vTab) {
			paragraph_vTab          = DomBox_vTab;
			paragraph_vTab.delete   = vTab_delete;
			paragraph_vTab.MinWidth = vTab_MinWidth;
			paragraph_vTab.MaxWidth = vTab_MaxWidth;
			paragraph_vTab.format   = vTab_format;
			paragraph_vTab.draw     = vTab_draw;
		}
		paragraph->Box._vtab = &paragraph_vTab;
	}
	paragraph->paragraph_code = PAR_NONE;
	paragraph->Hanging        = 0;
	
	if (current->prev_par && vspace) {
		paragraph->Box.Margin.Top = (vspace * current->word->word_height) /3;
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
		
		if (current->text > current->buffer
		    || current->paragraph->paragraph_code >= PAR_TABLE) {
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


/*------------------------------------------------------------------------------
 */
static void
paragraph_calc (PARAGRPH par, long width, struct blocking_area *blocker)
{
	WORDLINE * p_line    = &par->Line, line;
	WORDITEM   word      = par->item,  next;
	long       int_width = width;
	short      blocked   = 0x00;
	long       l_height  = 0;
	long       r_height  = 0;
	H_ALIGN    align;
	WORD hanging, hang_nxt;
	
	if (par->Hanging < 0) {
		hanging  = 0;
		hang_nxt = -par->Hanging;
	} else {
		hanging  = +par->Hanging;
		hang_nxt = 0;
	}
	
	if (par->paragraph_code == PAR_IMG) {
		align = ALN_LEFT;
	} else {
		align = par->alignment;
		if (blocker->L.bottom) {
			l_height   = blocker->L.bottom - par->Box.Rect.Y;
			int_width -= blocker->L.width;
			blocked   |= (BRK_LEFT & ~BRK_LN);
		}
		if (blocker->R.bottom) {
			r_height   = blocker->R.bottom - par->Box.Rect.Y;
			int_width -= blocker->R.width;
			blocked   |= (BRK_RIGHT & ~BRK_LN);
		}
	}
	par->Box.Rect.H = dombox_TopDist (&par->Box);
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
		offset += hanging + dombox_LftDist (&par->Box);

		if (blocked)
			offset += blocker->L.width;
		
		while(1) {
			word->line = line;
			if (word->image) {
				/* Origin already set in new_image() */
				word->image->offset.X = offset;
				word->image->offset.Y = par->Box.Rect.H;
				if (word->vertical_align != ALN_TOP) {
					word->image->offset.Y += line->Ascend - word->word_height;
				} else {
					word->word_height    = line->Ascend;
					word->word_tail_drop = img_h;
				}
			}
			if (word->link && !word->link->isHref) {
				/* Origin already set in new_named_location() */
				word->link->u.anchor->offset.X = -par->Box.Rect.X;
				word->link->u.anchor->offset.Y =  par->Box.Rect.H;
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
		
		par->Box.Rect.H += line->Ascend;
		line->OffsetY   =  par->Box.Rect.H;
		par->Box.Rect.H += line->Descend;
		
		if (blocked) {
			if (blocked & (BRK_LEFT & ~BRK_LN)) {
				if (ln_brk & (BRK_LEFT & ~BRK_LN)) {
					if (par->Box.Rect.H < l_height) {
						line->Descend += l_height - par->Box.Rect.H;
						par->Box.Rect.H = l_height;
					}
				}
				if (par->Box.Rect.H >= l_height) {
					blocked   &= ~(BRK_LEFT & ~BRK_LN);
					int_width += blocker->L.width;
					blocker->L.bottom = blocker->L.width = l_height = 0;
				}
			}
			if (blocked & (BRK_RIGHT & ~BRK_LN)) {
				if (ln_brk & (BRK_RIGHT & ~BRK_LN)) {
					if (par->Box.Rect.H < r_height) {
						line->Descend += r_height - par->Box.Rect.H;
						par->Box.Rect.H = r_height;
					}
				}
				if (par->Box.Rect.H >= r_height) {
					blocked   &= ~(BRK_RIGHT & ~BRK_LN);
					int_width += blocker->R.width;
					blocker->R.bottom = blocker->R.width = r_height = 0;
				}
			}
		}
		
		hanging = hang_nxt;
	
	} while ((word = next) != NULL);

	if ((line = *p_line) != NULL) {
		WORDLINE next_line;
		do {
			next_line = line->NextLine;
			free (line);
		} while ((line = next_line) != NULL);
		*p_line = NULL;
	}

	if (par->paragraph_code == PAR_IMG) {
		par->Box.Rect.W = par->item->word_width;
	} else {
		par->Box.Rect.W = width;
	}
	par->Box.Rect.H += dombox_BotDist (&par->Box);
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


/*============================================================================*/
void
content_setup (CONTENT * content, TEXTBUFF current,
               DOMBOX * parent, BOXCLASS class, short padding, short backgnd)
{
	dombox_ctor (&content->Box, parent, class);
	content->Box.Backgnd   = backgnd;
	content->Box.TextAlign = ALN_LEFT;
	if (padding) {
		content->Box.Padding.Top = content->Box.Padding.Bot =
		content->Box.Padding.Lft = content->Box.Padding.Rgt = padding;
	}
	if (current) {
		current->parentbox = &content->Box;
		content->Item    = new_paragraph (current);
		current->backgnd = backgnd;
	} else {
		content->Item    = NULL;
	}
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
