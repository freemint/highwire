/* @(#)highwire/scanner.h
 *
 * Scanner functions for HTML expressions.
 *
 * AltF4 - Jan 05, 2002
 *
 */
#ifndef __SCANNER_H__
#define __SCANNER_H__


#ifdef HTMLTAG
HTMLTAG scan_tag (const char ** pptr);
				/* Scanner for html TAG name expressions inside of
				 *    <TAG>  |  <TAG ...>  |  <TAG(n) ...>
				 * The 'pptr' content must point to the first character to be read
				 * (behind some leading '<' or '/'). After processing it is set to
				 * first character behind the expression.
				 * If not successful the symbol TAG_Unknown is returned.
				*/
#endif
#ifdef HTMLKEY
HTMLKEY scan_key (const char ** pptr, BOOL lookup);
				/* Scanner for html variable KEY name expressions inside of
				 *    <TAG KEY ...>  |  <TAG KEY= ...>
				 * At start time the 'pptr' content must point to the first
				 * character to be read (behind some leading white space).  After
				 * processing it is set to first character behind the expression.
				 * If 'lookup' is set to FALSE only the 'pptr' content will be
				 * modified but no name lookup is performed.  In this case or if not
				 * successful the symbol KEY_Unknown is returned.
				*/
#endif
#ifdef HTMLCSS
short   scan_css (const char ** pptr, size_t len);
				/* Same as scan_key() but for css variable KEY name expressions.
				 * 'len' is the maximum number of characters to evaluate.
				*/
#endif

BOOL scan_numeric (const char ** pptr, long * num, UWORD * unit);
				/* Scanns a floating point number and it unit.  The result will be
				 * stored in 'num' as a fixpoint number with the fractional part
				 * in the lower 8 bit.
				*/

void * scan_namedchar (const char ** pptr, void * dst,
                       BOOL wordNchar, WORD mapping);
				/* Scanner for named character expressions of the forms
				 *    &  |  &<expr>;  |  &#<dec_num>;  |  &#x<hex_num>;
				 * The 'pptr' content must point to the leading '&'.  If 'wordNchar'
				 * is TRUE the 'dst' and the return value are interpreted as
				 * pointers to a 16-bit character string encoded in BICS.  Else both
				 * are interpreted as pointers to a 8-bit ASCII string encoded in
				 * Atari System Font.
				 * If successful the 'pptr' content is set behind the expression and
				 * up to five characters (8 or 16 bit wide) are written to 'dst'.
				 * Else the 'pptr' content is only incremented and one single '&' is
				 * written.  In both cases the new 'dst' pointer is returned.
				*/

long scan_color (const char * text, size_t len);
				/* Scanner for color expressions of the forms
				 *    <name>  |  #<RGB hex value> | rgb(<r>,<g>,<b>)
				 * 'len' ist the expression's length in 'text'.
				 * If successful the value will be returned as a 24-bit RRGGBB long,
				 * else -1 will be returned.
				*/

void scan_string_to_16bit (const char *symbol, const ENCODING,
                           WCHAR **pwstring, WORD mapping);
				/* scan_string_to_16bit() is used to read a string parameter of a
				 * HTML key.  It merges white spaces and converts named entities.
				 */

ENCODING scan_encoding(const char *p, ENCODING encoding);
	/* Scanner for character set resp. encoding names.
	 * The parameter 'encoding' is a default return value.  If you need to know
	 * about unrecognized encoding, use ENCODING_Unknown.  Else use a predefined
	 * value or ENCODING_WINDOWS1252.
	 */


/*******************************************************************************
 *
 * Charset encoding filters in <encoding.c>
 *
*/

typedef WCHAR * (*ENCODER_W)(const char ** src, WCHAR * dst);
ENCODER_W encoder_word (ENCODING, WORD mapping);
							/* encoding to 16-bit character string */
typedef char  * (*ENCODER_C)(const char ** src, char  * dst);
ENCODER_C encoder_char (ENCODING);
							/* encoding to 8-bit character Atari System Font string */
				/* Select encoder function according to ENCODING to read one
				 * (possible multibyte) character from 'src' and write 0 to 4
				 * characters (either 8 or 16 bit wide) to 'dst'.  The 'src'
				 * content will be set behind the read character and the address
				 * behind the written character.
				 * (Main code by Rainer Seitel, 2002-03)
				*/

WCHAR * unicode_to_wchar (WCHAR uni, WCHAR * dst, WORD mapping);
char  * unicode_to_8bit  (WCHAR uni, char  * dst);


#endif /*__SCANNER_H__*/
