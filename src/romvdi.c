/*
 * romvdi.c - see romvdi.h for what this is and why.
 */
#include <stdlib.h>
#include <string.h>

#include <osbind.h>

#include <gemx.h>

#include "global.h"
#include "scanner.h"
#include "romvdi.h"
#include "fontbase.h"

BOOL has_fsm_gdos = FALSE; /* until romvdi_init() says otherwise */
static BOOL has_eddi_cookie = FALSE;

UWORD cur_font_format = 0; /* current font format 1 = Bitmap, 2 = Speedo, 4 = TrueType, 8 = Type-1 */

/* What vst_map_mode() was last asked for.  With a real GDOS the VDI keeps this
 * itself; without one we have to remember it, because it tells us how to read
 * the WCHAR arrays handed to the 16-bit text calls - as Unicode code points, or
 * as Atari characters already widened into 16-bit cells.
 */
static WORD cur_map_mode = MAP_ATARI;

/* Scratch space for narrowing 16-bit text down to 8-bit before handing it to
 * the ROM VDI.  HighWire draws a word at a time and the longest single string
 * is the info bar's URL, so this is comfortably oversized; anything longer is
 * truncated rather than risking the stack on a 68000.
 */
#define NARROW_MAX 1024
static char narrow_buf[NARROW_MAX + 1];

/* getcookie() reads a 32-bit configuration cookie from the system cookie jar.
 * The pointer 'value' can be NULL.
 */
static BOOL getcookie(long cookie, void *value)
{
	typedef struct {
		long id;
		long val;
	} COOKJAR;
	COOKJAR	*cookiejar;
	WORD i;

	cookiejar = (COOKJAR *)Setexc(0x5A0/4, (void (*)())-1);
	if (cookiejar) {
		for (i = 0; cookiejar[i].id; i++)
			if (cookiejar[i].id == cookie) {
				if (value)
					*(long *)value = cookiejar[i].val;
				return TRUE;
			}
	}

	return FALSE;
}


/*----------------------------------------------------------------------------*/
void
romvdi_init (long gdostype)
{
	/* Same test HighWire has always used to decide a Speedo/FSM GDOS is there:
	 * FSM GDOS, NVDI's -65536, or fVDI's signature.  The difference is that we
	 * now record the answer instead of giving up when it is negative.
	 */
	has_fsm_gdos = (gdostype == GDOS_FSM || gdostype == -65536L
	                || memcmp (&gdostype, "fVDI", 4) == 0);
	has_eddi_cookie = getcookie(0x45644449ul, NULL); /* 'EdDI' */
}


/*----------------------------------------------------------------------------*/
/* Narrow a 16-bit string to 8-bit Atari characters for the ROM VDI.
 * num < 0 means "until the NUL terminator".
 * Returns a NUL terminated static buffer, valid until the next call.
 */
static const char *
narrow (const WCHAR * src, WORD num)
{
	char * dst = narrow_buf;
	char * end = narrow_buf + NARROW_MAX - 4; /* unicode_to_8bit may emit >1 */

	if (cur_map_mode == MAP_UNICODE) {
		while (num-- != 0 && *src && dst < end) {
			dst = unicode_to_8bit (*(src++), dst);
		}
	} else {
		/* Already Atari (or Bitstream, for which this is the best we can do)
		 * characters sitting in 16-bit cells - just take the low byte.
		 */
		while (num-- != 0 && *src && dst < end) {
			*(dst++) = (char)*(src++);
		}
	}
	*dst = '\0';

	return narrow_buf;
}

/*----------------------------------------------------------------------------*/
/* Same conversion for a single character. */
static char
narrow_char (WORD ch)
{
	if (cur_map_mode == MAP_UNICODE) {
		char buf[8];
		return (unicode_to_8bit (ch, buf) > buf ? buf[0] : '\0');
	}
	return (char)ch;
}

/*----------------------------------------------------------------------------*/
/* Some drivers in NVDI does not redirect functions in vector fonts calls to old bitmap fonts functions */
WORD
hw_vst_font (WORD handle, FONTBASE base)
{
	WORD ret;
  
  if ((ret = vst_font(handle, base->RealId)) != base->RealId) { return 0; }
  
  cur_font_format = base->Format;

	return ret;
}


/*----------------------------------------------------------------------------*/
WORD
hw_vqt_xfntinfo (WORD handle, WORD flags, WORD id, WORD fnt_index, XFNT_INFO * info)
{
	if (has_fsm_gdos) {
		return vqt_xfntinfo (handle, flags, id, fnt_index, info);
	}

	/* No extended enquiry available, so answer it ourselves.  Ask the VDI
	 * whether it can select the font at all first: on the ROM VDI that is true
	 * only of the system font, but testing rather than assuming means a
	 * non-FSM GDOS with bitmap fonts loaded still gets a truthful answer.
	 *
	 * Saying no here is important.  HighWire's configured ids (5031..5034 by
	 * default) are SpeedoGDOS numbering and do not exist on the ROM VDI;
	 * claiming they do would leave font_byType() drawing with whatever font
	 * happened to be current.  A refusal instead sends it down the fallback
	 * chain it already has, which ends at the system font.
	 */
	if (vst_font (handle, id) != id) {
		return 0;
	}

	/* What the ROM VDI has is an Atari mapped bitmap font (format 1) whose id
	 * is its own real id.  This is the call whose failure used to take
	 * font_base() - and with it HighWire - down.
	 */
	info->format = 1;
	info->id     = id;
	info->index  = fnt_index;
	strcpy (info->font_name,   (id == 1 ? "System" : "Bitmap"));
	strcpy (info->family_name, info->font_name);
	strcpy (info->style_name,  "Regular");
	info->file_name1[0] = '\0';
	info->file_name2[0] = '\0';
	info->file_name3[0] = '\0';
	info->pt_cnt = 0;

	return 1;
}


/*----------------------------------------------------------------------------*/
void
hw_vst_map_mode (WORD handle, WORD mode)
{
	cur_map_mode = mode;
	if (has_fsm_gdos && (cur_font_format > 1)) {
		vst_map_mode (handle, mode);
	}
}


/*----------------------------------------------------------------------------*/
WORD
hw_vst_arbpt (WORD handle, WORD point, WORD * wc, WORD * hc, WORD * wb, WORD * hb)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
		return vst_arbpt (handle, point, wc, hc, wb, hb);
	}
	/* vst_point() is the GEM 1.0 equivalent: it snaps to whatever fixed sizes
	 * the font actually offers rather than scaling to an arbitrary one.
	 */
	return vst_point (handle, point, wc, hc, wb, hb);
}


/*----------------------------------------------------------------------------*/
void
hw_vst_setsize32 (WORD handle, LONG point, WORD * wc, WORD * hc, WORD * wb, WORD * hb)
{
	if (has_fsm_gdos) {
		vst_setsize32 (handle, point, wc, hc, wb, hb);
	}
	/* else: no condensed text on the ROM VDI - leave the size alone. */
}


/*----------------------------------------------------------------------------*/
void
hw_vqt_advance (WORD handle, WORD ch,
                WORD * advx, WORD * advy, WORD * remx, WORD * remy)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
		vqt_advance (handle, ch, advx, advy, remx, remy);
		return;
	}
	/* The ROM VDI has no sub-pixel advance; the character cell width is the
	 * advance and there is no remainder.  Note vqt_width() returns a status
	 * (the character, or -1) - the width arrives in its first output.
	 */
	if (vqt_width (handle, narrow_char (ch), advx, remx, remy) < 0) {
		*advx = 0; /* character not in the font */
	}
	*advy = *remx = *remy = 0;
}


/*----------------------------------------------------------------------------*/
void
hw_v_ftext (WORD handle, WORD x, WORD y, const char * str)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
		v_ftext (handle, x, y, str);
	} else {
		v_gtext (handle, x, y, str);
	}
}


/*----------------------------------------------------------------------------*/
void
hw_v_ftext16 (WORD handle, WORD x, WORD y, const WCHAR * str)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
		v_ftext16 (handle, x, y, str);
	} else {
		v_gtext (handle, x, y, narrow (str, -1));
	}
}


/*----------------------------------------------------------------------------*/
void
hw_v_ftext16n (WORD handle, WORD x, WORD y, const WCHAR * str, WORD num)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
#ifdef __PUREC__
		v_ftext16n (handle, x, y, str, num);
#else
		PXY pos;
		pos.p_x = x;
		pos.p_y = y;
		v_ftext16n (handle, pos, str, num);
#endif
	} else {
		v_gtext (handle, x, y, narrow (str, num));
	}
}


/*----------------------------------------------------------------------------*/
void
hw_vqt_f_extent16n (WORD handle, const WCHAR * str, WORD num, WORD * extent)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
		vqt_f_extent16n (handle, str, num, extent);
	} else {
		vqt_extent (handle, narrow (str, num), extent);
	}
}


/*----------------------------------------------------------------------------*/
void
hw_vst_scratch (WORD handle, WORD mode)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
		vst_scratch (handle, mode);
	}
}


/*----------------------------------------------------------------------------*/
void
hw_vst_kern (WORD handle, WORD tmode, WORD pmode, WORD * tracks, WORD * pairs)
{
	if (has_fsm_gdos && (cur_font_format > 1)) {
		vst_kern (handle, tmode, pmode, tracks, pairs);
	} else {
		*tracks = *pairs = 0;
	}
}


/*----------------------------------------------------------------------------*/
void
hw_vst_load_fonts (WORD handle, WORD sel)
{
	if (has_fsm_gdos) {
		vst_load_fonts (handle, sel);
	}
}


/*----------------------------------------------------------------------------*/
void
hw_vst_unload_fonts (WORD handle, WORD sel)
{
	if (has_fsm_gdos) {
		vst_unload_fonts (handle, sel);
	}
}


/*----------------------------------------------------------------------------*/
/* vq_scrninfo() arrived with NVDI and the plain ROM VDI answers it with
 * garbage.  Without a GDOS (or given a senseless reply) describe the screen
 * ourselves: an ST/STE screen is always interleaved bitplanes, one plane per
 * bit of depth, with the VDI colour index -> hardware pixel value mapping in
 * out[16..271].  out must hold the full 273 WORD reply.
 */
void
hw_vq_scrninfo (WORD handle, WORD * out)
{
	/* the TOS VDI puts white first and black second (or last), so not the
	 * identity map */
	static const WORD st_pixel_16[16] =
		{ 0, 15, 1, 2, 4, 6, 3, 5, 7, 8, 9, 10, 12, 14, 11, 13 };
	static const WORD st_pixel_4[4] = { 0, 3, 1, 2 };
	static const WORD tt_pixel_256[16] =
		{ 0, 255, 1, 2, 4, 6, 3, 5, 7, 8, 9, 10, 12, 14, 11, 13 };
	WORD i;

	if (has_fsm_gdos || has_eddi_cookie) {
		vq_scrninfo (handle, out);
		if (out[0] >= 0 && out[0] <= 2 && out[2] >= 1 && out[2] <= 32) {
			return;
		}
		/* implausible reply - fall through and answer it ourselves */
	}

	memset (out, 0, 273 * sizeof *out);
	out[2] = planes;
	if (planes > 8) {
		out[0] = 2; /* packed pixels - no colour index mapping to report */
		return;
	}
	/* out[0] = 0 from the memset: interleaved bitplanes */
	for (i = 0; i < 256; i++) {
		out[16 + i] = i;
	}
	if (planes == 4) {
		memcpy (out + 16, st_pixel_16, sizeof st_pixel_16);
	} else if (planes == 2) {
		memcpy (out + 16, st_pixel_4, sizeof st_pixel_4);
	} else if (planes == 8) {
		memcpy (out + 16, tt_pixel_256, sizeof tt_pixel_256);
		out[16 + 255] = 15;
	}
}
