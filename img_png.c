#ifdef LIBPNG

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
	decPng_start
};
#undef  DECODER_CHAIN
#define DECODER_CHAIN &_decoder_png


BOOL   first_pass;
ULONG  rowbytes;	
CHAR * png_image, * png_image_ptr;


/*----------------------------------------------------------------------------*/
static BOOL
decPng_start (const char * name, IMGINFO info)
{
	png_structp png_ptr;
	png_infop   info_ptr = NULL;
	char header[8];
	FILE * file = fopen (name, "rb");

	png_image = NULL;
	
	if (!file) {
	/*	puts ("decPng_start(): file not found.");*/
		return TRUE; /* avoid further tries of decoding */
	
	} else if (fread (header, sizeof(header), 1, file) != 1 ||
	           png_sig_cmp (header, 0, sizeof(header))) {
		fclose (file);
	/*	puts ("decPng_start(): wrong file type.");*/
		return FALSE;
	}
	
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
	
	if (info_ptr->bit_depth == 16) {
		png_set_strip_16 (png_ptr);
	} else if (info_ptr->bit_depth < 8) {
		png_set_packing (png_ptr);
		if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY) {
			png_set_gray_1_2_4_to_8 (png_ptr);
		}
	}
	if (info_ptr->color_type & PNG_COLOR_MASK_ALPHA) {
		png_set_strip_alpha (png_ptr);
	}
	info->_priv_data = png_ptr;
	info->_priv_more = info_ptr;
	info->_priv_file = file;
	info->read       = decPng_read;
	info->quit       = decPng_quit;
	info->ImgWidth   = info_ptr->width;
	info->ImgHeight  = info_ptr->height;
	info->NumComps   = (info_ptr->channels >= 3 ? 3 : 1);
	info->BitDepth   = info_ptr->bit_depth;
	info->NumColors  = info_ptr->num_palette;
	if ((info->Palette = (char*)info_ptr->palette) != NULL) {
		png_color * rgb = NULL;
		info->PalRpos = (unsigned)&rgb->red;
		info->PalGpos = (unsigned)&rgb->green;
		info->PalBpos = (unsigned)&rgb->blue;
		info->PalStep = (unsigned)&rgb[1];
	}
	info->Transp     = -1;
	info->Interlace  = 0;
	
	if (info_ptr->interlace_type == PNG_INTERLACE_ADAM7) {
		info->read = decPng_readi;
		first_pass = TRUE;
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
	png_read_row (png_ptr, buffer, NULL);
	return TRUE;
}
	
/*----------------------------------------------------------------------------*/
static BOOL
decPng_readi (IMGINFO info, char * buffer)
{
	png_structp png_ptr  = info->_priv_data;
	png_infop   info_ptr = info->_priv_more;

	if (setjmp (png_jmpbuf (png_ptr))) {
		return FALSE;
	}
	
	/* We make the first pass here and not before to avoid memory fragmentation */
	if (first_pass == TRUE) {
		int pass, number_passes = png_set_interlace_handling (png_ptr);

		png_read_update_info (png_ptr, info_ptr);

		rowbytes  = png_get_rowbytes (png_ptr, info_ptr);
		png_image = malloc (rowbytes * info->ImgHeight);
        
		if ((png_image_ptr = png_image) == NULL)
			return FALSE;

		for (pass = 0; pass < number_passes - 1; pass++) {
			int y;
			for (y = 0; y < info->ImgHeight; y++) {
				png_bytep row = png_image + y * rowbytes;
				png_read_rows (png_ptr, &row, NULL, 1);
			}
		}
		first_pass = FALSE;
	}
	
	(void)buffer;
	info->RowBuf = png_image_ptr;

	png_read_rows (png_ptr, &png_image_ptr, NULL, 1);
	png_image_ptr += rowbytes;		

	return TRUE;
}

/*----------------------------------------------------------------------------*/
static void
decPng_quit  (IMGINFO info)
{
	png_structp png_ptr  = info->_priv_data;
	png_infop   info_ptr = info->_priv_more;
	if (png_ptr) {
		if (png_image) {
			free (png_image);
		}
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

#endif /* LIBPNG */
