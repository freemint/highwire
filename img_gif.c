#include <gif_lib.h>

#undef TRUE

#undef FALSE



static BOOL decGif_start (const char * file, IMGINFO info);

static BOOL decGif_read  (IMGINFO, char * buffer);

static void decGif_quit  (IMGINFO);



static DECODER _decoder_gif = {

	DECODER_CHAIN,

	{ MIME_IMG_GIF, 0 },

	decGif_start

};

#undef  DECODER_CHAIN

#define DECODER_CHAIN &_decoder_gif



#if defined(GIFLIB_MAJOR) 
static void _PrintGifError(GifFileType *gif)
{
	const char *err = GifErrorString(gif->Error);
	if (err != NULL)
		fprintf(stderr, "\nGIF-LIB error: %s.\n", err);
	else
		fprintf(stderr, "\nGIF-LIB undefined error %d.\n", gif->Error);
}
#define PrintGifError() _PrintGifError(gif)
#endif





/*----------------------------------------------------------------------------*/

static BOOL

decGif_start (const char * file, IMGINFO info)

{

	short interlace = 0;

	short transpar  = -1;

	short img_w     = 0;

	short img_h     = 0;

	

	ColorMapObject * map = NULL;

#if defined(GIFLIB_MAJOR)

	int gif_error;

	GifFileType    * gif = DGifOpenFileName (file, &gif_error);

#else

	GifFileType    * gif = DGifOpenFileName (file);

#endif

	if (!gif) {

		return FALSE;

	

	} else {

		GifRecordType rec;

		do {

			if (DGifGetRecordType (gif, &rec) == GIF_ERROR) {

				fprintf (stderr, "DGifGetRecordType() ");

				PrintGifError();

				break;

			

			} else if (rec == IMAGE_DESC_RECORD_TYPE) {

				if (DGifGetImageDesc(gif) == GIF_ERROR) {

					fprintf (stderr, "DGifGetImageDesc() ");

					PrintGifError();

					break;

				

				} else {

					GifImageDesc * dsc = &gif->Image;

					map = (dsc->ColorMap ? dsc->ColorMap : gif->SColorMap);

					img_w = dsc->Width;

					img_h = dsc->Height;

					if (dsc->Interlace) {

						interlace = 8;

					}

				}

				break;

			

			} else if (rec == EXTENSION_RECORD_TYPE) {

				int           code;

				GifByteType * block;

				if (DGifGetExtension (gif, &code, &block) == GIF_ERROR) {

					fprintf (stderr, "DGifGetExtension() ");

					PrintGifError();

					break;

				

				} else while (block != NULL) {

					if (code == 0xF9 && (block[1] & 1)) {

						transpar = (short)block[4];

					}

					if (DGifGetExtensionNext (gif, &block) == GIF_ERROR) {

						fprintf (stderr, "DGifGetExtensionNext() ");

						PrintGifError();

						break;

					}

				}

			

			} else {

				fprintf (stderr, "other: %i \n", rec);

				break;

			}

		} while (rec != TERMINATE_RECORD_TYPE);

	}

	

	if (!map || img_w <= 0 || img_w >= 4096 || img_h <= 0 || img_h >= 4096) {

#if defined(GIFLIB_MAJOR)
		DGifCloseFile (gif, NULL);
#else
		DGifCloseFile (gif);
#endif

		return TRUE;

	}

	

	info->_priv_data = gif;

/*	info->_priv_more = NULL; */

/*	info->_priv_file = NULL; */

	info->read       = decGif_read;

	info->quit       = decGif_quit;

	info->ImgWidth   = img_w;

	info->ImgHeight  = img_h;

	info->NumComps   = 1;

	info->BitDepth   = map->BitsPerPixel;

	info->NumColors  = map->ColorCount;

	if ((info->Palette = (char*)map->Colors) != NULL) {

		GifColorType * rgb = NULL;

		info->PalRpos = (unsigned)&rgb->Red;

		info->PalGpos = (unsigned)&rgb->Green;

		info->PalBpos = (unsigned)&rgb->Blue;

		info->PalStep = (unsigned)&rgb[1];

	}

	info->Transp     = transpar;

	info->Interlace  = interlace;

	

	return TRUE;

}



/*----------------------------------------------------------------------------*/

static BOOL

decGif_read (IMGINFO info, char * buffer)

{
	return (DGifGetLine (info->_priv_data, (void *)buffer, info->ImgWidth) == GIF_OK);
}



/*----------------------------------------------------------------------------*/

static void

decGif_quit (IMGINFO info)

{

	if (info->_priv_data) {

#if defined(GIFLIB_MAJOR)
		DGifCloseFile (info->_priv_data, NULL);
#else
		DGifCloseFile (info->_priv_data);
#endif

		info->_priv_data = NULL;

	}

}

