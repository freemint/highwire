/* @(#)highwire/parser.c
 *
 * parser.c -- Parser functions for HTML expressions.
 *
 * AltF4 - Jan 14, 2002
 *
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef __PUREC__
# define CONTAINR struct s_containr *
#endif

#include "global.h"
#include "token.h"
#include "scanner.h"
#include "Loader.h"
#include "parser.h"
#include "Containr.h"

#include "Location.h"


#define TEXT_LENGTH 500 /* 500 is enough for 124 UTF-8 characters,
	                      * if the encoding is unrecognized, parsed
	                      * as single byte characters in PRE mode. */


typedef struct { /* array to store KEY=VALUE pairs found while a parse() call */
	HTMLKEY      Key :16;
	unsigned     Len :16;
	const char * Value;
} KEYVALUE;

#define ValueNum(p)              p->_priv[0]
#define ValueTab(p) (((KEYVALUE*)p->_priv) +1)
#define ValueMax(p) (sizeof(p->_priv) / sizeof(KEYVALUE) -1)


/*============================================================================*/
PARSER
new_parser (LOADER loader)
{
	PARSER parser = malloc (sizeof (struct s_parser) +
	                        sizeof (WCHAR) * TEXT_LENGTH);
	parser->Loader   = loader;
	parser->Target   = loader->Target;
	ValueNum(parser) = 0;
	
	memset (&parser->Current, 0, sizeof (parser->Current));
	parser->Current.text =
	parser->Current.buffer = parser->TextBuffer;
	parser->Watermark      = parser->TextBuffer + TEXT_LENGTH -1;
	
	parser->Frame  = new_frame (loader->Location, &parser->Current,
	                            loader->Encoding, loader->MimeType,
	                            loader->MarginW, loader->MarginH);
	if (loader->ScrollV > 0) {
		parser->Frame->v_bar.on     = TRUE;
		parser->Frame->v_bar.scroll = loader->ScrollV;
		parser->Frame->Page.Height  = parser->Frame->clip.g_h +1024;
	}
	if (loader->ScrollH > 0) {
		parser->Frame->h_bar.on     = TRUE;
		parser->Frame->h_bar.scroll = loader->ScrollH;
		parser->Frame->Page.Width   = parser->Frame->clip.g_w +1024;
	}
	parser->Frame->Container = parser->Target;
	
	containr_clear (parser->Target);
	if (!loader->notified) {
		char buf[1024];
		location_FullName (loader->Location, buf, sizeof(buf));
		loader->notified = containr_notify (loader->Target, HW_PageStarted, buf);
	}
	parser->Target->u.Frame = parser->Frame;
	
	return parser;
}

/*============================================================================*/
void
delete_parser (PARSER parser)
{
	CONTAINR cont   = parser->Target;
	FRAME    frame  = parser->Frame;
	
	if (!cont->Mode) {
		frame_finish (frame, parser, &parser->Current);
		containr_setup (cont, frame, frame->Location->Anchor);
		if (parser->Loader->notified) {
			containr_notify (cont, HW_PageFinished, &cont->Area);
			parser->Loader->notified = FALSE;
		}
	} else {
		delete_frame (&frame);
		containr_calculate (cont, NULL);
	}
	
	delete_loader (&parser->Loader);
	free (parser);
}


/*----------------------------------------------------------------------------*/
static KEYVALUE *
find_key (PARSER parser, HTMLKEY key)
{
	KEYVALUE * ent = ValueTab (parser);
	UWORD      num = ValueNum (parser);
	while (num) {
		if (ent->Key == key) {
			return ent;
		}
		ent++;
		num--;
	}
	return NULL;
}


/*==============================================================================
 * Finds the VALUE of 'key' that was read while the last parse() call.
 * If successful the VALUE will be copied to 'output' up to 'max_len' character
 * (including the trailing '\0') and a TRUE will be returned.
 * Else a FALSE will be returned.
 */
BOOL
get_value (PARSER parser, HTMLKEY key, char * output, const size_t max_len)
{
	KEYVALUE * ent = find_key (parser, key);
	BOOL     found = (ent != NULL);
	
	if (output) {
		if (!found) {
			output[0] = '\0';
		} else if (!ent->Len || ent->Len >= max_len) {
			found = FALSE;
			output[0] = '\0';
		} else {
			memcpy (output, ent->Value, ent->Len);
			output[ent->Len] = '\0';
		}
	}
	return found;
}


/*==============================================================================
 * Returns the VALUE of 'key' that was read while the last parse() call as a
 * malloc'ed zero terminated character string.
 * If not successful the result is a NULL pointer.
 */
char *
get_value_str (PARSER parser, HTMLKEY key)
{
	KEYVALUE * ent = find_key (parser, key);
	char * found;
	
	if (!ent) {
		found = NULL;
	
	} else {
		found = malloc (ent->Len +1);
		memcpy (found, ent->Value, ent->Len);
		found[ent->Len] = '\0';
	}
	return found;
}


/*==============================================================================
 * Returns the first character of the VALUE of 'key' that was read while the
 * last parse() call.
 * If not successful a '\0' will be returned.
 */
char
get_value_char (PARSER parser, HTMLKEY key)
{
	KEYVALUE * ent = find_key (parser, key);
	return (ent && ent->Len ? ent->Value[0] : '\0');
}


/*==============================================================================
 * Returns the VALUE of 'key' that was read while the last parse() call as a
 * unsigned short.
 * If not successful the value of 'dflt' will be returned instead, which may
 * also be negative.
 */
WORD
get_value_unum (PARSER parser, HTMLKEY key, WORD dflt)
{
	KEYVALUE * ent = find_key (parser, key);
	
	if (ent && ent->Len) {
		char * tail;
		long   value = strtol (ent->Value, &tail, 10);
		if (value >= 0 && value <= 0x7FFF && tail != ent->Value) {
			dflt = (WORD)value;
		}
	}
	return dflt;
}


/*==============================================================================
 * Returns the VALUE of 'key' that was read while the last parse() call as a
 * signed short.
 * On success the return value is either a positive absolute number or a
 * negative fractional of -1024 if a trailing '%' was found.
 * If not successful a zero will be returned.
 */
WORD
get_value_size (PARSER parser, HTMLKEY key)
{
	KEYVALUE * ent = find_key (parser, key);
	WORD      size = 0;
	
	if (ent && ent->Len) {
		char * tail;
		long   val = strtol (ent->Value, &tail, 10);
		if (val > 0 && val < 10000 && tail != ent->Value) {
			if (*tail != '%') {
				size = val;
			} else if (val < 99) {
				size = -((val *1024 +50) /100);
			} else {
				size = -1024;
			}
		}
	}
	return size;
}


/*==============================================================================
 * Returns the VDI color VALUE of 'key' that was read while the last parse()
 * call.
 * If not successful a negative number will be returned.
 */
WORD
get_value_color (PARSER parser, HTMLKEY key)
{
	KEYVALUE * ent = find_key (parser, key);
	WORD     color = -1;

	if (ent && ent->Len) {
		long value = scan_color (ent->Value, ent->Len);
		if (value >= 0) {
			color = remap_color (value);
		}
	}
	return color;
}


/*==============================================================================
 * Parses a html TAG expression of the forms
 *    <TAG>  |  <TAG KEY ...>  |  <TAG KEY=VALUE ...>
 * The 'pptr' content must point to the first character after the leading '<'
 * and a possibly '/'.  After processing it is set to first character behind the
 * expression, either behind the closing '>' or to a trailing '\0'.
 * If successful the found known KEYs are stored with their VALUEs internally
 * and a TAG enum is returned.
 * Else the symbol TAG_Unknown is returned.
 * 
 * also note that val_num is the number of total keys for this tag present
 */
HTMLTAG
parse_tag (PARSER parser, const char ** pptr)
{
	const char * line  = *pptr;
	KEYVALUE   * style = NULL;
	KEYVALUE   * entry;
	HTMLTAG      tag;
	BOOL         lookup;
	
	if (parser) {
		lookup = TRUE;
		entry  = ValueTab(parser);
		ValueNum(parser) = 0;
	} else {
		lookup = FALSE;
		entry  = NULL;
	}
	
	/* first check for comment
	 */
	if (*line == '!') {
		line++;
		if (line[0] == '-' && line[1] == '-') {
			const char * end = strstr (line + 2, "--");

			if (end)
				line = end;
		}
		while (*line && *(line++) != '>')
			;
		*pptr = line;

		return TAG_Unknown;
	}

	if ((tag = scan_tag (&line)) == TAG_H) {
		if (*line < '1' || *line > '6') {
			tag = TAG_Unknown;
		} else if (entry) {
			entry->Key   = KEY_H_HEIGHT;
			entry->Value = line++;
			entry->Len   = 1;
			entry++;
			ValueNum(parser) = 1;
		}
	}
	lookup &= (tag != TAG_Unknown);

	/*** if the tag is known or not, in every case we have to go through the list
	 *   of variabls to avoid the parser to become confused   */

	while (isspace(*line)) line++;

	while (*line  &&  *line != '>') {
		const char  * val = line;
		HTMLKEY key   = scan_key (&line, lookup);
		BOOL    rhs   = (val == line);
		char    delim = '\0';

		while (isspace(*line)) line++;
		if (*line == '=') {
			while (isspace(*(++line)));
			rhs = TRUE;
		}
		if (rhs) {
			val = line;
			if      (*val == 39)  line = strchr (++val, (delim = 39));
			else if (*val == '"') line = strchr (++val, (delim = '"'));
			else                  line = strpbrk (val, " >\t\r\n");
			if (!line) line = strchr (val, (delim = '\0'));
		} else {
			val = NULL;
		}
		if (lookup  &&  ValueNum(parser) < ValueMax(parser)) {
			entry->Key = key;
			if (val  &&  val < line) {
				entry->Value = val;
				entry->Len   = (unsigned)(line - val);
			} else {
				entry->Value = NULL;
				entry->Len   = 0;
			}
			if (key == KEY_STYLE) {
				style = entry;
			}
			entry++;
			ValueNum(parser)++;
		}
		if (delim && delim == *line) line++;
		while (isspace(*line)) line++;
	}

	*pptr = (*line ? ++line : line);
	
	if (style) {
		size_t len = style->Len;
		line       = style->Value;
		while (len) {
			const char * ptr = line;
			short        css = scan_css (&line, len);
			const char * val = (*line == ':' ? ++line : NULL);
			KEYVALUE   * ent = NULL;
			len -= line - ptr;
			
			if (val) {
				while (len && isspace(*val)) {
					len--;
					val++;
				}
				line = val;
			}
			while (len && *line != ';') {
				len--;
				line++;
			}
			if (((css < CSS_Unknown && (ent = find_key (parser, css)) == NULL) ||
			      css != CSS_Unknown) && (ValueNum(parser) < ValueMax(parser))) {
				ent = entry++;
				ValueNum(parser)++;
			}
			if (ent) {
				ent->Key = css;
				if (val  &&  val < line) {
					ent->Value = val;
					ent->Len   = (unsigned)(line - val);
				} else {
					ent->Value = NULL;
					ent->Len   = 0;
				}
			}
			if (*line == ';') {
				len--;
				line++;
			}
			while (len && isspace(*line)) {
				len--;
				line++;
			}
		}
	}

	return tag;
}
