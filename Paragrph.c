#include <stdlib.h>

#ifdef __PUREC__
#include <tos.h>
#endif

#include "global.h"
#include "Table.h"
#include "fontbase.h"


struct blocking_area {
	struct {
		long width;
		long bottom;
	} L, R;
};


void
destroy_paragraph_structure (PARAGRPH current_paragraph)
{
	struct paragraph_item *temp;

	while (current_paragraph != 0)
	{
		if (current_paragraph->item != 0)
			destroy_word_list (current_paragraph->item, NULL);
#if 0
		if (current_paragraph->table != 0)
			destroy_table_structure (current_paragraph->table);
#endif
		temp = current_paragraph->next_paragraph;
		free (current_paragraph);
		current_paragraph = temp;
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
	paragraph->Backgnd = -1;
	paragraph->Height  = 0;
	paragraph->Offset.Origin = NULL;
	paragraph->Offset.X      = 0;
	paragraph->Offset.Y      = 0;
	paragraph->paragraph_code = PAR_NONE;
	paragraph->alignment      = ALN_LEFT;
	paragraph->floating       = ALN_NO_FLT;
	paragraph->eop_space      = 0;
	paragraph->min_width      = 0;
	paragraph->max_width      = 0;

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
		paragraph->Hanging = 0;
		paragraph->Backgnd = -1;
		paragraph->Height  = 0;
		paragraph->Offset.Origin = copy_from->Offset.Origin;
		paragraph->Offset.X      = 0;
		paragraph->Offset.Y      = 0;
		paragraph->alignment = copy_from->alignment;
		paragraph->eop_space = 0;
		paragraph->min_width = 0;
		paragraph->max_width = 0;
	}
	paragraph->paragraph_code = PAR_NONE;
	paragraph->floating       = ALN_NO_FLT;
	
	if (current->prev_par && vspace) {
		vspace *= current->word->word_height;
		vspace /= 3;
		if (current->prev_par->eop_space < vspace) {
			current->prev_par->eop_space = vspace;
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
			current->prev_par->eop_space      = 0;
			destroy_paragraph_structure (current->paragraph);
		}
	}
	current->word      = current->prev_wrd = NULL;
	current->paragraph = current->prev_par = NULL;
	
	if (current->font_step) {
		struct font_step * step = current->font_step, * prev;
		do {
			prev = step->previous_font_step;
			free (step);
		} while ((step = prev) != NULL);
		current->font_step = NULL;
	}
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
			l_height   = blocker->L.bottom - par->Offset.Y;
			int_width -= blocker->L.width;
			blocked   |= (BRK_LEFT & ~BRK_LN);
		}
		if (blocker->R.bottom) {
			r_height   = blocker->R.bottom - par->Offset.Y;
			int_width -= blocker->R.width;
			blocked   |= (BRK_RIGHT & ~BRK_LN);
		}
	}
	par->Height = 0;
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
		offset += hanging;

		if (blocked)
			offset += blocker->L.width;
		
		while(1) {
			word->line = line;
			if (word->image) {
				/* Origin already set in new_image() */
				word->image->offset.X = offset;
				word->image->offset.Y = par->Height;
				if (word->vertical_align != ALN_TOP) {
					word->image->offset.Y += line->Ascend - word->word_height;
				} else {
					word->word_height    = line->Ascend;
					word->word_tail_drop = img_h;
				}
			}
			if (word->link && !word->link->isHref) {
				/* Origin already set in new_named_location() */
				word->link->u.anchor->offset.X = -par->Offset.X;
				word->link->u.anchor->offset.Y = par->Height;
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
		
		par->Height  += line->Ascend;
		line->OffsetY = par->Height;
		par->Height  += line->Descend;
		
		if (blocked) {
			if (blocked & (BRK_LEFT & ~BRK_LN)) {
				if (ln_brk & (BRK_LEFT & ~BRK_LN)) {
					if (par->Height < l_height) {
						line->Descend += l_height - par->Height;
						par->Height = l_height;
					}
				}
				if (par->Height >= l_height) {
					blocked   &= ~(BRK_LEFT & ~BRK_LN);
					int_width += blocker->L.width;
					blocker->L.bottom = blocker->L.width = l_height = 0;
				}
			}
			if (blocked & (BRK_RIGHT & ~BRK_LN)) {
				if (ln_brk & (BRK_RIGHT & ~BRK_LN)) {
					if (par->Height < r_height) {
						line->Descend += r_height - par->Height;
						par->Height = r_height;
					}
				}
				if (par->Height >= r_height) {
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
		par->Width = par->item->word_width;
	} else {
		par->Width = width;
	}
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
	
	while (line) {
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
		area[3] =  par->Height - bot;
	
	} else if (x < word->h_offset) {
		area[2] = word->h_offset;
		word = NULL;
	
	} else if (x >= line->Width) {
		area[0] += line->Width;
		area[2] =  par->Width - line->Width;
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
			ext.g_y += line->Paragraph->Offset.Y - orig->Paragraph->Offset.Y;
			ext.g_x =  line->Paragraph->Offset.X - orig->Paragraph->Offset.X;
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
				short d_x = next_par->Offset.X - par->Offset.X;
				lft     += d_x;
				rgt     += d_x;
				ext.g_h += par->eop_space;
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
content_setup (CONTENT * content, TEXTBUFF current, short margns, short backgnd)
{
	if (current) {
		content->Item   = new_paragraph (current);
		current->backgnd = backgnd;
	} else {
		content->Item   = NULL;
	}
	content->Minimum   = 0;
	content->Width     = 0;
	content->Height    = 0;
	content->Backgnd   = backgnd;
	content->Alignment = ALN_LEFT;
	content->MarginTop = content->MarginBot =
	content->MarginLft = content->MarginRgt = margns;
}


/*============================================================================*/
void
content_destroy (CONTENT * content)
{
	PARAGRPH par = content->Item;

	while (par) {
		PARAGRPH next = par->next_paragraph;
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
		free (par);
		par = next;
	}
	content->Item = NULL;
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

/*==============================================================================
 * content_minimum()
 *
 * Returns the smallest width that is needed for a list of paragraphs.
 */
long
content_minimum (CONTENT * content)
{
	PARAGRPH paragraph = content->Item;
	long     min_width = 0;
	
	while (paragraph) {
		long par_width;
		
		if (paragraph->paragraph_code == PAR_HR ||
		    paragraph->paragraph_code == PAR_TABLE) {
			
			par_width = paragraph->min_width
		             + paragraph->Indent + paragraph->Rindent;
		
		} else {
			WORDITEM  word = paragraph_filter (paragraph);
			BOOL      lbrk = TRUE;
			long wrd_width = (paragraph->Hanging > 0 ? +paragraph->Hanging : 0);
			long hanging   = (paragraph->Hanging < 0 ? -paragraph->Hanging : 0);
			par_width = 0;
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
				
				if (par_width < wrd_width) {
					 par_width = wrd_width;
				}
				wrd_width = hanging;
				lbrk = TRUE;
			}
			if (par_width < wrd_width) {
				 par_width = wrd_width;
			}
			paragraph->min_width = par_width;
			par_width += paragraph->Indent + paragraph->Rindent;
		}
		if (min_width < par_width) {
			 min_width = par_width;
		}
		paragraph = paragraph->next_paragraph;
	}
	
	min_width += content->MarginLft + content->MarginRgt;
	
	return (content->Minimum = min_width);
}


/*==============================================================================
 * content_maximum()
 *
 * Returns the largest width that occures in a list of paragraphs.
 */
long
content_maximum (CONTENT * content)
{
	PARAGRPH paragraph = content->Item;
	long     max_width = 0;
	
	while (paragraph) {
		if (!paragraph->max_width) {
			struct word_item * word = paragraph->item;
			long width = paragraph->Indent + paragraph->Rindent;
			while (word) {
				BOOL ln_brk = word->line_brk;
				width += word->word_width;
				word = word->next_word;
				if (ln_brk || !word) {
					if (paragraph->max_width < width) {
						 paragraph->max_width = width;
					}
					width = paragraph->Indent;
				}
			}
		}
		if (max_width < paragraph->max_width) {
			 max_width = paragraph->max_width;
		}
		paragraph = paragraph->next_paragraph;
	}
	return (max_width + content->MarginLft + content->MarginRgt);
}


/*==============================================================================
 */
long
content_calc (CONTENT * content, long set_width)
{
	PARAGRPH paragraph = content->Item;
	long     height    = content->MarginTop;
	struct blocking_area blocker = { {0, 0}, {0, 0} };
	
	content->Width = set_width;
	set_width     -= content->MarginLft + content->MarginRgt;
	
	while (paragraph) {
		long par_width = set_width - paragraph->Indent - paragraph->Rindent;
		long blk_width = set_width - blocker.L.width - blocker.R.width;
		paragraph->Offset.X = content->MarginLft + paragraph->Indent;
		paragraph->Offset.Y = height;

		if (paragraph->min_width > blk_width) {
			height = (blocker.L.bottom >= blocker.R.bottom
			          ? blocker.L.bottom : blocker.R.bottom);
			paragraph->Offset.Y = height;
			blocker.L.bottom = blocker.L.width =
			blocker.R.bottom = blocker.R.width = 0;
			blk_width = par_width;
		} else {
			blk_width -= paragraph->Indent + paragraph->Rindent;
		}
		
		if (paragraph->paragraph_code == PAR_HR) {
			struct word_item * word = paragraph->item;
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
				if (paragraph->alignment <= ALN_JUSTIFY) {
					word->h_offset   = 0;
				} else {
					word->h_offset   =  blk_width - word->word_width;
					if (paragraph->alignment == ALN_CENTER) {
						word->h_offset /= 2;
					}
				}
			}
			paragraph->Offset.X += blocker.L.width;
			paragraph->Width  = par_width;
			paragraph->Height = word->word_height *2
			                  + (word->word_tail_drop > 0
			                     ? +word->word_tail_drop : -word->word_tail_drop);
			paragraph->eop_space = 0;
			
		} else if (paragraph->paragraph_code == PAR_TABLE) {
			paragraph->Offset.X += blocker.L.width;
			table_calc (paragraph->Table, par_width);
		
		} else {   /* normal text or image */
			if (paragraph->paragraph_code == PAR_IMG) {
				paragraph->Offset.X += blocker.L.width;
			}
			paragraph_calc (paragraph, par_width, &blocker);
		}
		
		switch (paragraph->floating) {
			case ALN_LEFT: {
				long new_bottom = height + paragraph->Height;
				if (blocker.L.bottom < new_bottom)
					 blocker.L.bottom = new_bottom;
				blocker.L.width += paragraph->Width;
			}	break;
			case ALN_RIGHT: {
				long new_bottom = height + paragraph->Height;
				if (blocker.R.bottom < new_bottom)
					 blocker.R.bottom = new_bottom;
				paragraph->Offset.X += blk_width - paragraph->Width; 
				blocker.R.width += paragraph->Width;
			}	break;
			case ALN_CENTER:
				paragraph->Offset.X += (blk_width - paragraph->Width) /2;
				blk_width = 0; /* avoid double centering */
			case ALN_NO_FLT:
				if (paragraph->Width < blk_width
				    && paragraph->alignment > ALN_JUSTIFY) {
					short indent = blk_width - paragraph->Width;
					if (paragraph->alignment == ALN_CENTER) indent /= 2;
					paragraph->Offset.X += indent;
				}
				height += paragraph->Height + paragraph->eop_space;
				if (blocker.L.bottom && blocker.L.bottom < height) {
					blocker.L.bottom = blocker.L.width = 0;
				}
				if (blocker.R.bottom && blocker.R.bottom < height) {
					blocker.R.bottom = blocker.R.width = 0;
				}
		}
		
		paragraph = paragraph->next_paragraph;
	}

	if (height < blocker.L.bottom) {
		 height = blocker.L.bottom;
	}
	if (height < blocker.R.bottom) {
		 height = blocker.R.bottom;
	}
	return (content->Height = height + content->MarginBot);
}


/*==============================================================================
 */
void
content_stretch (CONTENT * content, long height, V_ALIGN valign)
{
	long offset = height - content->Height;
	
	content->Height = height;
	
	if (valign && offset) {
		PARAGRPH par = content->Item;
		if (valign == ALN_MIDDLE) offset /= 2;
		while (par) {
			par->Offset.Y += offset;
			par = par->next_paragraph;
		}
	}
}


/*============================================================================*/
PARAGRPH
content_paragraph (CONTENT * content, long x, long y, long area[4])
{
	PARAGRPH par = content->Item;
	
	if (!par) {                     /* nothing in it at all */
		area[2] = content->Width;
		area[3] = content->Height;
	
	} else if (par->Offset.Y > y) { /* above all paragraphs */
		area[2] = content->Width;
		area[3] = par->Offset.Y;
		par = NULL;
	
	} else { /* search the paragraph that includes the y coordinate.  also find
	          * left/right aligned paragraphs that might affect the resulting
	          * rectangle.
		      */
		PARAGRPH l_par = NULL, r_par = NULL, flt = NULL;
		long l_bot = 0, r_bot = 0;
		long top = par->Offset.Y, bot;
		do {
			bot = top + par->Height;
			if (par->floating == ALN_RIGHT) {
				if (r_bot <= top) r_par = flt = par;
				if (r_bot <  bot) r_bot = bot;
			} else if (par->floating == ALN_LEFT) {
				if (l_bot <= top) l_par = flt = par;
				if (l_bot <  bot) l_bot = bot;
			}
			if (bot > y) { /* this paragraph includes y, ignoring x here */
				break;
			}
			if ((par = par->next_paragraph) == NULL) { /* below the last */
				top = bot;                              /* paragraph      */
				bot = content->Height;
			
			} else if ((top = par->Offset.Y) > y) { /* between paragraphs */
				top = bot;
				bot = par->Offset.Y;
				par = NULL;
			}
		} while (par);
		
		if (!par) {       /* below all paragraphs */
			area[1] += top;
			area[2] =  content->Width;
			area[3] =  bot - top;
		
		} else {
			top = bot = 0;
			if (r_bot <= top) { flt = l_par; r_par = NULL; r_bot = 0; }
			if (l_bot <= top) { flt = r_par; l_par = NULL; l_bot = 0; }
			if (r_par && l_par) flt = (flt == r_par ? l_par : r_par);
			if (flt) {
				long lft = 0, rgt = content->Width;
				
				while (par->next_paragraph) {
					PARAGRPH next = par->next_paragraph;
					if (next->Offset.Y > y) {
						break;
					}
					par = next;
					if (par->floating == ALN_RIGHT) {
						if (r_bot < par->Offset.Y + par->Height) {
							r_bot = par->Offset.Y + par->Height;
						}
					} else if (par->floating == ALN_LEFT) {
						if (l_bot < par->Offset.Y + par->Height) {
							l_bot = par->Offset.Y + par->Height;
						}
					}
				}
				do {
					if (x < flt->Offset.X) {
						if (flt->floating == ALN_RIGHT) {
							rgt = flt->Offset.X;
						} else if (y < l_bot) {
							bot = l_bot;
							par = flt; /* break condition */
						} else {
							top = l_bot;
						}
					
					} else if (x >= flt->Offset.X + flt->Width) {
						if (flt->floating == ALN_LEFT) {
							lft = flt->Offset.X + flt->Width;
						} else if (y < r_bot) {
							bot = r_bot;
							par = flt; /* break condition */
						} else {
							top = r_bot;
							rgt = flt->Offset.X;
						}
					
					} else if (y < flt->Offset.Y + flt->Height) {
						if (top < flt->Offset.Y) top = flt->Offset.Y;
						bot = flt->Offset.Y + flt->Height;
						par = flt;
						break;
				
					} else if (flt == par) { /* between two paragraphs */
						top = par->Offset.Y + par->Height;
						area[0] += lft;
						area[1] += top;
						area[2] =  rgt - lft;
						area[3] =  par->next_paragraph->Offset.Y - top;
						return NULL;
						
					} else if (flt->floating == ALN_LEFT) {
						if (y < l_bot) {
							top = flt->Offset.Y + flt->Height;
							bot = l_bot;
							par = NULL;
							break;
					/*	} else {
							top = l_bot;
					*/	}
						lft = flt->Offset.X + flt->Width;
					
					} else if (flt->floating == ALN_RIGHT) {
						if (y < r_bot) {
							top = flt->Offset.Y + flt->Height;
							bot = r_bot;
							par = NULL;
							break;
					/*	} else {
							top = r_bot;
					*/	}
						rgt = flt->Offset.X;
					}
				} while ((flt = (flt == par ? NULL : flt->next_paragraph)) != NULL);
				
				if (flt) {
					area[0] += flt->Offset.X;
					area[1] += top;
					area[2] =  flt->Width;
					area[3] =  bot - top;
					return par;
				}
			}
		
			if (top < par->Offset.Y) {
				top = par->Offset.Y;
			}
			if (bot < par->Offset.Y + par->Height) {
				bot = par->Offset.Y + par->Height;
			}

			if (x < par->Offset.X) {
				area[1] += top;
				area[2] =  par->Offset.X;
				area[3] =  bot - top;
				par = NULL;
			
			} else if (x >= par->Offset.X + par->Width) {
				area[0] += par->Offset.X + par->Width;
				area[1] += top;
				area[2] =  content->Width - (par->Offset.X + par->Width);
				area[3] =  bot - top;
				par = NULL;
			
			} else {
				area[0] += par->Offset.X;
				area[1] += par->Offset.Y;
				area[2] =  par->Width;
				area[3] =  par->Height;
			}
		}
	}
	return par;
}
