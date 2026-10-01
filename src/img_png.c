#include <setjmp.h>
#if defined(__PUREC__) && !defined(STDC)
#	define STDC
#	define STDC_for_zlib
#endif
#include <png.h>
#if defined(STDC_for_zlib)
#	undef STDC
#	undef STDC_for_zlib
#endif

static BOOL decPng_start (const char * file, IMGINFO info);
static BOOL decPng_read  (IMGINFO, CHAR * buffer);
static BOOL decPng_readi (IMGINFO, CHAR * buffer);
static void decPng_quit  (IMGINFO);

static DECODER _decoder_png = {
	DECODER_CHAIN,
	{ MIME_IMG_PNG, 0 },
	decPng_start
};
#undef  DECODER_CHAIN
#define DECODER_CHAIN &_decoder_png


/*----------------------------------------------------------------------------*/
static BOOL
decPng_start (const char * name, IMGINFO info)
{
	png_structp png_ptr;
	png_infop   info_ptr = NULL;
	char header[8];
	FILE * file = fopen (name, "rb");
	png_color *palette;
	int num_colors;
	int color_type;
	BOOL alpha  = FALSE;
	WORD transp = -1;
	unsigned char * pal_alpha = NULL;
	WORD            num_alpha = 0;

	if (!file) {
	/*	puts ("decPng_start(): file not found.");*/
		return TRUE; /* avoid further tries of decoding */
	}
#if (PNG_LIBPNG_VER_MAJOR >= 1) && (PNG_LIBPNG_VER_MINOR >= 4)
	else if (fread (header, sizeof(header), 1, file) != 1 ||
	           png_sig_cmp ((png_bytep)header, 0, sizeof(header))) {
		fclose (file);
	/*	puts ("decPng_start(): wrong file type.");*/
		return FALSE;
	}
#else
	else if (fread (header, sizeof(header), 1, file) != 1 ||
	           png_sig_cmp (header, 0, sizeof(header))) {
		fclose (file);
	/*	puts ("decPng_start(): wrong file type.");*/
		return FALSE;
	}
#endif	
	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, 0, 0);
	if (!png_ptr || (info_ptr = png_create_info_struct (png_ptr)) == NULL) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		fclose (file);
		errprintf ("decPng_start(): low memory.\n");
		return TRUE;
	}
	if (setjmp (png_jmpbuf (png_ptr))) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		fclose (file);
		return TRUE;
	}
	png_init_io       (png_ptr, file);
	png_set_sig_bytes (png_ptr, (int)sizeof(header));
	png_read_info (png_ptr, info_ptr);
	
	if (png_get_bit_depth(png_ptr, info_ptr) == 16) {
		png_set_strip_16 (png_ptr);
	} else if (png_get_bit_depth(png_ptr, info_ptr) < 8) {
		png_set_packing (png_ptr);
		if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY) {
#if (PNG_LIBPNG_VER_MAJOR >= 1) && (PNG_LIBPNG_VER_MINOR >= 4)
			png_set_expand_gray_1_2_4_to_8(png_ptr);	
#else
			png_set_gray_1_2_4_to_8 (png_ptr);
#endif
		}
	}
	/* A palette whose entries are each fully clear or fully solid stays a
	 * palette, its first clear entry taking a GIF's kind of transparency and
	 * setup() handing any others the same pixel.  FAST_IMAGES counts every
	 * entry under half as clear and the rest as solid, so no palette leaves.
	 * Anything else that is not opaque becomes RGBA, for the rows to be
	 * blended over the background as they are read. */
	color_type = png_get_color_type (png_ptr, info_ptr);
	if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_bytep trns   = NULL;
		int       n_trns = 0;
		png_get_tRNS (png_ptr, info_ptr, &trns, &n_trns, NULL);
		if (color_type == PNG_COLOR_TYPE_PALETTE) {
			int i, clear = 0;
			for (i = 0; i < n_trns && !alpha; i++) {
				if (trns[i] < 128 && (cfg_FastImages || !trns[i])) {
					if (!clear++) transp = i;
				} else if (trns[i] != 255 && !cfg_FastImages) {
					alpha = TRUE;
				}
			}
			if (alpha) {
				transp = -1;
				png_set_palette_to_rgb (png_ptr);
			} else if (clear > 1) {
				pal_alpha = trns;
				num_alpha = n_trns;
			}
		} else {
			alpha = TRUE;
		}
		if (alpha) {
			png_set_tRNS_to_alpha (png_ptr);
		}
	} else if (color_type & PNG_COLOR_MASK_ALPHA) {
		alpha = TRUE;
	}
	if (alpha && !(color_type & PNG_COLOR_MASK_COLOR)) {
		png_set_gray_to_rgb (png_ptr);
	}
	info->_priv_data = png_ptr;
	info->_priv_more = info_ptr;
	info->_priv_file = file;
	info->read       = decPng_read;
	info->quit       = decPng_quit;
	info->ImgWidth   = png_get_image_width(png_ptr, info_ptr);
	info->ImgHeight  = png_get_image_height(png_ptr, info_ptr);
	info->NumComps   = (png_get_channels(png_ptr, info_ptr) >= 3 ? 3 : 1);
	info->BitDepth   = png_get_bit_depth(png_ptr, info_ptr);
	if (png_get_PLTE(png_ptr, info_ptr, &palette, &num_colors) & PNG_INFO_PLTE) {
		info->Palette = (unsigned char *)palette;
		info->NumColors = num_colors;
#if 1 /* bug-endian */
		info->PalRpos = (int)(offsetof(png_color, red) + sizeof(palette->red) - 1);
		info->PalGpos = (int)(offsetof(png_color, green) + sizeof(palette->green) - 1);
		info->PalBpos = (int)(offsetof(png_color, blue) + sizeof(palette->blue) - 1);
#else
		info->PalRpos = (int)offsetof(png_color, red);
		info->PalGpos = (int)offsetof(png_color, green);
		info->PalBpos = (int)offsetof(png_color, blue);
#endif
		info->PalStep = (int)sizeof(png_color);
	} else
	{
		info->Palette = NULL;
		info->NumColors = 0;
	}
	if (alpha) {
		info->NumComps  = 3;
		info->BitDepth  = 8;
		info->Palette   = NULL;
		info->NumColors = 0;
	}
	info->Alpha      = alpha;
	info->Transp     = transp;
	info->PalAlpha   = pal_alpha;
	info->NumAlpha   = num_alpha;
	info->Interlace  = 0;
	
	if (png_get_interlace_type(png_ptr, info_ptr) == PNG_INTERLACE_ADAM7) {
		png_set_interlace_handling (png_ptr);
		png_read_update_info (png_ptr, info_ptr);
		info->RowBytes = png_get_rowbytes (png_ptr, info_ptr);
		info->read     = decPng_readi;
	} else {
		png_read_update_info (png_ptr, info_ptr);
	}
	return TRUE;
}
	
/*----------------------------------------------------------------------------*/
/* Composite a row of RGBA over info->AlphaBg, in place, leaving the RGB the
 * rasterizers expect.  Most pixels are fully opaque or fully clear, so only
 * the edges pay for the multiplies.
*/
static void
blend_row (IMGINFO info, CHAR * row)
{
	const UWORD     r_bg = (UWORD)(info->AlphaBg >>16) & 0xFF;
	const UWORD     g_bg = (UWORD)(info->AlphaBg >> 8) & 0xFF;
	const UWORD     b_bg = (UWORD)(info->AlphaBg     ) & 0xFF;
	/* FAST_IMAGES cuts at half way, so nothing is left to blend */
	const UWORD     solid = (cfg_FastImages ? 128 : 255);
	const UWORD     clear = (cfg_FastImages ? 127 :   0);
	unsigned char * src  = (unsigned char *)row;
	unsigned char * dst  = src;
	UWORD           n    = info->ImgWidth;

	while (n--) {
		UWORD a = src[3];
		if (a >= solid) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		} else if (a <= clear) {
			dst[0] = r_bg;
			dst[1] = g_bg;
			dst[2] = b_bg;
		} else {
			/* (x + (x >>8)) >>8 divides by 255, rounded, once x carries +128 */
			UWORD b = 255 - a, x;
			x = (UWORD)(src[0] * a + r_bg * b + 128); dst[0] = (x + (x >>8)) >>8;
			x = (UWORD)(src[1] * a + g_bg * b + 128); dst[1] = (x + (x >>8)) >>8;
			x = (UWORD)(src[2] * a + b_bg * b + 128); dst[2] = (x + (x >>8)) >>8;
		}
		src += 4;
		dst += 3;
	}
}

/*----------------------------------------------------------------------------*/
static BOOL
decPng_read (IMGINFO info, CHAR * buffer)
{
	png_structp png_ptr = info->_priv_data;
	if (setjmp (png_jmpbuf (png_ptr))) {
		return FALSE;
	}
#if (PNG_LIBPNG_VER_MAJOR >= 1) && (PNG_LIBPNG_VER_MINOR >= 4)
	png_read_row (png_ptr, (png_bytep)buffer, NULL);
#else
	png_read_row (png_ptr, buffer, NULL);
#endif
	if (info->Alpha) {
		blend_row (info, buffer);
	}
	return TRUE;
}
	
/*----------------------------------------------------------------------------*/
static BOOL
decPng_readi (IMGINFO info, CHAR * buffer)
{
	png_structp png_ptr  = info->_priv_data;
	
	(void)buffer;

	if (setjmp (png_jmpbuf (png_ptr))) {
		return FALSE;
	}
	
	if (png_get_current_row_number(png_ptr) == 0) {
		ULONG       rowbytes      = info->RowBytes;	
		png_bytep * rowptrs       = info->RowMem;
		png_bytep   row           = info->RowBuf;
		int         number_passes = png_set_interlace_handling (png_ptr), pass, y;
		
		/* setup row pointer array */
		for (y = 0; y < info->ImgHeight; rowptrs[y++] = row, row += rowbytes);
		
		for (pass = 0; pass < number_passes -1; pass++) {
			png_read_rows (png_ptr, rowptrs, NULL, info->ImgHeight);
		}
	
	} else {
		info->RowBuf += info->RowBytes;
	}
	png_read_rows (png_ptr, (png_bytep*)&info->RowBuf, NULL, 1);
	if (info->Alpha) {
		blend_row (info, info->RowBuf);
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
static void
decPng_quit  (IMGINFO info)
{
	png_structp png_ptr  = info->_priv_data;
	png_infop   info_ptr = info->_priv_more;
	if (png_ptr) {
		if (!setjmp (png_jmpbuf (png_ptr))) {
			png_read_end         (png_ptr,  info_ptr);
		}
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		fclose (info->_priv_file);
		info->_priv_file = NULL;
		info->_priv_more = NULL;
		info->_priv_data = NULL;
	}
}
