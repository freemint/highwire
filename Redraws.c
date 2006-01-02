/* @(#)highwire/Redraws.c
 */
#include <stdlib.h>

#include <gemx.h>

#include "global.h"
#include "Form.h"
#include "fontbase.h"

#include "Location.h" /* loc->Flags */


/*==============================================================================
 * Patch for gemlib <= 0.42.2
*/
#if (__GEMLIB_MINOR__<42)||((__GEMLIB_MINOR__==42)&&(__GEMLIB_REVISION__<=2))
void
vsf_udpat (short handle, short pat[], short n_planes)
{
	vdi_params.intin = pat;
	
	vdi_control[0] = 112;
	vdi_control[1] = 0;
	vdi_control[3] = n_planes *16;
	vdi_control[5] = 0;
	vdi_control[6] = handle;
	vdi (&vdi_params);

	vdi_params.intin = vdi_intin;
}
#endif


/*============================================================================*/
void
frame_draw (FRAME frame, const GRECT * p_clip, void * highlight)
{
	GRECT clip = *p_clip;
	PXY   bgnd[2];

	bgnd[1].p_x = (bgnd[0].p_x = frame->clip.g_x) + frame->clip.g_w -1;
	bgnd[1].p_y = (bgnd[0].p_y = frame->clip.g_y) + frame->clip.g_h -1;

	v_hide_c (vdi_handle);

	if (clip.g_x < bgnd[0].p_x || clip.g_x + clip.g_w -1 > bgnd[1].p_x ||
	    clip.g_y < bgnd[0].p_y || clip.g_y + clip.g_h -1 > bgnd[1].p_y) {
		/*
		 * draw borders and/or slider
		*/
		PXY l[5];
		int corner = 0;
		
		if (frame->scroll != SCROLL_NEVER)
		{
			vsf_color (vdi_handle, (ignore_colours ? G_WHITE : G_LWHITE));
			
			if (frame->v_bar.on && clip.g_x + clip.g_w -1 > bgnd[1].p_x) {
				draw_vbar (frame, TRUE);
				corner++;
			}

			if (frame->h_bar.on && clip.g_y + clip.g_h -1 > bgnd[1].p_y) {
				draw_hbar (frame, TRUE);
				corner++;
			}

			if (corner == 2) {
				short b  = scroll_bar_width - 2;
				l[1].p_x = (l[0].p_x = bgnd[1].p_x +2) + b;
				l[1].p_y = (l[0].p_y = bgnd[1].p_y +2) + b;
				vsf_color (vdi_handle, frame->Page.Backgnd);
				v_bar (vdi_handle, (short*)l);
			}
		}

		/* adjust the clipping rectangle to the contents extent
		*/
		if (rc_intersect (&frame->clip, &clip)) {
			l[1].p_x = (l[0].p_x = clip.g_x) + clip.g_w -1;
			l[1].p_y = (l[0].p_y = clip.g_y) + clip.g_h -1;
			vs_clip_pxy (vdi_handle, l);

		} else {
			v_show_c (vdi_handle, 1);
			return;
		}
	}
	
	dombox_draw (&frame->Page,
	             (long)frame->clip.g_x - frame->h_bar.scroll,
	             (long)frame->clip.g_y - frame->v_bar.scroll,
	             &clip, highlight);
	v_show_c (vdi_handle, 1);
}


/*============================================================================*/
void
draw_vbar (FRAME frame, BOOL complete)
{
	short lft = frame->clip.g_x + frame->clip.g_w +1;
	short rgt = lft + scroll_bar_width -3;
	short top = frame->clip.g_y + frame->v_bar.lu;
	short bot = frame->clip.g_y + frame->v_bar.rd;
	PXY p[8];

	vsl_color (vdi_handle, G_BLACK);

	if (complete) {
		short n;
		p[0].p_x = p[1].p_x = lft -1;
		p[0].p_y            = frame->clip.g_y + frame->clip.g_h -1;
		p[1].p_y            = frame->clip.g_y;
		p[2].p_x = p[3].p_x = rgt +1;
		p[2].p_y = p[1].p_y;
		p[3].p_y = p[0].p_y;
		if (frame->h_bar.on) {
			p[0].p_y += scroll_bar_width;
			n = 4;
		} else {
			p[4] = p[0];
			n = 5;
		}
		v_pline (vdi_handle, n, (short*)p);
		
		p[0].p_y = top - (scroll_bar_width -2);
		p[1].p_y = top - 1;
		p[2].p_y = bot + 1;
		p[3].p_y = bot + (scroll_bar_width -2);
		p[0].p_x = p[2].p_x = lft;
		p[1].p_x = p[3].p_x = rgt;
		v_bar (vdi_handle, (short*)(p +0));  /* up arrow area   */
		v_bar (vdi_handle, (short*)(p +2));  /* down arrow area */
		p[0].p_y = ++p[1].p_y;
		p[3].p_y = --p[2].p_y;
		v_pline (vdi_handle, 2, (short*)(p +0));/* border up arrow bottom */
		v_pline (vdi_handle, 2, (short*)(p +2));/* border down arrow top  */

		p[0].p_x = p[7].p_x = lft + (scroll_bar_width -2) /2;
		p[1].p_x = p[6].p_x = lft + 1;
		p[2].p_x = p[5].p_x = rgt - 1;
		p[0].p_y            = top - (scroll_bar_width -3);
		p[1].p_y = p[2].p_y = top - 3;
		p[3]     = p[0];
		p[7].p_y            = bot + (scroll_bar_width -3);
		p[6].p_y = p[5].p_y = bot + 3;
		p[4]     = p[7];
		
		/* arrow buttons */
		if (ignore_colours) {
			v_pline (vdi_handle, 4, (short*)(p +0));
			v_pline (vdi_handle, 4, (short*)(p +4));
		} else {
			GRECT b;
			b.g_w = b.g_h = scroll_bar_width -2;
			b.g_x = lft;
			b.g_y = top - (scroll_bar_width -2);
			draw_border (&b, G_WHITE, G_LBLACK, 1);
			b.g_y = bot +1;
			draw_border (&b, G_WHITE, G_LBLACK, 1);
			v_pline (vdi_handle, 2, (short*)(p +0));
			v_pline (vdi_handle, 3, (short*)(p +5));
			vsl_color (vdi_handle, G_WHITE);
			v_pline (vdi_handle, 3, (short*)(p +1));
			v_pline (vdi_handle, 2, (short*)(p +4));
		}
	} else {
		v_hide_c (vdi_handle);
	}

	/* slider background area */

	p[0].p_x = p[2].p_x = lft;
	p[1].p_x = p[3].p_x = rgt;
	p[0].p_y = top +1;
	p[1].p_y = bot -1;
	vswr_mode    (vdi_handle, MD_REPLACE);
	vsf_interior (vdi_handle, FIS_PATTERN);
	vsf_style    (vdi_handle, 4);
	vsf_color    (vdi_handle, G_LBLACK);
	v_bar        (vdi_handle, (short*)p);

	/* slider control box */
	p[2].p_y = frame->clip.g_y + frame->v_bar.pos  +1;
	p[3].p_y = p[2].p_y        + frame->v_bar.size -3;
	vsf_interior (vdi_handle, FIS_SOLID);
	vsf_color    (vdi_handle, (ignore_colours ? G_WHITE : G_LWHITE));
	v_bar        (vdi_handle, (short*)(p +2));

	if (!ignore_colours) {
		GRECT b;
		*(PXY*)&b = p[2];
		b.g_w = scroll_bar_width -2;
		b.g_h = p[3].p_y - p[2].p_y +1;
		draw_border (&b, G_WHITE, G_LBLACK, 1);
		vsl_color (vdi_handle, G_BLACK);
	}
	p[1].p_y = --p[2].p_y;
	p[4].p_y = ++p[3].p_y;
	p[4].p_x = lft;
	if (p[1].p_y > top) v_pline (vdi_handle, 2, (short*)(p +1));
	if (p[4].p_y < bot) v_pline (vdi_handle, 2, (short*)(p +3));

	if (!complete) {
		v_show_c (vdi_handle, 1);
	}
	vswr_mode (vdi_handle, MD_TRANS);
}


/*============================================================================*/
void
draw_hbar (FRAME frame, BOOL complete)
{
	short top = frame->clip.g_y + frame->clip.g_h +1;
	short bot = top + scroll_bar_width -3;
	short lft = frame->clip.g_x + frame->h_bar.lu;
	short rgt = frame->clip.g_x + frame->h_bar.rd;
	PXY p[8];

	vsl_color (vdi_handle, G_BLACK);

	if (complete) {
		short n;
		p[0].p_y = p[1].p_y = top -1;
		p[0].p_x            = frame->clip.g_x + frame->clip.g_w -1;
		p[1].p_x            = frame->clip.g_x;
		p[2].p_y = p[3].p_y = bot +1;
		p[2].p_x = p[1].p_x;
		p[3].p_x = p[0].p_x;
		if (frame->v_bar.on) {
			p[0].p_x += scroll_bar_width;
			n = 4;
		} else {
			p[4] = p[0];
			n = 5;
		}
		v_pline (vdi_handle, n, (short*)p);

		p[0].p_x = lft - (scroll_bar_width -2);
		p[1].p_x = lft - 1;
		p[2].p_x = rgt + 1;
		p[3].p_x = rgt + (scroll_bar_width -2);
		p[0].p_y = p[2].p_y = top;
		p[1].p_y = p[3].p_y = bot;
		v_bar (vdi_handle, (short*)(p +0));
		v_bar (vdi_handle, (short*)(p +2));
		p[0].p_x = ++p[1].p_x;
		p[3].p_x = --p[2].p_x;
		v_pline (vdi_handle, 2, (short*)(p +0));
		v_pline (vdi_handle, 2, (short*)(p +2));

		p[0].p_y = p[7].p_y = top + (scroll_bar_width -2) /2;
		p[1].p_y = p[6].p_y = top + 1;
		p[2].p_y = p[5].p_y = bot - 1;
		p[0].p_x            = lft - (scroll_bar_width -3);
		p[1].p_x = p[2].p_x = lft - 3;
		p[3]     = p[0];
		p[7].p_x            = rgt + (scroll_bar_width -3);
		p[6].p_x = p[5].p_x = rgt + 3;
		p[4]     = p[7];
		if (ignore_colours) {
			v_pline (vdi_handle, 4, (short*)(p +0));
			v_pline (vdi_handle, 4, (short*)(p +4));
		} else {
			GRECT b;
			b.g_w = b.g_h = scroll_bar_width -2;
			b.g_y = top;
			b.g_x = lft - (scroll_bar_width -2);
			draw_border (&b, G_WHITE, G_LBLACK, 1);
			b.g_x = rgt + 1;
			draw_border (&b, G_WHITE, G_LBLACK, 1);
			v_pline (vdi_handle, 2, (short*)(p +0));
			v_pline (vdi_handle, 3, (short*)(p +5));
			vsl_color (vdi_handle, G_WHITE);
			v_pline (vdi_handle, 3, (short*)(p +1));
			v_pline (vdi_handle, 2, (short*)(p +4));
		}

	} else {
		v_hide_c (vdi_handle);
	}

	p[0].p_y = p[2].p_y = top;
	p[1].p_y = p[3].p_y = bot;
	p[0].p_x = lft +1;
	p[1].p_x = rgt -1;
	vswr_mode    (vdi_handle, MD_REPLACE);
	vsf_interior (vdi_handle, FIS_PATTERN);
	vsf_style    (vdi_handle, 4);
	vsf_color    (vdi_handle, G_LBLACK);
	v_bar        (vdi_handle, (short*)p);

	p[2].p_x = frame->clip.g_x + frame->h_bar.pos  +1;
	p[3].p_x = p[2].p_x        + frame->h_bar.size -3;
	vsf_interior (vdi_handle, FIS_SOLID);
	vsf_color    (vdi_handle, (ignore_colours ? G_WHITE : G_LWHITE));
	v_bar        (vdi_handle, (short*)(p +2));

	if (!ignore_colours) {
		GRECT b;
		*(PXY*)&b = p[2];
		b.g_w     = p[3].p_x - p[2].p_x +1;
		b.g_h     = scroll_bar_width -2;
		draw_border (&b, G_WHITE, G_LBLACK, 1);
		vsl_color (vdi_handle, G_BLACK);
	}
	p[1].p_x = --p[2].p_x;
	p[4].p_x = ++p[3].p_x;
	p[4].p_y = top;
	if (p[1].p_x > lft) v_pline (vdi_handle, 2, (short*)(p +1));
	if (p[4].p_x < rgt) v_pline (vdi_handle, 2, (short*)(p +3));

	if (!complete) {
		v_show_c (vdi_handle, 1);
	}
	vswr_mode (vdi_handle, MD_TRANS);
}


/*------------------------------------------------------------------------------
 * draw_hr()
 *
 * current_paragraph - our paragraph descriptor for the hr tag
 * x,y and w - the location and restraining width for the hr tag
 */
void
draw_hr (PARAGRPH paragraph , WORD x, WORD y)
{
	struct word_item * word = paragraph->item;

	/* I changed the line below to frame_w, with
	 * the old code the horizontal rulers didn't not
	 * cover the frame as they are supposed to
	 * baldrick - July 13, 2001
	 *
	 * Alright I made another modification it now substracts
	 * 10 from the length and starts at 5 on a default
	 * This gives it that slight offset that you might be
	 * used to with other browsers
	 * baldrick - July 18, 2001
	 *
	 * Completely reworked:
	 * - hr_wrd->space_width:    line width (absolute or negative percent)
	 * - hr_wrd->word_height:    vertical distance to the line
	 * - hr_wrd->word_tail_drop: line size (height, negative for 'noshade')
	 * AltF4 - July 21, 2002
	 *
	 */
	GRECT hr;
	hr.g_x = x + word->h_offset;
	hr.g_y = y + word->word_height;
	hr.g_w = word->word_width;
	if (word->word_tail_drop >= 2) {
		hr.g_h = word->word_tail_drop;
		draw_border (&hr, G_LBLACK, G_WHITE, 1);
		if (word->word_tail_drop > 2) {
			hr.g_x += 1;
			hr.g_w -= 2;
			hr.g_y += 1;
			hr.g_h -= 2;
		} else {
			hr.g_h = 0;
		}
	} else {
		hr.g_h = (word->word_tail_drop < 0 ? -word->word_tail_drop : 1);
	}
	if (hr.g_w && hr.g_h) {
		hr.g_w += hr.g_x -1;
		hr.g_h += hr.g_y -1;
		vsf_color (vdi_handle, TA_Color(word->attr));
		vr_recfl  (vdi_handle, (short*)&hr);
	}
}


/*----------------------------------------------------------------------------*/
static short
draw_image (IMAGE img, short x, short y, void * highlight)
{
	static unsigned short filler_sml[1*16] = {
		0xffff,0x8001,0x8401,0x8e71,0x8e89,0x9f05,0x9f05,0xbf05,
		0x8a89,0x8d71,0x8aa1,0x8d61,0x8aa1,0x8fe1,0x8001,0xffff
	};
	static unsigned short filler_big[2*32] = {
		0xffff,0xffff,0x8000,0x0001,0x8000,0x0001,0x8020,0x0001,
		0x8050,0x0001,0x8070,0x0001,0x80d8,0x1f01,0x80a8,0x60c1,
		0x8154,0x8021,0x81ad,0x0011,0x8357,0x0011,0x82aa,0x0009,
		0x8556,0x0009,0x86aa,0x0009,0x8d56,0x0009,0x8aaa,0x0009,
		0x9fff,0x0011,0x8081,0x0011,0x80aa,0x8021,0x8080,0x6041,
		0x80aa,0x9f81,0x8080,0x0201,0x80aa,0xaa01,0x8080,0x0201,
		0x80aa,0xaa01,0x8080,0x0201,0x80aa,0xaa01,0x8080,0x0201,
		0x80ff,0xfe01,0x8000,0x0001,0x8000,0x0001,0xffff,0xffff
	};
	static unsigned short blcked_sml[1*16] = {
		0x0000,
	};
	static unsigned short blcked_big[2*32] = {
		0x0000,
	};
	static MFDB mfdb_sml = { filler_sml, 16,16,1, 0, 1, 0,0,0 };
	static MFDB mfdb_big = { filler_big, 32,32,2, 0, 1, 0,0,0 };
	
	short offs;
	MFDB scrn = { NULL, }, * mfdb;
	short colors[2] = { G_BLACK, G_WHITE };
	PXY p[7] = { {0, 0}, };
	p[2].p_x = x;
	p[2].p_y = y;

	(void)highlight; /* not used yet */
	
	if (img->source->Flags & (1uL << ('I' - 'A'))) {
		if (!blcked_big[0]) {
			long  m0 = 0xE000;
			long  m1 = 0x0007;
			short i, j;
			blcked_sml[0] = filler_sml[0];
			blcked_big[0] = filler_big[0];
			blcked_big[1] = filler_big[1];
			for (i = 1, j = 2; i <= 15; i++, j += 2) {
				blcked_sml[i]        = (filler_sml[i] ^ (m0 | m1)) | 0x8001;
				blcked_big[j+0]      = (filler_big[j+0]      ^ m0) | 0x8000;
				blcked_big[2*32-j-2] = (filler_big[2*32-j-2] ^ m0) | 0x8000;
				blcked_big[j+1]      = (filler_big[j+1]      ^ m1) | 0x0001;
				blcked_big[2*32-j-1] = (filler_big[2*32-j-1] ^ m1) | 0x0001;
				m0 >>= 1;
				m1 <<= 1;
			}
			blcked_sml[15]     = filler_sml[15];
			blcked_big[2*32-2] = filler_big[2*32-2];
			blcked_big[2*32-1] = filler_big[2*32-1];
		}
		mfdb_big.fd_addr = blcked_big;
		mfdb_sml.fd_addr = blcked_sml;
	} else {
		mfdb_big.fd_addr = filler_big;
		mfdb_sml.fd_addr = filler_sml;
	}
	if (img->u.Mfdb) {
		colors[0] = img->u.Data->fgnd;
		colors[1] = img->u.Data->bgnd;
		mfdb = img->u.Mfdb;
		offs = 0;
	} else if (img->disp_w >= 32 && img->disp_h >= 32) {
		mfdb = &mfdb_big;
		offs = (img->disp_w > 33 ? 33 : 0);
	} else if (img->disp_w >= 10 && img->disp_h >= 10) {
		mfdb = &mfdb_sml;
		offs = (img->disp_w > 17 ? 17 : 0);
	} else {
		mfdb = NULL;
		offs = 1;
	}

	if (img->disp_w > 1 && img->disp_h > 1 &&
	    (!mfdb || mfdb->fd_w < img->disp_w || mfdb->fd_h < img->disp_h)) {
		p[3].p_x = p[4].p_x = (p[5].p_x = x) + img->disp_w -1;
		p[5].p_y = p[4].p_y = (p[3].p_y = y) + img->disp_h -1;
		p[6] = p[2];
		vsl_color (vdi_handle, G_BLACK);
		v_pline   (vdi_handle, 5, (short*)(p +2));
	}
	if (mfdb) {
		p[1].p_x = (mfdb->fd_w < img->disp_w ? mfdb->fd_w : img->disp_w) -1;
		p[1].p_y = (mfdb->fd_h < img->disp_h ? mfdb->fd_h : img->disp_h) -1;
		p[3].p_x = p[2].p_x + p[1].p_x;
		p[3].p_y = p[2].p_y + p[1].p_y;
		if (mfdb->fd_nplanes > 1) {
			vro_cpyfm (vdi_handle, S_ONLY, (short*)p, mfdb, &scrn);
		} else {
			vrt_cpyfm (vdi_handle, MD_REPLACE, (short*)p, mfdb, &scrn, colors);
		}
	}
	return offs;
}


/*------------------------------------------------------------------------------
 * draw_paragraph()
 *
 * This handles any text sort of paragraph rendering
 *
 * current_paragraph - our paragraph descriptor
 * x,y,w and h - the location and restraining width
 * top_of_page ->start of drawing area
 * l_border, r_border and b_border are borders for the text to avoid
 * alignment any default alignment for the paragraph
 */
long
draw_paragraph (PARAGRPH paragraph,
                WORD x_off, long y_off, const GRECT * clip, void * highlight)
{
	WORDLINE line   = paragraph->Line;
	long     clip_y = clip->g_y;
	FONT     font   = NULL;
	TEXTATTR attrib;
	
	while (line && y_off + line->OffsetY + line->Descend <= clip_y) {
		line = line->NextLine;
	}
	clip_y += clip->g_h -1;

	if (!line) {
		return y_off; /* nothing to do */
	
	} else if (!line->Word) {
		puts ("draw_paragraph(): empty line!");
		return 0;
	}
	
	attrib.packed = ~line->Word->attr.packed;
	
	do {
		struct word_item * word = line->Word;
		WCHAR * text = (word->space_width ? word->item +1 : word->item);
		short   n    = line->Count;
		long   y_abs = y_off + line->OffsetY;
		if (y_abs - line->Ascend > clip_y) {
			break;
		}

		if (*text == font_Space (word->font)) {
			text++;
		}

		while(1) {
			short x = x_off + word->h_offset;
			short y = y_abs;

		 	if (!word) {
		 		puts ("draw_paragraph(): empty line (3)!");
		 		break;
		 	} else if (!word->item) {
		 		puts ("draw_paragraph(): empty word (2)!");
		 		break;
		 	}

			attrib.packed ^= word->attr.packed;
			if ((attrib.packed &= (TA_SizeMASK|TA_FaceMASK|TA_AttrMASK|TA_ColrMASK)) != 0) {
				if (font_switch (word->font, font)) {
					vst_effects (vdi_handle,
					           TAgetUndrln (word->attr) | word->font->Base->Effects);
				} else if (TAgetUndrln (attrib)) {
					vst_effects (vdi_handle, TAgetUndrln (word->attr));
				}
				if (TA_Color (attrib)) {
					if (word->link && word->link == highlight) {
						vst_color (vdi_handle, highlighted_link_colour);
					} else {
						vst_color (vdi_handle, TA_Color (word->attr));
					}
				}
			}
			attrib = word->attr;
			font   = word->font;
			
			switch (word->vertical_align) {
				case ALN_BELOW: y += word->word_height /2; break;
				case ALN_ABOVE: y -= word->word_height /2; break;
			/*	case ALN_TOP: 	y -= line->Ascend; break;*/
				default: ;
			}
			
			if (word->image) {
				short offs = draw_image (word->image, x + word->image->hspace,
				                         (y -= word->word_height
				                             - word->image->vspace), highlight);
				if (offs && *text) {
					PXY alt[2];
					alt[0].p_x = x += offs;
					alt[0].p_y = y;
					alt[1].p_x = word->word_width - offs - word->image->hspace -1;
					alt[1].p_y = word->image->disp_h                           -1;
					if (rc_intersect (clip, (GRECT*)alt)) {
						alt[1].p_x += alt[0].p_x -1;
						alt[1].p_y += alt[0].p_y -1;
						y += word->image->alt_h;
						vs_clip_pxy (vdi_handle, alt);
						v_ftext16   (vdi_handle, x + word->image->hspace, y, text);
						alt[1].p_x = (alt[0].p_x = clip->g_x) + clip->g_w -1;
						alt[1].p_y = (alt[0].p_y = clip->g_y) + clip->g_h -1;
						vs_clip_pxy (vdi_handle, alt);
					}
				}

			} else if (word->input) {
				input_draw (word->input, x, y);
			
			} else {
				v_ftext16 (vdi_handle, x, y, text);

				if (TAgetStrike (word->attr)) {
					PXY p[2];
					p[1].p_x = (p[0].p_x = x) + word->word_width  -1;
					p[1].p_y =  p[0].p_y = y  - word->word_height /3;
					if (n == line->Count) {
						p[1].p_x -= word->space_width;
					}
					vsl_color (vdi_handle, TA_Color (word->attr));
					if (word->word_height >= 12) {
						vsl_width (vdi_handle, 3);
					}
					v_pline (vdi_handle, 2, (short*)p);
					vsl_width (vdi_handle, 1);
				}
			}
			if (!--n) break;
			word = word->next_word;
			text = word->item;
		}

	} while ((line = line->NextLine) != NULL);
	
	return y_off;
}


/*============================================================================*/
void
draw_border (const GRECT * rec, short lu, short rd, short width)
{
	PXY p[3];
	short b = width;
	p[2].p_x = (p[1].p_x = p[0].p_x = rec->g_x) + rec->g_w -1;
	p[0].p_y = (p[1].p_y = p[2].p_y = rec->g_y) + rec->g_h -1;
	vsl_color (vdi_handle, lu);
	while(1) {
		v_pline (vdi_handle, 3, (short*)p);
		if (!--b) break;
		p[1].p_x = ++p[0].p_x;
		p[1].p_y = ++p[2].p_y;
		p[2].p_x--;
		p[0].p_y--;
	}
	b = width;
	p[0].p_x++; p[1].p_x = p[2].p_x;
	p[2].p_y++; p[1].p_y = p[0].p_y;
	vsl_color (vdi_handle, rd);
	while(1) {
		v_pline (vdi_handle, 3, (short*)p);
		if (!--b) break;
		p[1].p_x = ++p[2].p_x;
		p[1].p_y = ++p[0].p_y;
		p[0].p_x--;
		p[2].p_y--;
	}
}

/*============================================================================*/
void
draw_TBLR_border (const GRECT * rec, short lu, short rd, TBLR width)
{
	PXY p[3];
	short b	= width.Top;

	p[2].p_x = (p[1].p_x = p[0].p_x = rec->g_x) + rec->g_w -1;
	p[0].p_y = (p[1].p_y = p[2].p_y = rec->g_y) + rec->g_h -1;
	vsl_color (vdi_handle, lu);
	while(1) {
		v_pline (vdi_handle, 3, (short*)p);
		if (!--b) break;
		p[1].p_x = ++p[0].p_x;
		p[1].p_y = ++p[2].p_y;
		p[2].p_x--;
		p[0].p_y--;
	}
	
	b = width.Lft;
	p[0].p_x++; p[1].p_x = p[2].p_x;
	p[2].p_y++; p[1].p_y = p[0].p_y;
	vsl_color (vdi_handle, rd);
	while(1) {
		v_pline (vdi_handle, 3, (short*)p);
		if (!--b) break;
		p[1].p_x = ++p[2].p_x;
		p[1].p_y = ++p[0].p_y;
		p[0].p_x--;
		p[2].p_y--;
	}
}