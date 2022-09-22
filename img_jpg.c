#define XMD_H /* avoid redefining INT16 and INT32, already done in gemlib */
#include <setjmp.h>
#include <jpeglib.h>
#undef XMD_H

static BOOL decJpg_start (const char * file, IMGINFO info);
static BOOL decJpg_read  (IMGINFO, CHAR * buffer);
static void decJpg_quit  (IMGINFO);

static DECODER _decoder_jpg = {
	DECODER_CHAIN,
	{ MIME_IMG_JPEG, 0 },
	decJpg_start
};
#undef  DECODER_CHAIN
#define DECODER_CHAIN &_decoder_jpg

typedef struct jpeg_decompress_struct * JPEG_DEC;
typedef struct jpeg_error_mgr         * JPEG_ERR;


/*----------------------------------------------------------------------------*/
static void
_jpeg_shutup (j_common_ptr cinfo, int msg_level)
{
	(void)cinfo;
	(void)msg_level;
/*	puts("blah...");*/
}

/*----------------------------------------------------------------------------*/
static void
_jpeg_errjmp (j_common_ptr cinfo)
{
	jmp_buf * escape = cinfo->client_data;
	if (!escape) {
		hwUi_fatal ("image::jpeg_error", "got lost!");
	} else {
/*		puts("ouch...");*/
		longjmp (*escape, TRUE);
	}
}

/*----------------------------------------------------------------------------*/
static BOOL
decJpg_start (const char * name, IMGINFO info)
{
	jmp_buf escape;
	
	FILE   * file = fopen (name, "rb");
	volatile JPEG_DEC jpeg = NULL;
	volatile JPEG_ERR jerr = NULL;
	int header = 0;
	
	if (!file) {
/*		puts ("decJpg_start(): file not found.");*/
		return TRUE; /* avoid further tries of decoding */
	
	} else if ((jpeg = malloc (sizeof(struct jpeg_decompress_struct))) == NULL ||
	           (jerr = malloc (sizeof(struct jpeg_error_mgr)))         == NULL) {
		puts ("decJpg_start(): low memory.");
		if (jerr) free   (jerr);
		if (jpeg) free   (jpeg);
		if (file) fclose (file);
		return FALSE;
	}
	
	if (setjmp (escape)) {
		if (header >= 2) {
			jpeg_abort_decompress (jpeg);
		}
		jpeg_destroy_decompress (jpeg);
		free   (jerr);
		free   (jpeg);
		fclose (file);
		return (header != 0);
	}

	jpeg->err               = jpeg_std_error (jerr);
	jpeg->err->emit_message = _jpeg_shutup;
	jpeg->err->error_exit   = _jpeg_errjmp;
	jpeg->client_data       = &escape;
	
	jpeg_create_decompress (jpeg);
	jpeg_stdio_src         (jpeg, file);
	jpeg_read_header       (jpeg, TRUE);
	
	jpeg->dct_method          = JDCT_IFAST;
#ifdef __M68881__
	jpeg->do_fancy_upsampling = TRUE;
#else
	jpeg->do_fancy_upsampling = FALSE;
#endif
	
	switch (jpeg->out_color_space) {
		case JCS_RGB:
			info->NumComps = 3;
			break;
		case JCS_GRAYSCALE:
			info->NumComps = 1;
			break;
		default: ;
			jpeg->out_color_space = JCS_RGB;
			info->NumComps = jpeg->out_color_components = 3;
			break;
	}
	header = 1;
	jpeg_start_decompress  (jpeg);
	header = 2;
	
	info->_priv_data = jpeg;
/*	info->_priv_more = NULL; */
	info->_priv_file = file;
	info->read       = decJpg_read;
	info->quit       = decJpg_quit;
	info->ImgWidth   = jpeg->image_width;
	info->ImgHeight  = jpeg->image_height;
	info->BitDepth   = 8;
/*	info->NumColors  = 0;    0-values needn't to be set */
/*	info->Palette    = NULL; */
/*	info->PalRpos    = 0; */
/*	info->PalGpos    = 0; */
/*	info->PalBpos    = 0; */
/*	info->PalStep    = 0; */
	info->Transp     = -1;
/*	info->Interlace  = 0; */
	
	jpeg->client_data = NULL; /* unset jump buffer */
	
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static BOOL
decJpg_read (IMGINFO info, CHAR * buffer)
{
	jpeg_read_scanlines (info->_priv_data, (JSAMPROW*)&buffer, 1);
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static void
decJpg_quit (IMGINFO info)
{
	JPEG_DEC jpeg = info->_priv_data;
	if (jpeg) {
		jmp_buf escape;
		jpeg->client_data = &escape;
		if (!setjmp (escape)) {
			jpeg_finish_decompress (jpeg);
		}
		jpeg->client_data = NULL;
		jpeg_destroy_decompress (jpeg);
		free   (jpeg->err);
		free   (jpeg);
		fclose (info->_priv_file);
		info->_priv_file = NULL;
		info->_priv_data = NULL;
	}
}
