#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "hw-types.h"
#include "mime.h"


struct ext_2_mime {
	const char   * Ext;
	const MIMETYPE Type;
	const char   * Appl;
};
static const struct ext_2_mime mime_list[] = {
/*
	{ "avr",  MIME_AUDIO,      "GEMJing"  },
	{ "dvs",  MIME_AUDIO,      "GEMJing"  },
	{ "hsn",  MIME_AUDIO,      "GEMJing"  },
	{ "hyp",  MIME_APPL,       "ST-Guide" },
	{ "img",  MIME_IMG_X_XIMG, ""         },
	{ "mpg",  MIME_VID_MPEG,   "ANIPLAY"  },
	{ "pdf",  MIME_APP_PDF,    "MyPdf"    },
*/
	{ "au",    MIME_AUD_BASIC,  "GEMJing"  },
	{ "css",   MIME_TXT_CSS,    NULL       },
	{ "gif",   MIME_IMG_GIF,    NULL       },
	{ "htm",   MIME_TXT_HTML,   NULL       },
	{ "html",  MIME_TXT_HTML,   NULL       },
	{ "jpeg",  MIME_IMG_JPEG,   ""         },
	{ "jpg",   MIME_IMG_JPEG,   ""         },
	{ "png",   MIME_IMG_PNG,    ""         },
	{ "shtml", MIME_TXT_HTML,   NULL       },
	{ "snd",   MIME_AUD_BASIC,  "GEMJing"  },
	{ "txt",   MIME_TXT_PLAIN,  NULL       },
	{ "wav",   MIME_AUD_X_WAV,  "GEMJing"  },
	{ "xbm",   MIME_IMG_X_XBM,  ""         },
	{ "xpm",   MIME_IMG_X_XPM,  ""         }
};

struct mime_2_ext {
	const MIMETYPE Type;
	const char     Ext[4];
};
static const struct mime_2_ext ext_list[] = {
	{ MIME_TXT_CSS,   "css" },
	{ MIME_TXT_HTML,  "htm" },
	{ MIME_TXT_PLAIN, "txt" },
	{ MIME_IMG_JPEG,  "jpg" },
	{ MIME_IMG_GIF,   "gif" },
	{ MIME_IMG_PNG,   "png" },
	{ MIME_AUD_BASIC, "au"  },
	{ MIME_AUD_X_WAV, "wav" }
};


/*============================================================================*/
MIMETYPE
mime_byExtension (const char * file, const char ** stored, char * ptr)
{
	const char * end =                     strchr (file, '?');
	const char * ext = (end ? end : (end = strchr (file, '\0')));
	MIMETYPE    type = MIME_Unknown;
	
	while (ext > file && *(--ext) != '.');
	if (*ext == '.' && ++ext < end) {
		size_t len = end - ext;
		short  i   = 0;
		do if (strnicmp (ext, mime_list[i].Ext, len) == 0
		       && strlen (mime_list[i].Ext) == len) {
			type = mime_list[i].Type;
			if (stored) {
				*stored = mime_list[i].Appl;
			}
			if (ptr) {
				strcpy (ptr, mime_toExtension (type));
			}
			break;
		} while (++i < (short)numberof(mime_list));
		
		if (!type && ptr) {
			ptr[0] = toupper (ext[0]);
			ptr[1] = toupper (ext[1]);
			ptr[2] = toupper (ext[2]);
			ptr[len < 3 ? len : 3] = '\0';
		}
	
	} else if (ptr) {
		*ptr = '\0';
	}
	
	return type;
}


/*============================================================================*/
const char *
mime_toExtension (MIMETYPE type)
{
	const char * ext = "";
	short        i   = 0;
	do if (type == ext_list[i].Type) {
		ext = ext_list[i].Ext;
		break;
	} while (++i < (short)numberof(ext_list));
	
	return ext;
}


/*============================================================================*/
MIMETYPE
mime_byString (const char * str, const char ** tail)
{
	MIMETYPE type = 0;
	
	static struct mime_sub {
		const char * str;
		MIMETYPE     type;
	}
	txt_types[] = {
		{ "plain", MIME_TXT_PLAIN },
		{ "html",  MIME_TXT_HTML  },
		{ "css",   MIME_TXT_CSS   },
		{ NULL, }
	},
	img_types[] = {
		{ "jpeg", MIME_IMG_JPEG },
		{ "gif",  MIME_IMG_GIF  },
		{ "png",  MIME_IMG_PNG  },
		{ NULL, }
	},
	aud_types[] = {
		{ "basic", MIME_AUD_BASIC },
		{ "x-wav", MIME_AUD_X_WAV },
		{ NULL, }
	},
	vid_types[] = {
		{ "mpeg", MIME_VID_MPEG },
		{ NULL, }
	},
	app_types[] = {
		{ "octet-stream",  MIME_APP_OCTET },
		{ "pdf",           MIME_APP_PDF   },
		{ NULL, }
	};
	static struct mime {
		const char      * str;
		MIMETYPE          type;
		struct mime_sub * sub;
	} mime_types[] = {
		{ "text",        MIME_TEXT,  txt_types },
		{ "image",       MIME_IMAGE, img_types },
		{ "audio",       MIME_AUDIO, aud_types },
		{ "video",       MIME_VIDEO, vid_types },
		{ "application", MIME_APPL,  app_types },
		{ NULL, }
	};
	
	struct mime * mime = mime_types;
	size_t        len  = 0;
	while (isalpha(str[len])) len++;
	if (len) do {
		if (strnicmp (str, mime->str, len) == 0 && !mime->str[len]) {
			str += len;
			type = mime->type;
			if (*str == '/' && isalpha(*(++str))) {
				struct mime_sub * sub = mime->sub;
				len = 0;
				while (isalpha(str[++len]) || str[len] == '-');
				if (len) do {
					if (strnicmp (str, sub->str, len) == 0 && !sub->str[len]) {
						str += len;
						type = sub->type;
						break;
					}
				} while ((++sub)->str);
			}
			break;
		}
	} while ((++mime)->str);
	
	if (tail) *tail = str;
	
	return type;
}




