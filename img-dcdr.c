#include <stddef.h>
#include <stdlib.h>

#include "global.h"
#include "image_P.h"

typedef struct s_decoder {
	struct s_decoder * Next;
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
	
	DECODER * decoder = decoder_chain;
	IMGINFO   info    = (decoder ? malloc (sizeof (struct s_img_info)) : NULL);
	
	if (info) {
		memset (info, 0, offsetof (struct s_img_info, Pixel));
		while (decoder && !(*decoder->start)(file, info)) {
			decoder = decoder->Next;
		}
		if (!info->_priv_data) {
			free (info);
			info = NULL;
		}
	}
	return info;
}
