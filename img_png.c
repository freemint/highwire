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

static BOOL decPng_read  (IMGINFO, char * buffer);

static BOOL decPng_readi (IMGINFO, char * buffer);

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

		puts ("decPng_start(): low memory.");

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

	if (png_get_color_type(png_ptr, info_ptr) & PNG_COLOR_MASK_ALPHA) {

		png_set_strip_alpha (png_ptr);

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
		info->Palette = (char *)palette;
		info->NumColors = num_colors;
		info->PalRpos = (int)offsetof(png_color, red);

		info->PalGpos = (int)offsetof(png_color, green);

		info->PalBpos = (int)offsetof(png_color, blue);

		info->PalStep = (int)sizeof(png_color);
	} else
	{
		info->Palette = NULL;
		info->NumColors = 0;
	}

	info->Transp     = -1;

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

static BOOL

decPng_read (IMGINFO info, char * buffer)

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

	return TRUE;

}

	

/*----------------------------------------------------------------------------*/

static BOOL

decPng_readi (IMGINFO info, char * buffer)

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

