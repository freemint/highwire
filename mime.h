#ifndef __MIME_H__
#define __MIME_H__


typedef enum {   /* Basic MIME types from RFC2045/2046 by the
                  * "Network Working Group",
                  * partly registered with IANA
                 */
	MIME_Unknown = 0,

	MIME_TEXT = 0x1000,
		MIME_TXT_PLAIN,   /* text/plain "*"                         */
		MIME_TXT_HTML,    /* text/html  "*.htm,*.html,*.php,*.php?" */
		MIME_TXT_CSS,     /* text/css   "*.css"                     */

	MIME_IMAGE = 0x2000,
		MIME_IMG_JPEG,    /* image/jpeg      "*.jpg,*.jpeg" */
		MIME_IMG_GIF,     /* image/gif       "*.gif"        */
		MIME_IMG_PNG,     /* image/png       "*.png"        */
		MIME_IMG_X_XBM,   /* image/x-xbitmap "*.xbm"        */
		MIME_IMG_X_XPM,   /* image/x-xpixmap "*.xbm"        */
		MIME_IMG_X_XIMG,  /* image/x-ximg    "*.img"        */

	MIME_AUDIO = 0x3000,
		MIME_AUD_BASIC,   /* audio/basic  "*.au,*.snd" */
		MIME_AUD_X_WAV,   /* audio/x-wav  "*.wav"      */

	MIME_VIDEO = 0x4000,
		MIME_VID_MPEG,    /* video/mpeg   "*.mpg" */

	MIME_APPL = 0x5000,
		MIME_APP_OCTET,   /* application/octet-stream         */
		MIME_APP_PDF      /* application/pdf          "*.pdf" */
} MIMETYPE;

#define MIME_Major(t)   (t & 0xF000)
#define MIME_Minor(t)   (t & 0x0FFF)


MIMETYPE     mime_byExtension (const char *, const char ** stored, char * ext);
const char * mime_toExtension (MIMETYPE);

MIMETYPE mime_byString (const char *, const char ** tail);


#endif /*__MIME_H__*/
