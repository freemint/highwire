/* @(#)highwire/scanner.c
 *
 * Scanner functions for HTML expressions.
 *
 * In case of extending one of th expression lists in token.h take care that
 * some of the functions below uses buffers for upcase conversion, which must
 * match the length of the longest expression in the list!
 *
 * AltF4 - Jan 05, 2002
 *
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hw-types.h"
#include "token.h"
#include "scanner.h"


/*==============================================================================
 * Scanner for html TAG name expressions inside of
 *    <TAG>  |  <TAG ...>  |  <TAG(n) ...>
 * At start time the input stream pointer must be positioned to the first
 * character to be read (behind some leading '<' or '/').  After processing it
 * is set to first character behind the expression.
 * If not successful the symbol TAG_Unknown is returned.
 *
 * The tag name lookup table is built by including the token.h header file with
 * the __TAG_ITEM() macro defined.
 *
 * AltF4 - Jan 05, 2002
 */
struct TAG {
	const char  * Name;
	const HTMLTAG Tag;
};
static const struct TAG tag_table[] = {
	#define __TAG_ITEM(t) { #t, TAG_##t }
	#include "token.h"
};
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
HTMLTAG
scan_tag (const char ** pptr)
{
	HTMLTAG      tag  = TAG_Unknown;
	const char * line = *pptr;
	size_t       len  = 0;
	char buf[12], c;

	int beg = 0;
	int end = (int)numberof(tag_table) -1;

	while ((c = *line) > ' ') {
		if (len < sizeof(buf)) {
			if (isalpha (c)) {
				buf[len++] = toupper(c);
				line++;
			} else {
				if (c >= '1' && c <= '6') {
					buf[len++] = c;
					line++;
				}
				break;
			}
		} else { /* expression is longer than the longest known tag */
			while (isalpha (*(line))) line++;
			len = 0;   /* reset, don't perform a search */
			break;
		}
	}
	*pptr = line;

	if (len) do {
		int i = (end + beg) /2;
		int d = strncmp (buf, tag_table[i].Name, len);
		if (d > 0) {
			beg = i +1;
		} else if (d || tag_table[i].Name[len]) {
			end = i -1;
		} else {
			tag = tag_table[i].Tag;
			break;
		}
	} while (beg <= end);

	return tag;
}


/*==============================================================================
 * Scanner for html variable KEY name expressions inside of
 *    <TAG KEY ...>  |  <TAG KEY= ...>
 * At start time the input stream pointer must be positioned to the first
 * character to be read (behind some leading white space).  After processing it
 * is set to first character behind the expression.
 * If 'lookup' is set to FALSE only the input stream pointer will be modified
 * but no name lookup is performed.  In this case or if not successful the
 * symbol KEY_Unknown is returned.
 *
 * The tag name lookup table is built by including the token.h header file with
 * the __KEY_ITEM() macro defined.
 *
 * AltF4 - Jan 05, 2002
 */
struct KEY {
	const char  * Name;
	const HTMLKEY Key;
};
static const struct KEY key_table[] = {
	#define __KEY_ITEM(k) { #k, KEY_##k }
	#include "token.h"
};
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
HTMLKEY
scan_key (const char ** pptr, BOOL lookup)
{
	HTMLKEY      key  = KEY_Unknown;
	const char * line = *pptr;
	size_t       len  = 0;
	char buf[12], c;

	int beg = 0;
	int end = (int)numberof(key_table) -1;

	if (lookup) while (isalpha (c = *line) || (c == '-')) {
		line++;
		if (len < sizeof(buf)) {
			/* The above preprocessor construction doesn't work for HTTP-EQUIV.
			 * So we convert '-' to '_'.
			 */
			buf[len++] = (c == '-')? '_' : toupper(c);

		} else { /* expression is longer than the longest known key */
			lookup = FALSE;
			break;
		}
	}
	if (!lookup) {
		while (isalpha (c = *line) || (c == '-')) line++;
		len = 0;   /* reset, don't perform a search */
	}
	*pptr = line;

	if (len) do {
		int i = (end + beg) /2;
		int d = strncmp (buf, key_table[i].Name, len);
		if (d > 0) {
			beg = i +1;
		} else if (d || key_table[i].Name[len]) {
			end = i -1;
		} else {
			key = key_table[i].Key;
			break;
		}
	} while (beg <= end);

	return key;
}


/*==============================================================================
 */
struct CSS {
	const char  * Name;
	const HTMLCSS Key;
	const HTMLKEY Equiv;
};
static const struct CSS css_table[] = {
	#define __CSS_ITEM(k,e) { #k, CSS_##k, e }
	#include "token.h"
};
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
short
scan_css (const char ** pptr, size_t length)
{
	short        key  = CSS_Unknown;
	const char * line = *pptr;
	size_t       len  = 0;
	char buf[18];

	while (len < length) {
		char c = *line;
		if (c == '-' && len) {
			buf[len++] = '_';
		} else if (isalpha (c)) {
			buf[len++] = toupper(c);
		} else {
			break;
		}
		if (len >= sizeof(buf)) {
			/* expression is longer than the longest known key */
			length -= len;
			len    =  0;
			break;
		}
		line++;
	}
	length -= len;
	
	if (len) {
		int beg = 0;
		int end = (int)numberof(css_table) -1;
		do {
			int i = (end + beg) /2;
			int d = strncmp (buf, css_table[i].Name, len);
			if (d > 0) {
				beg = i +1;
			} else if (d || css_table[i].Name[len]) {
				end = i -1;
			} else {
				key = (css_table[i].Equiv > 0 ? css_table[i].Equiv
				                              : css_table[i].Key);
				break;
			}
		} while (beg <= end);
		
		if (key == CSS_Unknown) {
			beg = 0;
			end = (int)numberof(key_table) -1;
			do {
				int i = (end + beg) /2;
				int d = strncmp (buf, key_table[i].Name, len);
				if (d > 0) {
					beg = i +1;
				} else if (d || key_table[i].Name[len]) {
					end = i -1;
				} else {
					key = key_table[i].Key;
					break;
				}
			} while (beg <= end);
		}
	}
	
	if (key != CSS_Unknown) {
		while (length-- && isspace (*line)) line++;
	} else {
		while (length-- && *line != ';') line++;
	}
	*pptr = line;
	
	return key;
}


/*============================================================================*/
BOOL
scan_numeric (const char ** pptr, long * num, UWORD * unit)
{
	BOOL   ok = TRUE;
	char * ptr;
	BOOL   neg;
	long   size;
	
	while (isspace (**pptr)) (*pptr)++;
	neg  = (**pptr == '-');
	size = strtol (*pptr, &ptr, 10);
	
	if (ptr <= *pptr) {
		*num = 0;
		return FALSE;
	
	} else if (size < -32768l || size > +32767l) {
		*num = size;
		ok   = FALSE;
	
	} else if (size < 0l) {
		size = -size;
	}
	if (*ptr == '.' && isdigit(*(++ptr))) {
		size = size * 10 + (*ptr - '0');
		if (!isdigit(*(++ptr))) {
			size = ((size <<8) +5) /10;
		} else {
			size = (((size * 10 + (*ptr - '0')) <<8) +5) /100;
			while (isdigit(*(++ptr)));
		}
	} else {
		size <<= 8;
	}
	if (ok) {
		*num = (neg ? -size : +size);
	}
	
	switch (toupper(*ptr)) {
		#define _(u,l) (((UWORD)(u)<<8)|(l))
		case 'E':
			if      (toupper(ptr[1]) == 'M') { *unit = _('E','M'); ptr += 2; }
			else if (toupper(ptr[1]) == 'X') { *unit = _('E','X'); ptr += 2; }
			else                               *unit = 0;
			break;
		case 'P':
			if      (toupper(ptr[1]) == 'C') { *unit = _('P','C'); ptr += 2; }
			else if (toupper(ptr[1]) == 'T') { *unit = _('P','T'); ptr += 2; }
			else if (toupper(ptr[1]) == 'X') { *unit = _('P','X'); ptr += 2; }
			else                               *unit = 0;
			break;
		case 'M':
			if      (toupper(ptr[1]) == 'M') { *unit = _('M','M'); ptr += 2; }
			else                               *unit = 0;
			break;
		case 'C':
			if      (toupper(ptr[1]) == 'M') { *unit = _('C','M'); ptr += 2; }
			else                               *unit = 0;
			break;
		case 'I':
			if      (toupper(ptr[1]) == 'N') { *unit = _('I','N'); ptr += 2; }
			else                               *unit = 0;
			break;
		case '%':                           { *unit = _('%',0);   ptr += 1; }
			break;
		default:                              *unit = 0;
		#undef _
	}
	*pptr = ptr;
	
	return ok;
}


/*==============================================================================
 * Scanner for named character expressions of the forms
 *    &  |  &<expr>;  |  &#<dec_num>;  |  &#x<hex_num>;
 * If successful the input stream pointer is set to the last character of the
 * expression (not behind!), normally the ';', and the translated character is
 * returned.
 * Else the input stream pointer isn't modified and an '&' is returned, so the
 * faulty expression can be displayed.  This also allows to display incorrect
 * used single '&'s in the html code.
 *
 * The name lookup table below must always be sorted by ASCII.  Otherwise the
 * search algorithm won't find missplaced entries.  That is digits, upper case
 * letters, lower case letters.
 */

struct NAMEDCHAR {
	const char * Name;
	const WCHAR  Code; /* Unicode */
};
static const struct NAMEDCHAR named_char[] = {
	/* A */ {"AElig",  198},{"Aacute",  193},{"Acirc",  194},{"Agrave", 192},
	        {"Alpha",  913},{"Aring",   197},{"Atilde", 195},{"Auml",   196},
	/* B */ {"Beta",   914},
	/* C */ {"COPY",   169},{"Ccaron", 268},{"Ccedil",  199},{"Chi",    935},
	/* D */ {"Dagger",8225},{"Delta",   916},
	/* E */ {"ETH",    208},{"Eacute",  201},{"Ecirc",  202},{"Egrave", 200},
	        {"Epsilon",917},{"Eta",     919},{"Euml",   203},
	/* G */ {"Gamma",  915},
	/* I */ {"Iacute", 205},{"Icirc",   206},{"Igrave", 204},{"Iota",   921},
	        {"Iuml",   207},
	/* K */ {"Kappa",  922},
	/* L */ {"Lambda", 923},
	/* M */ {"Mu",     924},
	/* N */ {"Ntilde", 209},{"Nu",      925},
	/* O */ {"OElig",  338},{"Oacute", 211},{"Ocirc",   212},{"Ograve", 210},
	        {"Omega",  937},{"Omicron", 927},{"Oslash", 216},{"Otilde", 213},
	        {"Ouml",   214},
	/* P */ {"Phi",    934},{"Pi",      928},{"Prime", 8243},{"Psi",    936},
	/* R */ {"Rho",    929},
	/* S */ {"Scaron", 352},{"Sigma",   931},
	/* T */ {"THORN",  222},{"Tau",     932},{"Theta",  920},
	/* U */ {"Uacute", 218},{"Ucirc",   219},{"Ugrave", 217},{"Upsilon",933},
	        {"Uuml",   220},
	/* X */ {"Xi",     926},
	/* Y */ {"Yacute", 221},{"Yuml",    376},
	/* Z */ {"Zcaron", 381},{"Zeta",    918},
	/* a */ {"aacute", 225},{"acirc",   226},{"acute",  180},{"aelig",  230},
	        {"agrave", 224},{"alefsym",8501},{"alpha",  945},{"amp",     38},
	        {"and",   8869},{"ang",    8736},{"apos",    39},{"aring",  229},
	        {"asymp", 8776},
	     /* {"ataril",32,14},{"atarir",32,15}, pseudocodes */
	        {"atilde", 227},{"auml",    228},
	/* b */ {"bdquo", 8222},{"beta",    946},{"brvbar", 166},{"bull",  8226},
	/* c */ {"cap",   8745},{"ccaron",  269},{"ccedil", 231},{"cedil",  184},
	        {"cent",   162},{"chi",     967},{"circ",   710},{"clubs", 9827},
	        {"cong",  8773},{"copy",    169},{"crarr", 8629},{"cup",   8746},
	        {"curren", 164},
	/* d */ {"dArr",  8659},{"dagger", 8224},{"darr",  8595},{"deg",    176},
	        {"delta",  948},{"diams",  9830},{"divide", 247},
	/* e */ {"eacute", 233},{"ecirc",   234},{"egrave", 232},{"empty", 8709},
	        {"emsp",  8195},{"ensp",   8194},{"epsilon",949},{"equiv", 8801},
	        {"eta",    951},{"eth",     240},{"euml",   235},{"euro",  8364},
	        {"exist", 8707},
	/* f */ {"fnof",   402},{"forall", 8704},{"frac12", 189},{"frac14", 188},
	        {"frac34", 190},{"frasl",  8260},
	/* g */ {"gamma",  947},{"ge",     8805},{"gt",      62},
	/* h */ {"hArr",  8660},{"harr",   8596},{"hearts",9829},{"hellip",8230},
	/* i */ {"iacute", 237},{"icirc",   238},{"iexcl",  161},{"igrave", 236},
	        {"image", 8465},{"infin",  8734},{"int",   8747},{"iota",   953},
	        {"iquest", 191},{"isin",   8712},{"iuml",   239},
/*	{"jnodot",176,106},  Only SGML, no HTML, no Unicode! */
	/* k */ {"kappa",  954},
	/* l */ {"lArr",  8656},{"lambda",  955},{"lang",  9001},{"laquo",  171},
	        {"larr",  8592},{"lceil",  8968},{"ldquo", 8220},{"le",    8804},
	        {"lfloor",8970},{"lowast", 8727},{"loz",   9674},{"lrm",   8206},
	        {"lsaquo",8249},{"lsquo",  8216},{"lt",      60},
	/* m */ {"macr",   175},{"mdash",  8212},{"micro",  181},{"middot", 183},
	        {"minus", 8722},{"mu",      956},
	/* n */ {"nabla", 8711},{"nbsp",    160},{"ndash", 8211},{"ne",    8800},
	        {"ni",    8715},{"not",     172},{"notin", 8713},{"nsub",  8836},
	        {"ntilde", 241},{"nu",      957},
	/* o */ {"oacute", 243},{"ocirc",   244},{"oelig",  339},{"ograve", 242},
	        {"oline", 8254},{"omega",   969},{"omicron",959},{"oplus", 8853},
	        {"or",    8870},{"ordf",    170},{"ordm",   186},{"oslash", 248},
	        {"otilde", 245},{"otimes", 8855},{"ouml",   246},
	/* p */ {"para",   182},{"part",   8706},{"permil",8240},{"perp",  8869},
	        {"phi",    966},{"pi",      960},{"piv",    982},{"plusmn", 177},
	        {"pound",  163},{"prime",  8242},{"prod",  8719},{"prop",  8733},
	        {"psi",    968},
	/* q */ {"quot",    34},
	/* r */ {"rArr",  8658},{"radic",  8730},{"rang",  9002},{"raquo",  187},
	        {"rarr",  8594},{"rceil",  8969},{"rdquo", 8221},{"real",  8476},
	        {"reg",    174},{"rfloor", 8971},{"rho",    961},{"rlm",   8207},
	        {"rsaquo",8250},{"rsquo",  8217},
	/* s */ {"sbquo", 8218},{"scaron",  353},{"sdot",  8901},{"sect",   167},
	        {"shy",    173},{"sigma",   963},{"sigmaf", 962},{"sim",   8764},
	        {"spades",9824},{"sub",    8834},{"sube",  8838},{"sum",   8721},
	        {"sup",   8835},{"sup1",    185},{"sup2",   178},{"sup3",   179},
	        {"supe",  8839},{"szlig",   223},
	/* t */ {"tau",    964},{"there4", 8756},{"theta",  952},{"thetasym",977},
	        {"thinsp",8201},{"thorn",   254},{"tilde", 8764},{"times",  215},
	        {"trade", 8482},
	/* u */ {"uArr",  8657},{"uacute",  250},{"uarr",  8593},{"ucirc",  251},
	        {"ugrave", 249},{"uml",     168},{"upsih",  978},{"upsilon",965},
	        {"uuml",   252},
	/* w */ {"weierp",8472},
	/* x */ {"xi",     958},
	/* y */ {"yacute", 253},{"yen",     165},{"yuml",   255},
	/* z */ {"zcaron", 382},{"zeta",    950},{"zwj",   8205},{"zwnj",  8204}
};

/* AltF4 December 19, 2001: completely redisigned, uses binary tree search now
 *
 * Rainer Seitel 2002-03-03: Corrected the tables. Added characters from
 * codepage 1252 and HTML 4. The Atari Compendium, Appendix G.7.
 * It would be a better idea to map the named characters to Unicode. After this,
 * including the numberd characters above 255, they would be printed in Unicode
 * mode with NVDI 4.11 and Unicode TTF, or mapped to BICS (as present).
 * Further more than one replacement characters should be possible.
 */

void *
scan_namedchar (const char ** pptr, void * dst, BOOL wordNchar, WORD mapping)
{
	const char * str = ++(*pptr);
	WCHAR        uni = 0;
	
	if (str[0] == '#') {  /* &#<number>; */
		char * next = NULL;
		long   val;
		int    radix;

		if (*(++str) == 'x') {
			radix = 16;
			str++;
		} else {
			radix = 10;
		}
		if ((val = strtol (str, &next, radix)) >= 32 && val <= 0xFFFDL) {
			uni = val;
			str = next;
		}
		/* Check for idiotic web authors who think that M$ is the measure of all
		 * things and use the code page 1252 numeric encoding instead of a valid
		 * Unicode value.  So we might be used to recode manually here.
		*/
		if (mapping == MAP_UNICODE && uni >= 0x80 && uni <= 0x9F) {
			const char * src = (const char *)&uni +1;
			(*encoder_word (ENCODING_WINDOWS1252, MAP_UNICODE))(&src, &uni);
		}
	
	} else {  /* &<name>; */
		int   beg = 0;
		int   end = (int)numberof(named_char) - 1;
		short len = 0;
		while (isalnum (str[len])) len++;
		do {
			int i = (end + beg) / 2;
			int d = strncmp (str, named_char[i].Name, len);
			if (d > 0) {
				beg = i + 1;
			} else if (d || named_char[i].Name[len]) {
				end = i - 1;
			} else {
				uni =  named_char[i].Code;
				str += len;
				break;
			}
		} while (beg <= end);
	}
	
	if (wordNchar) { /* write 16-bit BICS or Unicode */
		WCHAR * w;
		if (!uni) {
			w = unicode_to_wchar (0x0026, dst, mapping); /* ampersand */
		} else {
			w = unicode_to_wchar (uni, dst, mapping);
			if (!mapping) {
				if (*(WCHAR*)dst == 563) {
					*(WCHAR*)dst = 6;
					uni = 0;
				}
			} else {
				if (*(WCHAR*)dst == 6) {
					*(WCHAR*)dst = '&';
					uni = 0;
				}
			}
		}
		dst = w;
		
	} else { /* write 8-bit Atari System Font */
		char * c = dst;
		if (!uni) {
			*(c++) = '&'; /* ampersand */
		} else {
			c = unicode_to_8bit (uni, dst);
			if (*(char*)dst == 6) {
				*(char*)dst = '&';
				uni = 0;
			}
		}
		dst = c;
	}
	if (uni) {
		*pptr = (*str == ';' ? str +1 : str);
	}
	return dst;
}


/*==============================================================================
 * Scanner for color expressions of the forms
 *    <name>  |  #<RGB hex value> | rgb(<r>,<g>,<b>)
 * If successful the value will be returned as a 24-bit RRGGBB long, else -1
 * will be returned.
 *
 * The color name lookup table below must always be sorted case insensitive,
 * else the search algorithm won't find missplaced entries.
 *
 */
struct COLORNAME {
	const char * Name;
	const long   Value;
};
static const struct COLORNAME color[] = {
	{"aliceblue",       0xF0F8FFL}, {"antiquewhite",  0xFAEBD7L},
	{"aqua",            0x00FFFFL}, {"aquamarine",    0x7FFFD4L},
	{"azure",           0xF0FFFFL},
	{"beige",           0xF5F5DCL}, {"black",         0x000000L},
	{"blue",            0x0000FFL}, {"blueviolet",    0x8A2BE2L},
	{"brown",           0xA52A2AL}, {"burlywood",     0xDEB887L},
	{"cadetblue",       0x5F9EA0L}, {"chartreuse",    0x7FFF00L},
	{"chocolate",       0xD2691EL}, {"coral",         0xFF7F50L},
	{"cornflowerblue",  0x6495EDL}, {"cornsilk",      0xFFF8DCL},
	{"crimson",         0xDC143CL}, {"cyan",          0x33FFFFL},
	{"darkblue",        0x00008BL}, {"darkcyan",      0x008B8BL},
	{"darkgoldenrod",   0xB8860BL}, {"darkgray",      0xA9A9A9L},
	{"darkgreen",       0x006400L}, {"darkkhaki",     0xBDB76BL},
	{"darkmagenta",     0x8B008BL}, {"darkolivegreen",0x556B2FL},
	{"darkorange",      0xFF8C00L}, {"darkorchid",    0x9932CCL},
	{"darkred",         0x8B0000L}, {"darksalmon",    0xE9967AL},
	{"darkseagreen",    0x8FBC8FL}, {"darkslateblue", 0x483D8BL},
	{"darkslategray",   0x2F4F4FL}, {"darkturquoise", 0x00CED1L},
	{"darkviolet",      0x9400D3L}, {"deeppink",      0xFF1493L},
	{"deepskyblue",     0x00BFFFL}, {"dimgray",       0x696969L},
	{"dodgerblue",      0x1E90FFL},
	{"firebrick",       0xB22222L}, {"floralwhite",   0xFFFAF0L},
	{"forestgreen",     0x228B22L}, {"fuchsia",       0xFF00FFL},
	{"gainsboro",       0xDCDCDCL}, {"ghostwhite",    0xF8F8FFL},
	{"gold",            0xFFD700L}, {"goldenrod",     0xDAA520L},
	{"gray",            0x808080L}, {"green",         0x008000L},
	{"greenyellow",     0xADFF2FL}, {"grey",          0x808080L},
	{"honeydew",        0xF0FFF0L}, {"hotpink",       0xFF69B4L},
	{"indianred",       0xCD5C5CL}, {"indigo",        0x4B0082L},
	{"ivory",           0xFFFFF0L},
	{"khaki",           0xF0E68CL},
	{"lavender",        0xE6E6FAL}, {"lavenderblush", 0xFFF0F5L},
	{"lawngreen",       0x7CFC00L}, {"lemonchiffon",  0xFFFACDL},
	{"lightblue",       0xADD8E6L}, {"lightcoral",    0xF08080L},
	{"lightcyan",       0xE0FFFFL}, {"lightgoldenrodyellow", 0xFAFAD2L},
	{"lightgreen",      0x90EE90L}, {"lightgrey",     0xD3D3D3L},
	{"lightpink",       0xFFB6C1L}, {"lightsalmon",   0xFFA07AL},
	{"lightseagreen",   0x20B2AAL}, {"lightskyblue",  0x87CEFAL},
	{"lightslategray",  0x778899L}, {"lightsteelblue",0xB0C4DEL},
	{"lightyellow",     0xFFFFE0L}, {"lime",          0x00FF00L},
	{"limegreen",       0x32CD32L}, {"linen",         0xFAF0E6L},
	{"magenta",         0xFF00FFL},
	{"maroon",          0x800000L}, {"mediumaquamarine",0x66CDAAL},
	{"mediumblue",      0x0000CDL}, {"mediumorchid",  0xBA55D3L},
	{"mediumpurple",    0x9370DBL}, {"mediumseagreen",0x3CB371L},
	{"mediumslateblue", 0x7B68EEL}, {"mediumspringgreen", 0x00FA9AL},
	{"mediumturquoise", 0x48D1CCL}, {"mediumvioletred", 0xC71585L},
	{"midnightblue",    0x191970L}, {"mintcream",     0xF5FFFAL},
	{"mistyrose",       0xFFE4E1L}, {"moccasin",      0xFFE4B5L},
	{"navajowhite",     0xFFDEADL}, {"navy",          0x000080L},
	{"oldlace",         0xFDF5E6L}, {"olive",         0x808000L},
	{"olivedrab",       0x6B8E23L}, {"orange",        0xFFA500L},
	{"orangered",       0xFF4500L}, {"orchid",        0xDA70D6L},
	{"palegoldenrod",   0xEEE8AAL}, {"palegreen",     0x98FB98L},
	{"paleturquoise",   0xAFEEEEL}, {"palevioletred", 0xDB7093L},
	{"papayawhip",      0xFFEFD5L}, {"peachpuff",     0xFFDAB9L},
	{"peru",            0xCD853FL}, {"pink",          0xFFC0CBL},
	{"plum",            0xDDA0DDL}, {"powderblue",    0xB0E0E6L},
	{"purple",          0x800080L},
	{"red",             0xFF0000L}, {"rosybrown",     0xBC8F8FL},
	{"royalblue",       0x4169E1L},
	{"saddlebrown",     0x8B4513L}, {"salmon",        0xFA8072L},
	{"sandybrown",      0xF4A460L}, {"seagreen",      0x2E8B57L},
	{"seashell",        0xFFF5EEL}, {"sienna",        0xA0522DL},
	{"silver",          0xC0C0C0L}, {"skyblue",       0x87CEEBL},
	{"slateblue",       0x6A5ACDL}, {"slategray",     0x708090L},
	{"snow",            0xFFFAFAL}, {"springgreen",   0x00FF7FL},
	{"steelblue",       0x4682B4L},
	{"tan",             0xD2B48CL}, {"teal",          0x008080L},
	{"thistle",         0xD8BFD8L}, {"tomato",        0xFF6347L},
	{"turquoise",       0x40E0D0L},
	{"violet",          0xEE82EEL},
	{"wheat",           0xF5DEB3L}, {"white",         0xFFFFFFL},
	{"whitesmoke",      0xF5F5F5L},
	{"yellow",          0xFFFF00L}, {"yellowgreen",   0x9ACD32L}
};
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
long
scan_color (const char * text, size_t len)
{
	char name[22];
	long col = -1;

	if (len) {
		if (text[0] == '#') {
			if (!isspace(text[1])) {
				char * end;
				col = strtol (text + 1, &end, 16);
				if (end == text +4) {
					/* three digit #RGB format in CSS */
					col = ((((col & 0xF00) <<4) | (col & 0xF00)) <<8)
					    | ((((col & 0x0F0) <<4) | (col & 0x0F0)) <<4)
					    | ((((col & 0x00F) <<4) | (col & 0x00F))    );
				}
				if (col > 0xFFFFFFL) {
					col = -1;
				}
			}
		
		} else if (isxdigit(text[0]) && isxdigit(text[1]) && isxdigit(text[2]) &&
		           isxdigit(text[3]) && isxdigit(text[4]) && isxdigit(text[5])) {
			col = strtol (text, NULL, 16);
			if (col > 0xFFFFFFL) {
				col = -1;
			}
		
		} else if (len > 4 && strnicmp (text, "rgb(", 4) == 0) {
			char * rgb = (char*)&col;
			short  i   = 3;
			col  =  0;
			text += 4;
			do {
				char * p;
				long   c = strtol (text, &p, 10);
				while (isspace(*p)) p++;
				if (*(text = p) == '%') {
					c = (255 * c + 50) /100;
					while (isspace(*(++text)));
				}
				++rgb;
				if (c > 0) *rgb = (c < 255 ? (char)c : 255);
				if (*text != ',') break;
				text++;
			} while (--i);
		
		} else if (len < sizeof(name)) {
			int beg = 0;
			int end = (int)numberof(color) - 1;
			char * p = name;

			while (len--) *(p++) = tolower(*(text++));
			*p = '\0';

			do {
				int i = (end + beg) / 2;
				int d = strcmp (name, color[i].Name);
				if (d > 0) {
					beg = i + 1;
				} else if (d) {
					end = i - 1;
				} else {
					col = color[i].Value;
					break;
				}
			} while (beg <= end);
		}
	}

	return col;
}


/*==============================================================================
 * scan_string_to_16bit() is used to read a string parameter of a HTML key.
 * It merges white spaces and converts named entities.
 * Rainer Seitel, 2002-05-05
 */
#include "defs.h"

void
scan_string_to_16bit (const char *symbol, const ENCODING source_encoding,
                      WCHAR **pwstring, WORD mapping)
{
	ENCODER_W encoder = encoder_word (source_encoding, mapping);
	WCHAR   * wstring = *pwstring;
	BOOL  space_found = TRUE;
	WCHAR space_code  = (mapping == MAP_BITSTREAM ? SPACE_BICS : SPACE_ATARI);

	while (*symbol != '\0') {
		switch (*symbol)
		{
			case '&':
				/* the 'symbol' pointer will be set by the scanner function
				 * to the expression end, ';' or '\0'.
				 */
				wstring = scan_namedchar (&symbol, wstring, TRUE, mapping);
				space_found = FALSE;
				break;
			case 9:  /* HT HORIZONTAL TABULATION */
			case 10: /* LF LINE FEED */
			case 11: /* VT VERTICAL TABULATION */
			case 12: /* FF FORM FEED */
			case 13: /* CR CARRIAGE RETURN */
			case ' ':
				if (!space_found) {
					*(wstring++) = space_code;
					space_found = TRUE;
				}
				symbol++;
				break;
			default:
				wstring = (*encoder)(&symbol, wstring);
				space_found = FALSE;
		}
	}
	if (space_found && wstring > *pwstring) {
		wstring--;
	}
	*pwstring = wstring;
}


/*==============================================================================
 * Scanner for character set resp. encoding names.
 * The parameter 'encoding' is a default return value.  If you need to know
 * about unrecognized encoding, use ENCODING_Unknown.  Else use a predefined
 * value or ENCODING_WINDOWS1252.
 */
ENCODING scan_encoding (const char *text, ENCODING encoding)
{
	/* http://www.iana.org/assignments/character-sets */
	if (stricmp(text, "ISO-8859-1") == 0 || stricmp(text, "windows-1252") == 0)
		encoding = ENCODING_WINDOWS1252;
	else if (stricmp(text, "ISO-8859-2") == 0)
		encoding = ENCODING_ISO8859_2;
	else if (stricmp(text, "ISO-8859-15") == 0)
		encoding = ENCODING_ISO8859_15;
	else if (stricmp(text, "UTF-8") == 0)
		encoding = ENCODING_UTF8;
	else if (stricmp(text, "macintosh") == 0)
		encoding = ENCODING_MACINTOSH;
	else if (stricmp(text, "atarist") == 0)  /* HighWire extension */
		encoding = ENCODING_ATARIST;
	else if (stricmp(text, "atarinvdi") == 0)  /* HighWire extension */
		encoding = ENCODING_ATARINVDI;

/*	logprintf(LOG_LGREEN, "scan_encoding(%s): %d\n", text, encoding);*/
	return encoding;
}
