/* @(#)highwire/encoding.c
 */
#include <stdio.h> /* printf */

#include "defs.h"
#include "token.h"
#include "scanner.h"


/*------------------------------------------------------------------------------
 * Perform a binary tree search of 'uni' in 'tab'.  The Format of 'tab' is
 * either {Unicode, 0..2 replacement characters, 0} or {Unicode, 0xFFFF, pointer
 * to a 0-delimited WCHAR string of replacement characters}.
*/
struct s_uni_trans {
	WCHAR Uni;
	WCHAR Trans;
	LONG  Extra;
};
static const WCHAR *
_bin_search (WCHAR uni, const struct s_uni_trans * tab, WORD end)
{
	WORD          beg = 0;
	const WCHAR * arr = &tab[end].Trans;
	do {
		WORD i = (end + beg) /2;
		if (uni > tab[i].Uni) {
			beg = i +1;
		} else if (uni < tab[i].Uni) {
			end = i -1;
		} else {
			arr = &tab[i].Trans;
			break;
		}
	} while (beg <= end);
	
	return (*arr == 0xFFFF ? *(const WCHAR **)(arr +1) : arr);
}
#define bin_search(uni, tab) _bin_search (uni, tab, numberof(tab) -1)


/*==============================================================================
 *
 * Encoders for Unicode Encoded Fonts (TrueType/Type-1).
 *
*/
#include "en_uni.h"

#define BEG_UTF8 0x80


/*----------------------------------------------------------------------------*/
static WCHAR
utf8_to_unicode (const char ** psymbol)
{
	const char * s = *psymbol;
	long         ch = *(s++);
	BOOL         utf8ok;
	short        octets, mask, shift, i;

	/* Uses code from Kosta Kostis, utf8.c, 1.3, 2001-03-04. */
	/* calculate number of additional octets to be read,
	 * check validity of octets read
	 */
	octets = 0;
	utf8ok = TRUE;
	if ((ch >= 0xC0) && (ch <= 0xFD) && (s[0] >= 0x80) && (s[0] <= 0xBF)) {
		++octets;  /* 1 octet for U+0080..U+07FF */
		if (ch > 0xDF) {
			if ((s[1] >= 0x80) && (s[1] <= 0xBF)) {
				++octets;  /* 2 octets for U+0800..U+FFFF */
				if (ch > 0xEF) {
					if ((s[2] >= 0x80) && (s[2] <= 0xBF)) {
						++octets;  /* 3 octets for U+10000..U+1FFFFF */
						if (ch > 0xF7) {
							if ((s[3] >= 0x80) && (s[3] <= 0xBF)) {
								++octets;  /* 4 octets for U+200000..U+3FFFFFF */
								if (ch > 0xFB) {
									if ((s[4] >= 0x80) && (s[4] <= 0xBF)) {
										++octets;  /* 5 octets for U+4000000..U+7FFFFFFF */
									} else {
										utf8ok = FALSE;
									}
								}
							} else {
								utf8ok = FALSE;
							}
						}
					} else {
						utf8ok = FALSE;
					}
				}
			} else {
				utf8ok = FALSE;
			}
		}
	} else {
		utf8ok = FALSE;
	}

	/* nonshortest forms are illegal */
	if ((ch < 0xC2) || ((ch == 0xE0) && (s[0] < 0xA0))
	    || ((ch == 0xF0) && (s[0] < 0x90)) || ((ch == 0xF8) && (s[0] < 0x88))
	    || ((ch == 0xFC) && (s[0] < 0x84)))
		utf8ok = FALSE;

	if (utf8ok) {
		/* calculate value */
		mask = (1 << (6 - octets)) - 1;  /* 0x1F, 0x0F, 0x07, 0x03, 0x01 */
		shift = octets * 6;  /* 30, 24, 18, 12, 6 */
		ch = (ch & mask) << shift;
		for (i = 0, shift -= 6; i < octets ; ++i, shift -= 6)
			ch += (long)(*s++ & 0x3F) << shift;
	} else {
		ch = 0xFFFD;  /* U+FFFD REPLACEMENT CHARACTER */
		s += octets;
	}
	*psymbol = s;

	/* Surrogates Area not supported by NVDI */
	if ((ch >= 0xD800 && ch <= 0xDFFF) || ch >= 0xFFFE)
		ch = 0xFFFD;

	return ch;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
windows1252_to_utf16 (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_Windows1252_to_Unicode && ch <= END_Windows1252_to_Unicode) {
		*(dst++) = Windows1252_to_Unicode[ch - BEG_Windows1252_to_Unicode];
	} else if (ch >= 32 && ch != 127) {
		*(dst++) = ch;
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
iso8859_2_to_utf16 (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_ISO8859_2_to_Unicode) {
		*(dst++) = ISO8859_2_to_Unicode[ch - BEG_ISO8859_2_to_Unicode];
	} else if (ch >= 32 && ch < 127) {
		*(dst++) = ch;
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
iso8859_15_to_utf16 (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if      (ch == 0xA4) ch = 0x20AC;
	else if (ch == 0xA6) ch = 0x0160;
	else if (ch == 0xA8) ch = 0x0161;
	else if (ch == 0xB4) ch = 0x017D;
	else if (ch == 0xB8) ch = 0x017E;
	else if (ch == 0xBC) ch = 0x0152;
	else if (ch == 0xBD) ch = 0x0153;
	else if (ch == 0xBE) ch = 0x0178;
	if ((ch >= 32 && ch < 127) || ch >= 160) {
		*(dst++) = ch;
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
utf8_to_utf16 (const char ** src, WCHAR * dst)
{
	WCHAR ch = **src;

	if (ch >= BEG_UTF8) {
		ch  = utf8_to_unicode (src);
	} else {
		(*src)++;
	}
	if (ch >= 32 && ch != 127) {
		*(dst++) = ch;
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
utf16_to_utf16(const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*(const WCHAR **)src)++);

	/* Surrogates Area not supported by NVDI */
	if ((ch >= 0xD800 && ch <= 0xDFFF) || ch >= 0xFFFE)
		ch = 0xFFFD;

	/* some characters to ignore */
	if (ch == 0x070F || (ch >= 0x180B && ch <= 0x180E) ||
	    (ch >= 0x200B && ch <= 0x200D) || ch == 0x2060 ||
	    ch == 0x2061 || (ch >= 0xFE00 && ch <= 0xFE0F) || ch == 0xFEFF)
		ch = 0x0000;

	if (ch >= 32 && ch != 127) {
		*(dst++) = ch;
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
macintosh_to_utf16 (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_Macintosh_to_Unicode) {
		ch = Macintosh_to_Unicode[ch - BEG_Macintosh_to_Unicode];
	}
	if (ch >= 32 && ch != 127) {
		*(dst++) = ch;
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
atari_to_utf16 (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_Atari_to_Unicode) {
		*(dst++) = Atari_to_Unicode[ch - BEG_Atari_to_Unicode];
	} else if (ch >= 32) {
		*(dst++) = ch;
	} /* else ignore control characters */

	return dst;
}


/*==============================================================================
 *
 * Encoders for BICS Encoded Fonts (Speedo).
 *
*/
#include "en_bics.h"

#define uni_2_bics(uni) bin_search (uni, Unicode_to_BICS)

/*----------------------------------------------------------------------------*/
static WCHAR *
windows1252_to_bics (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);
	
	if (ch >= BEG_Windows1252_to_BICS) {
		*(dst++) = Windows1252_to_BICS[ch - BEG_Windows1252_to_BICS];
	} else if (ch == ' ') {
		*(dst++) = SPACE_BICS;
	} else if (ch > 32 && ch < 127) {
		*(dst++) = ch - 32;
	} /* else ignore control characters */
	
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
iso8859_2_to_bics (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);
	
	if (ch >= BEG_ISO8859_2_to_BICS) {
		*(dst++) = ISO8859_2_to_BICS[ch - BEG_ISO8859_2_to_BICS];
	} else if (ch == ' ') {
		*(dst++) = SPACE_BICS;
	} else if (ch > 32 && ch < 127) {
		*(dst++) = ch - 32;
	} /* else ignore control characters */
	
	/* what's about 0x80..0x9F ??? */
	
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
atari_to_bics (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);
	
	if (ch >= BEG_Atari_to_BICS) {
		*(dst++) = Atari_to_BICS[ch - BEG_Atari_to_BICS];
	} else if (ch == ' ') {
		*(dst++) = SPACE_BICS;
	} else if (ch > 32) {
		*(dst++) = ch - 32;
	} /* else ignore control characters */
	
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
unicode_to_bics (WCHAR uni, WCHAR * dst)
{
	if (uni <= END_Windows1252_to_BICS) {
		const char * src = ((char*)&uni) +1;
		dst = windows1252_to_bics (&src, dst);
	
	} else {
		const WCHAR * arr = uni_2_bics (uni);
		while (*arr) {
			*(dst++) = *(arr++);
		}
	}
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
utf8_to_bics (const char ** src, WCHAR * dst)
{
	WCHAR ch = **src;
	
	if (ch >= BEG_Windows1252_to_BICS) {
		ch  = utf8_to_unicode (src);
		dst = unicode_to_bics (ch, dst);
	} else {
		if (ch == ' ') {
			*(dst++) = SPACE_BICS;
		} else if (ch > 32 && ch < 127) {
			*(dst++) = ch - 32;
		} /* else ignore control characters */
		(*src)++;
	}
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
utf16_to_bics (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*(const WCHAR **)src)++);

	/* some characters to ignore */
	if (ch == 0x070F || (ch >= 0x180B && ch <= 0x180E) ||
	    (ch >= 0x200B && ch <= 0x200D) || ch == 0x2060 || ch == 0x2061 ||
	    (ch >= 0xFE00 && ch <= 0xFE0F) || ch == 0xFEFF) {
		ch = 0x0000;
	
	} else {
		/* Surrogates Area not supported by NVDI */
		if ((ch >= 0xD800 && ch <= 0xDFFF) || ch >= 0xFFFE) {
			ch = 0xFFFD;
		}
		if ((ch >= 32 && ch < 127) || ch >= 160) {
			dst = unicode_to_bics (ch, dst);
		} /* else ignore control characters */
	}

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
iso8859_15_to_bics (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if      (ch == 0xA4) ch = 0x20AC;
	else if (ch == 0xA6) ch = 0x0160;
	else if (ch == 0xA8) ch = 0x0161;
	else if (ch == 0xB4) ch = 0x017D;
	else if (ch == 0xB8) ch = 0x017E;
	else if (ch == 0xBC) ch = 0x0152;
	else if (ch == 0xBD) ch = 0x0153;
	else if (ch == 0xBE) ch = 0x0178;
	if ((ch >= 32 && ch < 127) || ch >= 160) {
		dst = unicode_to_bics (ch, dst);
	} /* else ignore control characters */
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
macintosh_to_bics (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_Macintosh_to_Unicode) {
		ch = Macintosh_to_Unicode[ch - BEG_Macintosh_to_Unicode];
	}
	return unicode_to_bics (ch, dst);
}


/*==============================================================================
 *
 * Encoders for ATARI System Encoded Fonts (*.FNT).
 *
*/
#include "en_atari.h"

#define uni_2_atari(uni) bin_search (uni, Unicode_to_Atari)


/*----------------------------------------------------------------------------*/
static WCHAR *
windows1252_to_atari (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);
	
	if (ch >= 128) {
		const WCHAR * arr = uni_2_atari (ch);
		while (*arr) {
			*(dst++) = *(arr++);
		}
	
	} else if (ch >= 32 && ch < 127) {
		*(dst++) = ch;
	} /* else ignore control characters */
	
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
iso8859_2_to_atari (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_ISO8859_2_to_Unicode) {
		ch = ISO8859_2_to_Unicode[ch - BEG_ISO8859_2_to_Unicode];
	}
	
	if (ch >= 128) {
		const WCHAR * arr = uni_2_atari (ch);
		while (*arr) {
			*(dst++) = *(arr++);
		}
	
	} else if (ch >= 32 && ch < 127) {
		*(dst++) = ch;
	} /* else ignore control characters */
	
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
iso8859_15_to_atari (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= 32 && ch < 127) {
		*(dst++) = ch;
	
	} else if (ch >= 160) {
		const WCHAR * arr;
		if      (ch == 0xA4) ch = 0x20AC;
		else if (ch == 0xA6) ch = 0x0160;
		else if (ch == 0xA8) ch = 0x0161;
		else if (ch == 0xB4) ch = 0x017D;
		else if (ch == 0xB8) ch = 0x017E;
		else if (ch == 0xBC) ch = 0x0152;
		else if (ch == 0xBD) ch = 0x0153;
		else if (ch == 0xBE) ch = 0x0178;
		arr = uni_2_atari (ch);
		while (*arr) {
			*(dst++) = *(arr++);
		}
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
utf8_to_atari (const char ** src, WCHAR * dst)
{
	WCHAR ch = **src;
	
	if (ch >= BEG_Windows1252_to_BICS) {
		const WCHAR * arr = uni_2_atari (utf8_to_unicode (src));
		while (*arr) {
			*(dst++) = *(arr++);
		}
	
	} else {
		if (ch >= 32 && ch < 127) {
			*(dst++) = ch;
		} /* else ignore control characters */
		(*src)++;
	}
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
utf16_to_atari (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*(const WCHAR **)src)++);

	/* some characters to ignore */
	if (ch == 0x070F || (ch >= 0x180B && ch <= 0x180E) ||
	    (ch >= 0x200B && ch <= 0x200D) || ch == 0x2060 || ch == 0x2061 ||
	    (ch >= 0xFE00 && ch <= 0xFE0F) || ch == 0xFEFF) {
		ch = 0x0000;
	
	} else {
		/* Surrogates Area not supported by NVDI */
		if ((ch >= 0xD800 && ch <= 0xDFFF) || ch >= 0xFFFE) {
			ch = 0xFFFD;
		}
		if (ch >= 128) {
			const WCHAR * arr = uni_2_atari (ch);
			while (*arr) {
				*(dst++) = *(arr++);
			}
		
		} else if (ch >= 32 && ch < 127) {
			*(dst++) = ch;
		} /* else ignore control characters */
	}

	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
macintosh_to_atari (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_Macintosh_to_Unicode) {
		ch = Macintosh_to_Unicode[ch - BEG_Macintosh_to_Unicode];
	}
	
	if (ch >= 128) {
		const WCHAR * arr = uni_2_atari (ch);
		while (*arr) {
			*(dst++) = *(arr++);
		}
	
	} else if (ch >= 32 && ch < 127) {
		*(dst++) = ch;
	} /* else ignore control characters */
	
	return dst;
}


/*----------------------------------------------------------------------------*/
static WCHAR *
atari_to_atari (const char ** src, WCHAR * dst)
{
	WCHAR ch = *((*src)++);
	
	if (ch >= 32) {
		*(dst++) = ch;
	} /* else ignore control characters */
	
	return dst;
}


/*==============================================================================
 * Select encoder function according to ENCODING to read one (possible
 * multibyte) character from 'src' and write 0 to 4 characters of 16 bit width
 * to 'dst'.  The 'src' content will be set behind the read character and the
 * address behind the written character.
 */
ENCODER_W
encoder_word (ENCODING encoding, WORD mapping)
{
	if (mapping == MAP_BITSTREAM) {
		switch (encoding) {
			case ENCODING_WINDOWS1252: return windows1252_to_bics;
			case ENCODING_ISO8859_2:   return iso8859_2_to_bics;
			case ENCODING_ISO8859_15:  return iso8859_15_to_bics;
			case ENCODING_UTF8:        return utf8_to_bics;
			case ENCODING_UTF16:       return utf16_to_bics;
			case ENCODING_MACINTOSH:   return macintosh_to_bics;
			case ENCODING_ATARIST:     return atari_to_bics;
			default:
				printf ("encoder_word('BITSTREAM'): invalid %i\n", encoding);
				return windows1252_to_bics;
		}
	} else if (mapping == MAP_UNICODE) {
		switch (encoding) {
			case ENCODING_WINDOWS1252: return windows1252_to_utf16;
			case ENCODING_ISO8859_2:   return iso8859_2_to_utf16;
			case ENCODING_ISO8859_15:  return iso8859_15_to_utf16;
			case ENCODING_UTF8:        return utf8_to_utf16;
			case ENCODING_UTF16:       return utf16_to_utf16;
			case ENCODING_MACINTOSH:   return macintosh_to_utf16;
			case ENCODING_ATARIST:     return atari_to_utf16;
			default:
				printf ("encoder_word('UNICODE'): invalid %i\n", encoding);
				return windows1252_to_utf16;
		}
	} else if (mapping == MAP_ATARI) {
		switch (encoding) {
			case ENCODING_WINDOWS1252: return windows1252_to_atari;
			case ENCODING_ISO8859_2:   return iso8859_2_to_atari;
			case ENCODING_ISO8859_15:  return iso8859_15_to_atari;
			case ENCODING_UTF8:        return utf8_to_atari;
			case ENCODING_UTF16:       return utf16_to_atari;
			case ENCODING_MACINTOSH:   return macintosh_to_atari;
			case ENCODING_ATARIST:     return atari_to_atari;
			default:
				printf ("encoder_word('ATARI'): invalid %i\n", encoding);
				return windows1252_to_atari;
		}
	}
	printf ("encoder_word(%i,%i): invalid mapping\n", mapping, encoding);
	return windows1252_to_bics;
}


/*============================================================================*/
WCHAR *
unicode_to_wchar (WCHAR uni, WCHAR * dst, WORD mapping)
{
	if (mapping == MAP_BITSTREAM) {
		dst = unicode_to_bics (uni, dst);
	
	} else if (mapping == MAP_UNICODE) {
		const char * src = (const char *)&uni;
		dst = utf16_to_utf16 (&src, dst);
	
	} else /*if (mapping == MAP_ATARI)*/ {
		if (uni < 0x0080u) {
			*(dst++) = (char)uni;
		
		} else {
			const WCHAR * arr = uni_2_atari (uni);
			while (*arr) {
				*(dst++) = (char)*(arr++);
			}
		}
	}
	return dst;
}


/*==============================================================================
 *
 * Encoders for 8-bit AES Strings.
 *
*/

/*----------------------------------------------------------------------------*/
char *
unicode_to_8bit (WCHAR uni, char * dst)
{
	if (uni < 0x0080u) {
		*(dst++) = (char)uni;
	
	} else {
		const WCHAR * arr = uni_2_atari (uni);
		while (*arr) {
			*(dst++) = (char)*(arr++);
		}
	}
	return dst;
}


/*----------------------------------------------------------------------------*/
static char *
windows1252_to_8bit (const char ** src, char * dst)
{
	WCHAR ch = *((*src)++);
	
	if (ch >= 128) {
		dst = unicode_to_8bit (ch, dst);
	} else {
		if (ch >= 32 && ch < 127) {
			*(dst++) = ch;
		} /* else ignore control characters */
	}
	return dst;
}


/*----------------------------------------------------------------------------*/
static char *
iso8859_2_to_8bit (const char ** src, char * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_ISO8859_2_to_Unicode) {
		ch = ISO8859_2_to_Unicode[ch - BEG_ISO8859_2_to_Unicode];
	}
	return unicode_to_8bit (ch, dst);
}


/*----------------------------------------------------------------------------*/
static char *
iso8859_15_to_8bit (const char ** src, char * dst)
{
	WCHAR ch = *((*src)++);

	if      (ch == 0xA4) ch = 0x20AC;
	else if (ch == 0xA6) ch = 0x0160;
	else if (ch == 0xA8) ch = 0x0161;
	else if (ch == 0xB4) ch = 0x017D;
	else if (ch == 0xB8) ch = 0x017E;
	else if (ch == 0xBC) ch = 0x0152;
	else if (ch == 0xBD) ch = 0x0153;
	else if (ch == 0xBE) ch = 0x0178;
	if ((ch >= 32 && ch < 127) || ch >= 160) {
		dst = unicode_to_8bit (ch, dst);
	} /* else ignore control characters */

	return dst;
}


/*----------------------------------------------------------------------------*/
static char *
utf8_to_8bit (const char ** src, char * dst)
{
	WCHAR ch = **src;
	
	if (ch >= BEG_Windows1252_to_BICS) {
		ch  = utf8_to_unicode (src);
		dst = unicode_to_8bit (ch, dst);
	} else {
		if (ch >= 32 && ch < 127) {
			*(dst++) = ch;
		} /* else ignore control characters */
		(*src)++;
	}
	return dst;
}


/*----------------------------------------------------------------------------*/
static char *
utf16_to_8bit (const char ** src, char * dst)
{
	WCHAR ch = *((*(const WCHAR **)src)++);
	
	if (ch >= BEG_Windows1252_to_BICS) {
		dst = unicode_to_8bit (ch, dst);
	} else {
		if (ch >= 32 && ch < 127) {
			*(dst++) = ch;
		} /* else ignore control characters */
	}
	return dst;
}


/*----------------------------------------------------------------------------*/
static char *
macintosh_to_8bit (const char ** src, char * dst)
{
	WCHAR ch = *((*src)++);

	if (ch >= BEG_Macintosh_to_Unicode) {
		ch = Macintosh_to_Unicode[ch - BEG_Macintosh_to_Unicode];
	}
	return unicode_to_8bit (ch, dst);
}


/*----------------------------------------------------------------------------*/
static char *
atari_to_8bit (const char ** src, char * dst)
{
	WCHAR ch = *((*src)++);
	
	if (ch >= 32) {
		*(dst++) = ch;
	} /* else ignore control characters */
	
	return dst;
}


/*==============================================================================
 * Select encoder function according to ENCODING to read one (possible
 * multibyte) character from 'src' and write 0 to 4 characters of 8 bit width
 * to 'dst'.  The 'src' content will be set behind the read character and the
 * address behind the written character.
 */
ENCODER_C
encoder_char (ENCODING encoding)
{
	switch (encoding) {
		case ENCODING_WINDOWS1252: return windows1252_to_8bit;
		case ENCODING_ISO8859_2:   return iso8859_2_to_8bit;
		case ENCODING_ISO8859_15:  return iso8859_15_to_8bit;
		case ENCODING_UTF8:        return utf8_to_8bit;
		case ENCODING_UTF16:       return utf16_to_8bit;
		case ENCODING_MACINTOSH:   return macintosh_to_8bit;
		case ENCODING_ATARIST:     return atari_to_8bit;
		default: /* error */ ;
	}
	printf ("encoder_char(): invalid %i\n", encoding);
	return windows1252_to_8bit;
}
