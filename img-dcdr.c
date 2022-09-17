#include <stddef.h>
#include <stdlib.h>

#include "global.h"
#include "mime.h"
#include "image_P.h"

typedef struct s_decoder {
	struct s_decoder * Next;
	MIMETYPE           Mime[4];
	BOOL (*start)(const char * file, IMGINFO);
} DECODER;
#define DECODER_CHAIN NULL

#include "img_gif.c"
#include "img_jpg.c"
#include "img_png.c"
#include "img_xmp.c"
#include "img_ico.c"

/*----------------------------------------------------------------------------*/
IMGINFO
get_decoder (const char * file)
{
	static DECODER * decoder_chain = DECODER_CHAIN;
	
	IMGINFO info = (decoder_chain ? malloc (sizeof (struct s_img_info)) : NULL);
	
	if (info) {
		DECODER * decoder = NULL;
		DECODER * found   = NULL;
		MIMETYPE  mime    = mime_byExtension (file, NULL, NULL);
		memset (info, 0, offsetof (struct s_img_info, Pixel));
		if (MIME_Major(mime) && MIME_Minor(mime)) {
			found = decoder_chain;
			while (found) {
				MIMETYPE * mptr = found->Mime;
				while (*mptr && *mptr != mime) {
					mptr++;
				}
				if (*mptr) {
					if ((*found->start)(file, info)) {
						decoder = found;
					}
					break;
				}
				found = found->Next;
			}
		}
		if (!decoder) {
			decoder = decoder_chain;
			while (decoder) {
				if (decoder != found && (*decoder->start)(file, info)) {
					break;
				}
				decoder = decoder->Next;
			}
		}
		if (!info->_priv_data) {
			free (info);
			info = NULL;
		}
	}
	return info;
}
