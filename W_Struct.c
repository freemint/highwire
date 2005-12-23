#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <gemx.h>

#include "global.h"
#include "fontbase.h"
#include "Table.h"

static struct {
	WCHAR Space, Empty;
} fixed[3] = {
	{ SPACE_BICS,  0 }, /* 561 */
	{ SPACE_ATARI, 0 }, /* ' ' */
	{ SPACE_UNI,   0 }  /* 0x0020 */
};

typedef struct {
	ULONG used;
	struct word_item word[32];
} CHUNK;
#define word2chunk(w) ((CHUNK*)((char*)(w - w->_priv) - offsetof(CHUNK, word)))


/*----------------------------------------------------------------------------*/
static WORDITEM
_alloc (WORDITEM prev)
{
	CHUNK * chunk = NULL;
	UWORD   num;
	
	if (prev && (num = prev->_priv +1) < numberof (chunk->word)) {
		ULONG bit = 1uL << num;
		chunk = word2chunk (prev);
		do {
			if (!(chunk->used & bit)) {
				chunk->used |= bit;
				return &chunk->word[num];
			}
			num++;
		} while ((bit <<= 1) != 0uL);
	}
	
	if ((chunk = malloc (sizeof (CHUNK))) != NULL) {
		char * mem = (char*)chunk + offsetof(CHUNK, word->_priv);
		for (num = 0; num < numberof (chunk->word); num++) {
			*(UWORD*)mem = num;
			mem += offsetof(CHUNK, word[1]) - offsetof(CHUNK, word[0]);
		}
		chunk->used = 1uL;
		
		return &chunk->word[0];
	}
	
	return NULL; /* memory exhausted */
}


/*============================================================================*/
WORDITEM
destroy_word_list (WORDITEM word, WORDITEM last)
{
	if (last) {
		last = last->next_word;
	}
	if (word) do {
		WORDITEM next  = word->next_word;
		CHUNK  * chunk = word2chunk (word);
		if ((char*)word->item < (char*)fixed ||
		    (char*)word->item > (char*)fixed + sizeof(fixed)) {
			free (word->item);
		}
		if (word->link && word->link->start == word) {
			if (next && next->link == word->link) {
				word->link->start = next;
			
			} else {
				if (word->link->address) {
					free (word->link->address);
				}
				if (word->link->isHref && word->link->u.target) {
					free (word->link->u.target);
				}
				free (word->link);
			}
		}
		if (word->image) {
			delete_image (&word->image);
		}
	/*	if (word->input)
			delete_input (&word->input)*/;
		if ((chunk->used &= ~(1uL << word->_priv)) == 0uL) {
			free (chunk);
		}
		word = next;
	} while (word != last);
	
	return last;
}


/*==============================================================================
 * new_word()
 *
 * creates a new word_item object.  if the 'copy_from' is not NULL it is
 * taken as a source to copy attributes from.  the flag 'changed' determines
 * if the members of the changed struct shall be set or cleared.
 * AltF4 - Jan. 20, 2002
 *
 * modified to take frame_item * for inheritance from frame
 * Baldrick - Feb 28, 2002
*/
struct word_item *
new_word (TEXTBUFF current, BOOL do_link)
{
	struct word_item * copy_from = current->word;
	struct word_item * word      = NULL;
	
	if (!copy_from) {
		if ((word = _alloc (NULL)) == NULL) return NULL;
		word->attr.packed    = TEXTATTR (current->font->Size,
			                              current->font->Color);
		word->font           = NULL;
		font_byType (-1, -1, -1, word);
		word->vertical_align = ALN_BOTTOM;
		word->link           = NULL;
	} else /*if (current->text > current->buffer)*/ {
		word_store (current);
		if ((word = _alloc (copy_from)) == NULL) return NULL;
		word->attr.packed    = copy_from->attr.packed;
		word->font           = copy_from->font;
		word->word_height    = copy_from->word_height;
		word->word_tail_drop = copy_from->word_tail_drop;
		word->vertical_align = copy_from->vertical_align;
		word->link           = copy_from->link;
		if (do_link) {
			copy_from->next_word = word;
		} else {
			copy_from = NULL;
		}
	}
	word->item        = &fixed[0].Empty;
	word->length      = 0;
	word->line_brk    = BRK_NONE;
	word->word_width  = 0;
	word->space_width = 0;

	/* Only tables problematic ? */
	if (current->tbl_stack && current->tbl_stack->WorkCell->nowrap) {
		word->wrap        = FALSE;
	} else {
		word->wrap 		  = TRUE;
	}

	word->image       = NULL;
	word->input       = NULL;
	word->line        = NULL;
	word->next_word   = NULL;
	
	current->prev_wrd = copy_from;
	current->word     = word;
	
	return current->word;
}


/*============================================================================*/
void
word_store (TEXTBUFF current)
{
	size_t length = current->text - current->buffer;
	
	if (length > 0) {
		WORDITEM word = current->word;
		FONTBASE base = word->font->Base;
		WORD     pts[8];
		
		if (current->buffer[0] == base->SpaceCode) {
			word->space_width = word->font->SpaceWidth;
			if (!current->nowrap) {
				word->wrap = TRUE;
			}
		}

		if ((word->length = length) == 1 && word->space_width) {
			word->item = &fixed[base->Mapping].Space;
			pts[2] = word->space_width;
		
		} else {
			size_t  size = (length +1) *2;
			WCHAR * item = malloc (size);
			if (!item) {
				item = &fixed[0].Empty; /* memory exhausted */
			
			} else if (base->Mapping == MAP_ATARI) {
				WCHAR * src = current->buffer;
				WCHAR * dst = item;
				UWORD   len = length;
				do {
					*(dst++) = *(src++) & 0x00FF;
				} while (--len);
				*dst = 0;
			
			} else {
				memcpy (item, current->buffer, size);
				item[length] = 0;
			}
			word->item = item;
		
	#if (__GEMLIB_MINOR__<42)||((__GEMLIB_MINOR__==42)&&(__GEMLIB_REVISION__<2))
			vqt_f_extent16 (vdi_handle, word->item, pts);
	#else
			vqt_f_extent16n (vdi_handle, word->item, length, pts);
	#endif
			pts[2] -= pts[0];
		}
		if (word->image) {
			word->image->alt_w = pts[2];
		
		} else {
			word->word_width = pts[2];
		}
	}
	current->text = current->buffer;
}


/*==============================================================================
 * word_set_bold()
 *
 * changes the value of the BOLD status of the current word
*/
void
word_set_bold (TEXTBUFF current, BOOL onNoff)
{
	if (onNoff) {
		if (!current->styles.bold++) {
			TAsetBold (current->word->attr, TRUE);
		}
	} else if (current->styles.bold) {
		if (!--current->styles.bold) {
			TAsetBold (current->word->attr, FALSE);
		}
	}
}


/*==============================================================================
 * word_set_italic()
 *
 * changes the value of the ITALIC status of the current word
*/
void
word_set_italic (TEXTBUFF current, BOOL onNoff)
{
	if (onNoff) {
		if (!current->styles.italic++) {
			TAsetItalic (current->word->attr, TRUE);
		}
	} else if (current->styles.italic) {
		if (!--current->styles.italic) {
			TAsetItalic (current->word->attr, FALSE);
		}
	}
}


/*==============================================================================
 * word_set_strike()
 *
 * changes the value of the STRIKE THROUGH status of the current word
*/
void
word_set_strike (TEXTBUFF current, BOOL onNoff)
{
	if (onNoff) {
		if (!current->styles.strike++) {
			TAsetStrike (current->word->attr, TRUE);
		}
	} else if (current->styles.strike) {
		if (!--current->styles.strike) {
			TAsetStrike (current->word->attr, FALSE);
		}
	}
}


/*==============================================================================
 * word_set_underline()
 *
 * changes the value of the UNDERLINED status of the current word
*/
void
word_set_underline (TEXTBUFF current, BOOL onNoff)
{
	if (onNoff) {
		if (!current->styles.underlined++) {
			TAsetUndrln (current->word->attr, TRUE);
		}
	} else if (current->styles.underlined) {
		if (!--current->styles.underlined) {
			TAsetUndrln (current->word->attr, FALSE);
		}
	}
}


long
word_offset (WORDITEM word)
{
	WORDLINE line = word->line;
	return (line->OffsetY + line->Paragraph->Box.Rect.Y - word->word_height);
}
