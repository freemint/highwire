#ifdef LIBPNG

#include <setjmp.h>
#include <png.h>

static BOOL decPng_start (const char * file, IMGINFO info);
static BOOL decPng_read  (IMGINFO, char * buffer);
static void decPng_quit  (IMGINFO);

static DECODER _decoder_png = {
	DECODER_CHAIN,
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
	
	if (!file) {
		puts ("decPng_start(): file not found.");
		return FALSE;
	
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
	png_init_io       (png_ptr, file);
	png_set_sig_bytes (png_ptr, sizeof(header));
	png_read_info     (png_ptr, info_ptr);
	
	if (info_ptr->bit_depth == 16) {
		png_set_strip_16 (png_ptr);
	} else if (info_ptr->bit_depth < 8) {
		png_set_packing (png_ptr);
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
	info->NumComps   = info_ptr->channels;
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
static void
decPng_quit  (IMGINFO info)
{
	png_structp png_ptr  = info->_priv_data;
	png_infop   info_ptr = info->_priv_more;
	if (png_ptr) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		fclose (info->_priv_file);
		info->_priv_file = NULL;
		info->_priv_more = NULL;
		info->_priv_data = NULL;
	}
}

#endif /* LIBPNG */
