/* @(#)highwire/Frame.c
 */
#include <stdlib.h>
#include <string.h>

#include <gem.h>

#include "global.h"
#include "Table.h"
#include "Form.h"
#include "Location.h"
#include "mime.h"
#include "fontbase.h"


/*==============================================================================
 *
 * creates a new frame structure and initializes it's values
*/
FRAME
new_frame (LOCATION loc, TEXTBUFF current,
           ENCODING encoding, UWORD mime_type, short margin_w, short margin_h)
{
	FRAME frame = malloc (sizeof (struct frame_item));

	frame->Container    = NULL;
	frame->Location     = location_share (loc);
	frame->BaseHref     = location_share (loc);
	frame->Encoding     = encoding & 0x7Fu;
	frame->ForceEnc     = ((encoding & 0x80u) != 0);
	frame->Language     = LANG_Unknown;
	frame->MimeType     = (mime_type == MIME_TEXT ? MIME_TXT_PLAIN : mime_type);
	frame->h_bar.scroll = 0;
	frame->v_bar.scroll = 0;
	frame->v_bar.on     = FALSE;
	frame->h_bar.on     = FALSE;
	frame->scroll       = SCROLL_AUTO;
	frame->text_color   = current->font->Color;
	frame->link_color   = link_colour;
	frame->base_target  = NULL;
	frame->clip.g_x = 0;
	frame->clip.g_y = 0;
	frame->clip.g_w = 0;
	frame->clip.g_h = 0;
	frame->first_named_location = NULL;
	frame->FormList             = NULL;
	
	current->anchor    = &frame->first_named_location;
	current->quot_lang = frame->Language;
	
	content_setup (&frame->Page, current, NULL, BC_MAIN,
	               page_margin, background_colour);
	
	if (margin_w >= 0) {
		frame->Page.Box.Padding.Lft = frame->Page.Box.Padding.Rgt = margin_w;
	}
	if (margin_h >= 0) {
		frame->Page.Box.Padding.Top = frame->Page.Box.Padding.Bot = margin_h;
	}
	
	if (frame->MimeType == MIME_TXT_PLAIN) {
		font_byType (pre_font, -1, -1, current->word);
	}
	
	return frame;
}


/*============================================================================*/
void
delete_frame (FRAME * p_frame)
{
	FRAME frame = *p_frame;
	if (frame) {
		if (frame->first_named_location)
			destroy_named_location_structure (frame->first_named_location);
		free_location (&frame->Location);
		free_location (&frame->BaseHref);
		if (frame->base_target)
			free (frame->base_target);
		Delete (&frame->Page.Box);
		*p_frame = NULL;
	}
}


/*============================================================================*/
void
frame_finish (FRAME frame, PARSER parser, TEXTBUFF current)
{
	if (current->form) {
		form_finish (current);
	}
	while (current->tbl_stack) {
		table_finish (parser);
	}
	paragrph_finish (current);
	
	dombox_MinWidth (&frame->Page.Box);
}


/*============================================================================*/
/* frame_calculate() and frame_slider() contain unary plus signs to force Pure C
 * to calculate +(...) as a first step and prevent it from arithmetic
 * transformation.
 * Pure C, Apendix F, p. 247
*/
void
frame_calculate (FRAME frame, const GRECT * clip)
{
	long  old_width  = (frame->h_bar.on
	                    ? frame->Page.Box.Rect.W - frame->clip.g_w : 0);
	long  old_height = (frame->v_bar.on
	                    ? frame->Page.Box.Rect.H - frame->clip.g_h : 0);
	short scrollbar_size = scroll_bar_width;
	
	frame->clip = *clip;

	if (!frame->scroll) {
		
		if (frame->Page.Box.MinWidth <= frame->clip.g_w) {
			frame->h_bar.on = FALSE;
			dombox_format (&frame->Page.Box, frame->clip.g_w);
		
		} else {
			frame->h_bar.on = TRUE;
			frame->clip.g_h -= scrollbar_size;
			dombox_format (&frame->Page.Box, frame->Page.Box.MinWidth);
		}
		
		if (frame->Page.Box.Rect.H <= frame->clip.g_h) {
			frame->v_bar.on        = FALSE;
			frame->Page.Box.Rect.H = frame->clip.g_h;
		
		} else {
			frame->v_bar.on = TRUE;
			frame->clip.g_w -= scrollbar_size;
	
			if (!frame->h_bar.on) {
				if (frame->Page.Box.MinWidth > frame->clip.g_w) {
					frame->Page.Box.Rect.W = frame->Page.Box.MinWidth;
					frame->h_bar.on        = TRUE;
					frame->clip.g_h       -= scrollbar_size;
				} else {
				   dombox_format (&frame->Page.Box, frame->clip.g_w);
					if (frame->Page.Box.Rect.H < frame->clip.g_h) {
						 frame->Page.Box.Rect.H = frame->clip.g_h;
					}
				}
			}
		}
		
	} else {
		if ((frame->h_bar.on = frame->v_bar.on
		                     = (frame->scroll == SCROLL_ALWAYS)) == TRUE) {
			frame->clip.g_w -= scrollbar_size;
			frame->clip.g_h -= scrollbar_size;
		}
		if (frame->clip.g_w > frame->Page.Box.MinWidth) {
			dombox_format (&frame->Page.Box, frame->clip.g_w);
		} else {
			dombox_format (&frame->Page.Box, frame->Page.Box.MinWidth);
		}
		if (frame->Page.Box.Rect.H < frame->clip.g_h) {
			 frame->Page.Box.Rect.H = frame->clip.g_h;
		}
	}
	
	if (!frame->h_bar.on) {
		frame->h_bar.scroll = 0;
	
	} else {
		long new_width = frame->Page.Box.Rect.W - frame->clip.g_w;
		if (old_width) {
			long scroll = +(frame->h_bar.scroll * 1024 + old_width /2)
			            / old_width;
			frame->h_bar.scroll = +(scroll * new_width + 512) / 1024;
		}
		if (frame->h_bar.scroll > new_width) {
			 frame->h_bar.scroll = new_width;
		}
		frame->h_bar.lu = scroll_bar_width -1;
		frame->h_bar.rd = frame->clip.g_w - scroll_bar_width +1;
		if (!frame->v_bar.on) {
			frame->h_bar.rd--;
		}
		frame->h_bar.size = (long)(frame->h_bar.rd - frame->h_bar.lu +1)
		                  * frame->clip.g_w / frame->Page.Box.Rect.W;
		if (frame->h_bar.size < scroll_bar_width) {
			 frame->h_bar.size = scroll_bar_width;
		}
		frame_slider (&frame->h_bar, new_width);
	}
	
	if (!frame->v_bar.on) {
		frame->v_bar.scroll = 0;
	
	} else {
		long new_height = frame->Page.Box.Rect.H - frame->clip.g_h;

		if (frame->Page.Box.Rect.H <= frame->clip.g_h) {
			new_height = frame->clip.g_h;
		
		} else {
			if (old_height) {
				long scroll = +(frame->v_bar.scroll * 1024 + old_height /2)
				            / old_height;
				frame->v_bar.scroll = +(scroll * new_height + 512) / 1024;
			}

			 if (frame->v_bar.scroll > new_height) {
				 frame->v_bar.scroll = new_height;
			}
		}

		frame->v_bar.lu = scroll_bar_width -1;
		frame->v_bar.rd = frame->clip.g_h - scroll_bar_width +1;
		if (!frame->h_bar.on) {
			frame->v_bar.rd--;
		}

		if (frame->Page.Box.Rect.H <= frame->clip.g_h) {
			frame->v_bar.size = (long)(frame->v_bar.rd - frame->v_bar.lu +1);
		} else {
			frame->v_bar.size = (long)(frame->v_bar.rd - frame->v_bar.lu +1)
		                     * frame->clip.g_h / frame->Page.Box.Rect.H;
		}
		if (frame->v_bar.size < scroll_bar_width) {
			 frame->v_bar.size = scroll_bar_width;
		}
		frame_slider (&frame->v_bar, new_height);
	}
}


/*============================================================================*/
BOOL
frame_slider (struct slider * slider, long max_scroll)
{
	short size = slider->rd - slider->lu +1 - slider->size;
	short step = +(slider->scroll * size + (max_scroll /2)) / max_scroll
	           - (slider->pos - slider->lu);
	slider->pos += step;
	
	return  (step != 0);
}


/*============================================================================*/
OFFSET *
frame_anchor (FRAME frame, const char * name)
{
	ANCHOR anchor = frame->first_named_location;
	
	while (*name == '#') name++;
	
	while (anchor) {
		if (strcmp (name, anchor->address) == 0) {
			return &anchor->offset;
		}
		anchor = anchor->next_location;
	}
	return NULL;
}


/*==============================================================================
 * frame_paragraph()
 *
 * Returns the frame at the absolute screen coordinates x/y, or NULL if outside
 * of all paragraphs.  In both cases the calculated extents are stored in the
 * long array.  
*/
PARAGRPH
frame_paragraph (FRAME frame, long x, long y, long area[4])
{
	CONTENT * content = &frame->Page;
	PARAGRPH  par;
	area[0] = -frame->h_bar.scroll;
	area[1] = -frame->v_bar.scroll;
	area[2] = 0;
	area[3] = 0;
	x -= (long)frame->clip.g_x - frame->h_bar.scroll;
	y -= (long)frame->clip.g_y - frame->v_bar.scroll;
	
	while ((par = content_paragraph (content, x, y, area)) != NULL
	       && par->Table) {
		long save_x = area[0], save_y = area[1];
		x -= par->Box.Rect.X;
		y -= par->Box.Rect.Y;
		if ((content = table_content (par->Table, x, y, area)) != NULL) {
			x -= area[0] - save_x;
			y -= area[1] - save_y;
			area[2] = content->Box.Rect.W;
			area[3] = content->Box.Rect.H;
			par = content->Item;
		
		} else {
			par = NULL;
			break;
		}
	}
	return par;
}
