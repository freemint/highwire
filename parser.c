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
#include "cache.h"
#include "parser.h"
#include "Containr.h"
#include "Location.h"
#include "fontbase.h"


#ifdef LATTICE   /* get rid of compiler bitfield bug */
# define BF16(t,n)   UWORD n
#else
# define BF16(t,n)   t     n :16
#endif

typedef struct { /* array to store KEY=VALUE pairs found while a parse() call */
	BF16(HTMLKEY,  Key);
	BF16(unsigned, Len);
	const char   * Value;
	LONG           Weight;
} KEYVALUE;

typedef struct s_style * STYLE;

typedef struct s_parser_priv {
	struct s_stack {
		const char * Ptr;
		LOCATION     Base;
		}     Stack[5];
	struct s_own_mem {
		struct s_own_mem * Next;
		char             * Mem;
	}        OwnMem;
	struct s_style {
		STYLE    Next;
		STYLE    Link;
		KEYVALUE Css;
		char     ClassId; /* '.'Class '#'Id */
		char     Ident[1];
	}      * Styles;
	KEYVALUE KeyValTab[50];
	UWORD    KeyNum;
	WCHAR    Buffer[505]; /* 500 is enough for 124 UTF-8 characters, */
} * PARSPRIV;            /* if the encoding is unrecognized, parsed */
                         /* as single byte characters in PRE mode.  */
#define ParserPriv(p) ((PARSPRIV)(p +1))

#define keyval_disposable(prs)   (prs->KeyNum < numberof(prs->KeyValTab))


/*============================================================================*/
PARSER
new_parser (LOADER loader)
{
	PARSER parser = malloc (sizeof (struct s_parser) +
	                        sizeof (struct s_parser_priv));
	PARSPRIV prsdata = ParserPriv(parser);
	TEXTBUFF current = &parser->Current;
	parser->Loader   = loader;
	parser->Target   = loader->Target;
	parser->hasStyle = FALSE;
	parser->ResumePtr = loader->Data;
	parser->ResumeFnc = NULL;
	parser->ResumeErr = E_OK;
	memset (prsdata->Stack, 0, sizeof(prsdata->Stack));
	prsdata->Styles = NULL;
	prsdata->KeyNum = 0;
	prsdata->OwnMem.Next = NULL;
	prsdata->OwnMem.Mem  = NULL;
	
	memset (current, 0, sizeof (parser->Current));
	current->font     = fontstack_setup (&current->fnt_stack, -1);
	current->text     = current->buffer = prsdata->Buffer;
	parser->Watermark = current->buffer + numberof(prsdata->Buffer) -6;
	
	parser->Frame  = new_frame (loader->Location, current,
	                            loader->Encoding, loader->MimeType,
	                            loader->MarginW, loader->MarginH);
	
	if (loader->ScrollV > 0) {
		parser->Frame->v_bar.on     = TRUE;
		parser->Frame->v_bar.scroll = loader->ScrollV;
		parser->Frame->Page.Rect.H  = parser->Frame->clip.g_h +1024;
	}
	if (loader->ScrollH > 0) {
		parser->Frame->h_bar.on     = TRUE;
		parser->Frame->h_bar.scroll = loader->ScrollH;
		parser->Frame->Page.Rect.W  = parser->Frame->clip.g_w +1024;
	}
	if (loader->AuthRealm) {
		parser->Frame->AuthRealm = loader->AuthRealm; loader->AuthRealm = NULL;
		parser->Frame->AuthBasic = loader->AuthBasic; loader->AuthBasic = NULL;
	}
	parser->Frame->Container = parser->Target;
	
	containr_clear (parser->Target);
	if (!loader->notified) {
		loader->notified = containr_notify (loader->Target, HW_PageStarted,
		                                    loader->Location);
	}
	
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
		fontstack_clear (&parser->Current.fnt_stack);
		delete_frame (&frame);
		containr_calculate (cont, NULL);
	}

	delete_loader (&parser->Loader);
	
	if (ParserPriv(parser)->OwnMem.Mem) {
		struct s_own_mem * own = &ParserPriv(parser)->OwnMem;
		do {
			struct s_own_mem * next = own->Next;
			free (own->Mem);
			if (next) {
				own->Next = next->Next;
				own->Mem  = next->Mem;
				free (next);
			} else {
				break;
			}
		} while (own->Mem);
	}
	if (ParserPriv(parser)->Styles) {
		STYLE style = ParserPriv(parser)->Styles, next;
		do {
			while (style->Link) {
				STYLE link = style->Link;
				style->Link = link->Link;
				free (link);
			}
			next = style->Next;
			free (style);
		} while ((style = next) != NULL);
	}
	free (parser);
}


/*----------------------------------------------------------------------------*/
static BOOL
stack_store (PARSPRIV prsdata, char * ptr)
{
	struct s_own_mem * own;
	if (!prsdata->OwnMem.Mem) {
		own = &prsdata->OwnMem;
	} else if ((own = malloc (sizeof(struct s_own_mem))) != NULL) {
		own->Mem  = prsdata->OwnMem.Mem;
		own->Next = prsdata->OwnMem.Next;
		prsdata->OwnMem.Next = own;
	} else {
		return FALSE;
	}
	prsdata->OwnMem.Mem = ptr;

	return TRUE;
}

/*----------------------------------------------------------------------------*/
static BOOL
stack_push (PARSPRIV prsdata, LOCATION loc, const char * ptr)
{
	short i = (short)numberof(prsdata->Stack) -1;
	if (prsdata->Stack[i].Ptr || prsdata->Stack[i].Base) {
		puts ("CSS stack_push(): overflow");
		return FALSE;
	}
	
	do {
		prsdata->Stack[i] = prsdata->Stack[i -1];
	} while (--i);
	prsdata->Stack[0].Ptr  = ptr;
	prsdata->Stack[0].Base = location_share (loc);
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static BOOL
stack_pop (PARSPRIV prsdata, LOCATION * loc, const char ** ptr)
{
	short i;
	if (!prsdata->Stack[0].Ptr && !prsdata->Stack[0].Base) {
		puts ("CSS stack_pop(): underflow");
		if (loc) *loc = NULL;
		if (ptr) *ptr = NULL;
		return FALSE;
	}
	
	if (ptr) *ptr = prsdata->Stack[0].Ptr;
	if (loc) *loc = prsdata->Stack[0].Base;
	else     free_location (&prsdata->Stack[0].Base);
	for (i = 0; i < numberof(prsdata->Stack) -1; i++) {
		prsdata->Stack[i] = prsdata->Stack[i +1];
	}
	prsdata->Stack[numberof(prsdata->Stack)-1].Ptr  = NULL;
	prsdata->Stack[numberof(prsdata->Stack)-1].Base = NULL;

	return TRUE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
resume_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	if (!invalidated) {
		PARSER parser = loader->FreeArg;
		if (loader->Error) {
			char buf[1024];
			location_FullName (loader->Location, buf, sizeof(buf));
			printf ("not found: '%s'\n", buf);
			parser->ResumeErr = loader->Error;
		} else {
			PARSPRIV prsdata = ParserPriv(parser);
			char * data = loader->Data;
			if (!data && loader->Cached) {
				size_t size = 0;
				data = load_file (loader->Cached, &size, &size);
			} else {
				loader->Data = NULL;
			}
			if (!stack_push (prsdata, loader->Location, data)) {
				free (data);
			} else if (!stack_store (prsdata, data)) {
				free (data);
				stack_pop (prsdata, NULL, NULL);
			}
			parser->ResumeErr = E_OK;
		}
	}
	delete_loader (&loader);

	return FALSE;
}

/*============================================================================*/
int
parser_resume (PARSER parser, void * func, const char * ptr_sub)
{
	parser->ResumeFnc = func;
	parser->ResumePtr = ptr_sub;
	if (!func && !ptr_sub) {
		if (parser->ResumeErr == 2/*EBUSY*/) {
			puts ("parser_resume(): busy");
		}
		parser->ResumeErr = E_OK;
	}

	return -2; /*JOB_NOOP */
}


/*----------------------------------------------------------------------------*/
static KEYVALUE *
find_key (PARSER parser, HTMLKEY key)
{
	PARSPRIV prsdata   = ParserPriv(parser);
	UWORD    num 	   = 0; 
	UWORD	 num_max = prsdata->KeyNum;
	KEYVALUE * ent   = prsdata->KeyValTab;

	while (num++ < num_max) {
		if ((ent)->Key == key) {
			return ent;
		}
		ent++;
	}

	return NULL;
}

/*==============================================================================
 * Finds the VALUE of 'key' that was read while the first parse() call.
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
 * Returns the VALUE of 'key' that was read while the first parse() call as a
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
 * first parse() call.
 * If not successful a '\0' will be returned.
 */
char
get_value_char (PARSER parser, HTMLKEY key)
{
	KEYVALUE * ent = find_key (parser, key);
	return (ent && ent->Len ? ent->Value[0] : '\0');
}


/*==============================================================================
 * Returns the VALUE of 'key' that was read while the first parse() call as a
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
 * Returns the VALUE of 'key' that was read while the first parse() call as a
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
				size = (WORD)val;
			} else if (val < 99) {
				size = (WORD)(-((val *1024 +50) /100));
			} else {
				size = -1024;
			}
		}
	}

	return size;
}


/*==============================================================================
 * Returns the VDI color VALUE of 'key' that was read while the first parse()
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


/*----------------------------------------------------------------------------*/
static KEYVALUE *
css_values (PARSER parser, const char * line, size_t len, LONG weight)
{
	PARSPRIV   prsdata = ParserPriv(parser);
	KEYVALUE * entry   = prsdata->KeyValTab + prsdata->KeyNum;

	while (len) {
		const char * ptr = line;
		short        css = scan_css (&line, len);
		const char * val = (*line == ':' ? ++line : NULL);
		KEYVALUE   * ent = NULL;
		BOOL important = FALSE;

		len -= line - ptr;
		if (val) {
			while (len && isspace(*val)) {
				len--;
				val++;
			}
			line = val;
		}
		while (len && *line != ';') {
			/* ! important - we might need to check farther */
			if (*line == '!') {
				important = TRUE;	
			}
			len--;
			line++;
		}
		if (val) {
			ptr = line;
			while (--ptr >= val && isspace(*ptr));
			if (ptr < val) val = NULL;
		}

		if (val && (css != CSS_Unknown)) {
			if ((ent = find_key (parser, (HTMLKEY)css)) == NULL) {
				if (keyval_disposable (prsdata)) {
					ent = entry++;
					prsdata->KeyNum++;
					ent->Weight = 0;
				} else {
					printf("KeyValTab overflow\r\n");
					/*printf("val %.*s  ent %d \r\n",10,val,css);*/
				}
			} else {
#if 0
				if (css == 13) printf("found ent %d val %.*s %d  \r\n",
				                         css, ent->Len,ent->Value,ent->Weight);
#endif
			}
		}

		if (ent) {
/*			if (important && ent->Key == 13)
					printf("in   %.*s %ld vs %ld _______________\n",
					       (unsigned)(ptr - val +1),val,weight,ent->Weight);
*/
			if ((weight >= ent->Weight)
				||(important && ((weight + 10000000L) >= ent->Weight))) {
/*				if (ent->Key == 13)
					printf("new ent %d val %.*s %ld old %.*s %ld \r\n",
					       css,(unsigned)(ptr - val +1),val,weight,ent->Len,
					       ent->Value,ent->Weight);
*/						
				ent->Key   = css;
				ent->Value = val;
				ent->Len   = (unsigned)(ptr - val +1);
				ent->Weight = weight;
				if (important) {
					ent->Weight += 10000000L;
					important = FALSE;
/*					if ( ent->Key == 13)
						printf("\nin   %.*s %ld\n",ent->Len,ent->Value,ent->Weight);
*/					
				}
			}
		}

		if (len && *line == ';') {
			len--;
			line++;
		}
		while (len && isspace(*line)) {
			len--;
			line++;
		}
	}

	return entry;
}

/*----------------------------------------------------------------------------*/
static KEYVALUE *
css_filter (PARSER parser, HTMLTAG tag, char class_id, KEYVALUE * keyval)
{
	PARSPRIV   prsdata = ParserPriv(parser);
	KEYVALUE * entry   = prsdata->KeyValTab + prsdata->KeyNum;
	STYLE      style   = prsdata->Styles;
	LONG       weight  = 0;

	while (style) {
		BOOL match;

		if ((style->Css.Key && (HTMLTAG)style->Css.Key != tag
		                    && (HTMLTAG)style->Css.Key != TAG_LastDefined) ||
		    (class_id != style->ClassId)               ||
		    (keyval && (strncmp (style->Ident, keyval->Value, keyval->Len)
	                         || style->Ident[keyval->Len]))) {
			match = FALSE;
		} else {
			STYLE    link = style->Link;
			DOMBOX * box  = parser->Current.parentbox;

			if (style->Css.Key && (HTMLTAG)style->Css.Key == tag) {
				weight += 1;
			}
			if (class_id == style->ClassId) {
				if (style->ClassId == '.') {
					if (keyval &&
					    (strncmp(style->Ident, keyval->Value, keyval->Len) == 0)) {
						weight += 100;
						if (link && weight >= 100 && *link->Css.Value) {
							box = NULL;
						}
					}
				} else if (style->ClassId == '#') {
					if (keyval &&
					    (strncmp(style->Ident, keyval->Value, keyval->Len) == 0)) {
						weight += 10000;
						if (link && weight >= 10000 && *link->Css.Value) {
							box = NULL;
						}
					}
				}
			}
			
			while (link && box) {
				/* these  > * + get no weight */
				if (*link->Css.Value == '>') {
					/* exact: <parent><tag> */
				} else if (*link->Css.Value == '*') {
					/* exact: <parent><*><tag> */
					if ((box = box->Parent) == NULL) break;
				} else if (*link->Css.Value == '+') {
					/* exact: </sibling><tag> */
					if ((box = box->ChildBeg) == NULL) break;
					while (box->Sibling && box->Sibling->Sibling) box = box->Sibling;
				}
				if (link->ClassId == '.') {
					if (!box->ClName || strcmp (box->ClName, link->Ident)) {
						box = (!*link->Css.Value ? box->Parent : NULL);
						continue;
					} else {
						weight += 100;
					}
				} else if (link->ClassId == '#') {
					if (!box->IdName || strcmp (box->IdName, link->Ident)) {
						box = (!*link->Css.Value ? box->Parent : NULL);
						continue;
					} else {
						weight += 10000;
					}
				}
				if (link->Css.Key && link->Css.Key != box->HtmlCode) {
					if (box->real_parent
					    && (link->Css.Key == box->real_parent->HtmlCode)) {
						link = link->Link;
						box = box->Parent;
						weight += 1;

						/* temporary fix for missing a DOMBOX for HTML tag */
						if (link && (link->Css.Key && (HTMLTAG)link->Css.Key == TAG_HTML)
						         && (box == NULL)) {
							weight += 1;
							link = link->Link;
						}
					} else if (link) {
						/* This next line was problematic as it was aborting some
						 * valid cases before they could hit when '>' was used
						 * It's possible we will need to make a special case
						 * for it - Dan Feb 9, 2006
						 */
						box = (!*link->Css.Value ? box->Parent : NULL);
						/* box = box->Parent; */
						continue;
					}
				} else {
					link = link->Link;
					box  = box->Parent;
					weight += 1;
					/* temporary fix for missing a DOMBOX for HTML tag */
					if (link && ((HTMLTAG)link->Css.Key == TAG_HTML) && (box == NULL)) {
						weight += 1;
						link = link->Link;
					}
				}
			}
			match = (link == NULL);
		}

		if (match) {
			parser->hasStyle = TRUE;
			entry = css_values (parser, style->Css.Value, style->Css.Len, weight);
		}

		weight = 0;
		style = style->Next;
	}

	return entry;
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
	const char * line = *pptr;
	PARSPRIV     prsdata;
	KEYVALUE   * entry;
	HTMLTAG      tag;
	BOOL         lookup;
	
	if (parser) {
		prsdata = ParserPriv(parser);
		parser->hasStyle = FALSE;
		prsdata->KeyNum  = 0;
		entry   = prsdata->KeyValTab;
		lookup  = TRUE;
	} else {
		prsdata = NULL;
		entry   = NULL;
		lookup  = FALSE;
	}
	
	/* first check for comment */
	if (*line == '!') {
		const char * end;
		if (*(++line) == '-') {
			if      ((end = strstr (++line, "-->")) != NULL) end += 3;
			else if ((end = strstr (line,    "->")) != NULL) end += 2;
		} else {
			end = NULL;
		}
		if (!end) {
			if ((end = strchr (line, '>')) != NULL) end += 1;
		}
		*pptr = (end ? end : strchr (line, '\0'));

		return TAG_Unknown;
	}

	if ((tag = scan_tag (&line)) == TAG_Unknown) {
		lookup = FALSE;
	} else if (lookup && prsdata->Styles) {
		entry = css_filter (parser, tag, '\0', NULL);
	}

	/*** if the tag is known or not, in every case we have to go through
	 *   the list of variables to avoid the parser from becoming confused
	 */
	while (isspace(*line)) line++;
	while (*line  &&  *line != '>') {
		const char  * val = line;
		const char  * line2 = line;
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
			line++;
			
			if ((*val == 39)||(*val == '"')) {
				while ((*line != *val)&&(*line != '>')) ++line;
				if (*line == '>') {
					line2 = line;
					
					while ((*line != *val)&&(*line != '<')) ++line;

					if (*line == '<')	line = line2;
				}
				delim = *val;
				val++;
			} else {
				while (*line && *line != '>' && !isspace(*line)) line++;
			}
			if (*line == '\0') delim = '\0';
		} else {
			val = NULL;
		}
		if (lookup) {
			unsigned len = (unsigned)(val ? line - val : 0ul);
			
			while (len && isspace (*val)) {
				val++;
				len--;
			}
			while (len && isspace (val[len -1])) {
				len--;
			}
			if (key == KEY_STYLE) {
				if (val && len) {
					parser->hasStyle = TRUE;
					entry = css_values (parser, val, len,1000000L);
				}
			} else if (keyval_disposable (prsdata)) {
				int temp_count = 1;
				unsigned tlen = 0, tlen1 = 0;

				/* 1st Count classes in val */
				/* This could probably all be rewritten */
				if ((val && len) && (key == KEY_CLASS)) {
					while(tlen < len) {
						if (isspace(val[tlen])) {
							temp_count++;
						}
						tlen++;
					}
				}
				
				/* we have multiple CLASS */
				if (temp_count > 1) {
					const char  * tempval = val;
					tlen1 = 0;

					/* first send the whole thing */
					entry->Key = key;
					entry->Value = val;
					entry->Len   = len;
					entry++;
					prsdata->KeyNum++;
					if (val && len && prsdata->Styles) {
						entry = css_filter (parser, tag, '.', entry -1);
					}

					/* now send it in parts - multiple classes
					 * a pain and still not done 100% */
					while (temp_count > 0 && keyval_disposable (prsdata)) {
						while((!isspace(tempval[tlen1])) && (tlen1 < tlen)) {
							tlen1++;
						}
						entry->Key = key;
						entry->Value = tempval;
						entry->Len   = tlen1;
						entry++;
						prsdata->KeyNum++;

						/* only handling classes at the moment */
						if (val && len && prsdata->Styles) {
							entry = css_filter (parser, tag, '.', entry -1);
						}
						
						tempval += tlen1;
						tempval += 1; /* space */
						tlen = tlen - tlen1;
						tlen -= 1; /* space */
						tlen1 = 0;
						temp_count -= 1;
					}
				} else {
					entry->Key = key;
					if (val && len) {
						entry->Value = val;
						entry->Len   = len;
					} else {
						entry->Value = NULL;
						entry->Len   = 0;
					}
					entry++;
					prsdata->KeyNum++;
					if (val && len && prsdata->Styles) {
						if (key == KEY_CLASS) {
							entry = css_filter (parser, tag, '.', entry -1);
						} else if (key == KEY_ID) {
							entry = css_filter (parser, tag, '#', entry -1);
						}
					}
				}
			}
		}

		if (delim && delim == *line) line++;
		while (isspace(*line)) line++;
	}
	*pptr = (*line ? ++line : line);
	
	return tag;
}


/*----------------------------------------------------------------------------*/
static const char *
css_import (PARSER parser, const char * ptr, LOCATION * base)
{
	PARSPRIV prsdata = ParserPriv(parser);
	const char * ret;
	const char * p;
	LOCATION     loc;
	
	if (!ptr) {
		p   = "";
		loc = location_share (*base);
	} else {
		p   = ptr;
		loc = NULL;
		while (isspace (*p)) p++;
		if (strnicmp (p, "url", 3) == 0) {
			p += 3;
			while (isspace (*p)) p++;
			if (*p != '(') {
				p = NULL;
			} else {
				while (isspace (*(++p)));
			}
		}
		if (!p) {
			return ptr;
			
		} else {
			const char * e = (*p == '"'  ? strchr (++p, '"')  :
			                  *p == '\'' ? strchr (++p, '\'') : strchr (p, ')'));
			if (e && e > p) {
				char   buf[1024];
				size_t len = min (e - p, sizeof(buf) -1);
				((char*)memcpy (buf, p, len))[len] = '\0';
				loc = new_location (buf, *base);
			}
			p = (e ? strchr (++e, ';') : e);
			p = (p ? p + 1 : e ? e : strchr (ptr, '\0'));
		}
	}
	ret = p;
	
	if (!loc) {
		/* invalid syntax, skip it */
	} else {
		size_t size = 0;
		char * file = NULL;
		if (PROTO_isLocal (loc->Proto)) {
			file = load_file (loc, &size, &size);
		} else {
			struct s_cache_info info;
			CRESULT res = cache_query (loc, 0, &info);
			if (res & CR_LOCAL) {
				file = load_file (info.Local, &size, &size);
			} else if (res & CR_BUSY) {
				ret = NULL;
			} else if (!ptr || stack_push (prsdata, *base, p)) {
				start_objc_load (parser->Target, NULL, loc, resume_job, parser);
				parser->ResumeErr = 2/*EBUSY*/;
				ret = NULL;
			}
		}
		if (file) {
			if (size > 0) {
				if (!ptr || stack_push (prsdata, *base, p)) {
					if (stack_store (prsdata, file)) {
						free_location (base);
						*base = location_share (loc);
						ret  =  file;
						file =  NULL;
					} else if (ptr) {
						stack_pop (prsdata, NULL, NULL);
					}
				}
			}
			if (file) free (file);
		}
		free_location (&loc);
	}

	return ret;
}

/*============================================================================*/
static char next (const char ** pp) {
	const char * p = *pp; while (isspace(*p)) p++; return *(*pp = p);
}
/*- - - - - - - - - - - - - - - - - - - - - - - -*/
const char *
parse_css (PARSER parser, LOCATION loc, const char * p)
{
	PARSPRIV prsdata = ParserPriv(parser);
	STYLE  * p_style = &prsdata->Styles;
	BOOL     err     = FALSE;
	
	if (parser->ResumeErr == 2/*EBUSY*/) {
		puts ("parse_css(): busy");
		return NULL;
	}
	if (loc) {
		loc = location_share (loc);
		if (!p && (p = css_import (parser, NULL, &loc)) == NULL) {
			free_location (&loc);
			return NULL;
		}
	
	} else { /* if (!p)  error case, continue with next in stack */
		if (!stack_pop (prsdata, &loc, &p)) {
			return parser->ResumePtr;
		}
		
		if (!loc || !p) {
/*			printf ("parse_css(): loc=%p p=%p\n", loc, p);*/
			if (!p) {
				static const char * empty = "";
				p = empty;
			}
		}
	}
	
#ifdef __PUREC__
# define isgraph(c)   (c > 0x20 && c < 0x7F)  /* seems to be broken in Pure C */
#endif
	if (*p && !isspace (*p) && !isgraph (*p)) {
		/* check for invalid characters (probably some UTF-8) */
		int n = 0;
		do {
			if (++n > 10) {
				n = 0; /* too much nonsense, skip check */
				break;
			}
		} while (p[n] && !isspace (p[n]) && !isgraph (p[n]));
		if (n && p[n])
		{
			/* todo : output should probably be sent to a logfile */
			p += n;
		}
	}
	
	while (*p_style) { /* jump to the end of previous stored style sets */
		p_style = &(*p_style)->Next;
	}
	
	do {
		const char * beg = NULL, * end = NULL, * tok = p;
		STYLE style = NULL;
		STYLE b_lnk = NULL;
		BOOL  skip  = FALSE;
		BOOL  done  = FALSE;
		BOOL  universal = FALSE;
		const char * unvsel = p;  /* offset to '*' */

		while (*p) {
			WORD key = TAG_Unknown;
			char cid = '\0';

			if (next(&p) == '/') { /*........................ comment */
				const char * q = p;

				if (*(++q) == '/') {        /* C++ style */
					q = strchr (q +1, '\n');
				} else if (*(q++) == '*') { /* C style */
					while (((q = strchr (q, '*')) != NULL) && *(++q) != '/');
				} else {
					q = NULL; /* syntax */
				}
				if ((err = (q == NULL)) == TRUE)	break;
				p = q;
				while (isspace (*(++p)));
				done = (!*p && !style);
				continue;
			}
			
			tok = p;
			if (*p == '@') { /*............................... special */
				const char * q = p;
				if (strnicmp (q +1, "import", 6) == 0) {
					if ((q = css_import (parser, q +7, &loc)) == NULL) {
						free_location (&loc);
						return NULL;
					}
				
				} else if (strnicmp (q +1, "media", 5) == 0) {
					BOOL parse_media = FALSE; /* if we have a screen  *
					                           * then parse css rules */
					/*  http://www.w3.org/TR/REC-CSS2/media.html
					 */
					q += 6;
					while (next(&q) != '{') {
						if (strnicmp (q, "screen", 6) == 0) {
							parse_media = TRUE;
							q += 6;
						} else if (strnicmp (q, "all", 3) == 0) {
							parse_media = TRUE;
							q += 3;

							/* They have added conditional rules so
							 * an author can declare this style is
							 * for all when the case is true
							 * ex. @media all and (min-width:0px) {
							 *
							 * I'm testing on slashdot for anyone interested
							 * This needs to be handled for all cases, so
							 * this is just a cheap hack to begin - Dan
							 */
							if (strnicmp (q, " and", 4) == 0) {
								q += 4;

								if (isspace(*q))
									while (isspace(*q)) q++;

								if (isalpha(*q))
									while (isalpha(*q)) q++;
									
								if (*q == '(') {
									while (*q != ')')
										q++;
									q++;
								}								
							}
						} else {
							while (isalpha(*q)) q++;
							if (next(&q) == ',') q++;
							if (!isspace(*q) && !isalpha(*q) && *q != '{') break;
						}
					}
					if ((err = (*q != '{')) == TRUE)
						break;
					
					if (parse_media) {
						/* get us past opening bracket */
						q++;
					} else {
						/* this is designed to walk a complete
						 * media type that we don't use, for example
						 * media print
						 */
						int bracket_count = 1;

						while (*(++q)) {
							if (*q == '{') {
								bracket_count += 1;
							} else if (*q == '}') {
								bracket_count -= 1;
								
								if (bracket_count < 1) {
									break;
								}
							}
						}
					}
				} else if (strnicmp (q +1, "font-face", 9) == 0) {
					/* The same as setting the font family
					 * for the whole document
					 * http://www.w3.org/TR/REC-CSS2/fonts.html#font-descriptions
					 */
					/* dan - just ignore them for the moment? */
					if ((q = strchr (q +10, '}')) != NULL) {
						q++;
					}
				} else if (strnicmp (q +1, "ie", 2) == 0) {
					/*  Some stupid ie directive?		 */
					/* dan - just ignore them for the moment? */
					if ((q = strchr (q +3, '}')) != NULL) {
						q++;
					}
				} else if (strnicmp (q +1, "page", 4) == 0) {
					/* A way to define the size, margins etc
					 * for the whole document
					 * http://www.w3.org/TR/REC-CSS2/page.html#page-box
					 */
					/* dan - just ignore them for the moment? */
					if ((q = strchr (q +5, '}')) != NULL) {
						q++;
					}
				} else if (strnicmp (q +1, "charset", 7) == 0) {
					/* A way to define the charset of the
					 * css style sheet
					 * http://www.w3.org/TR/CSS21/syndata.html#x60
					 */
					/* dan - just ignore them for the moment? */
					if ((q = strchr (q +8, '}')) != NULL) {
						q++;
					}
				} else {
					while (isalpha (*(++q)));
					while (isspace (*(++q)));
					q = (*q == '{' ? strchr (q +1, '}') : NULL);
					if (q) q++;
				}
				if ((err = (q == NULL)) == TRUE) break;
				p = q;
				continue;
			}

			/* we may have a trailing } left over from media parsing. */
			if (next(&p) == '}') {
				p++;
				done = (!*p && !style);
				continue;
			}

			/*ignore netscape css rules *|*: */
			if (strnicmp (p, "*|*:", 4) == 0) {
				int bracket_count = 0;

				while (*(++p)) {
					if (*p == '{') {
						bracket_count += 1;
					} else if (*p == '}') {
						bracket_count -= 1;
						if (bracket_count < 1) break;
					}
				}
				p++;
			}

			/*ignore escaped css idents and their rules */
			if (*p == '\\') {
				int bracket_count = 0;

				while (*(++p)) {
					if (*p == '{') {
						bracket_count += 1;
					} else if (*p == '}') {
						bracket_count -= 1;
						if (bracket_count < 1) break;
					}
				}
				p++;
			}

			/*................................ joker */
			if (*p == '*') {
				key = TAG_LastDefined; /* matches all */
				universal = TRUE;
				unvsel = p;
				p++;
#if 0
				/* old code to probably be deleted */
				if (*(++p) == '.') {
					key = TAG_LastDefined; /* matches all */
				} else if (isalpha (*p)) {
					key = TAG_LastDefined; /* matches all */
					/*key = scan_tag (&p);*/
					continue;
				} else if (isspace (*p)) {
					/* ignore IE6 nonsense */
					while (isspace (*(++p)));
					continue;
				} else if (*p == '{') {
					key = TAG_LastDefined; /* matches all */
					p--;
				} else {
					key = TAG_LastDefined; /* matches all */
					p--;
					/*err = TRUE;
					break;*/
				}
#endif
			} else if (isalpha (*p)) { /*........................ tag */
				const char * q = p;

				key = scan_tag (&p);
				/* This could be a tag we don't support or
				 * it could be a typo on a class or id
				 *
				 * for the moment we will ignore it, but if it's a
				 * typo on a class, we need to filter out more possible
				 * characters than would be caught in scan_tag()
				 */
				if ((key == TAG_Unknown) || (*p == '_')) {
					p = q;
					while (isalnum (*(++p)) || *p == '-' || *p == '_' || *p == '&');
				}
			}

			/*............. class or id */
			if (*p == '.' || *p == '#') {
				cid = *(p++);

				/* can't start with numbers */
				if (!isalpha (*p)) skip = TRUE;

				beg = p;
				while (isalnum (*(++p)) || strchr ("-_.&", *p));
				end = p;
				if (beg == end) cid = '\0';
			} else {
				beg = end = NULL;
			}

			/*........................ pseudo format */
			if (*p == ':') {
				p++;
				if (key == TAG_A
				    && strnicmp (p, "link", 4) == 0 && !isalpha (p[4])) {
					p += 4; /* just eat this */
				} else if ((strnicmp (p, "before", 6) == 0 && !isalpha (p[6])) ||
				           (strnicmp (p, "after",  5) == 0 && !isalpha (p[5]))) {
					p += (*p == 'b' ? 6 : 5); /* just eat this */
				} else {
					while (isalpha (*p) || *p == ':' || *p == '-') p++;
					skip = TRUE; /* ignore */
				}
			}

			/*..................... conditional rule */
			if (*p == '[') {
				while (*(++p) && *p != ']');
				if (*p) p++;
				skip = TRUE; /* ignore */
			}

			/* invalid token ignore the rule as per CSS2 */
			if (*p == '&') {
				int bracket_count = 0;

				p++;
				while (*(++p)) {
					if (*p == '{') {
						bracket_count += 1;
					} else if (*p == '}') {
						bracket_count -= 1;
						
						if (bracket_count < 1) break;
					}
				}
				p++;
				skip = TRUE;           /* ignore */
				done = (!*p && !style);
			}

			/* store */
			if (key > TAG_Unknown || cid) {
				size_t len = end - beg;
				STYLE  tmp = malloc (sizeof (struct s_style) + len);
				if (len) memcpy (tmp->Ident, beg, len);
				tmp->Ident[len] = '\0';
				tmp->ClassId    = cid;
				tmp->Css.Key    = key; /*(key==TAG_LastDefined ?TAG_Unknown :key);*/
				tmp->Css.Value  = NULL;
				tmp->Next    = NULL;
				if (!*p_style) {
					tmp->Link   = NULL;
					*p_style    = tmp;
				} else if (!style->Css.Value) {
					b_lnk       = style;
					tmp->Link   = NULL;
					style->Next = tmp;
				} else if (!b_lnk) {
					tmp->Link   = style;
					*p_style    = tmp;
				} else {
					tmp->Link   = b_lnk->Next;
					b_lnk->Next = tmp;
				}
				style = tmp;
			}
			
			if (universal) {
				/* universal selector caught up above */
				if (style) style->Css.Value = unvsel;
				universal = FALSE;	
				continue;
			}

			/*............. look one ahead for selector concatenation */
			if (isalpha (next(&p)) || *p == '.' || *p == '#') {
				if (style) style->Css.Value = "";
				continue;
			} else if (*p == '>' || *p == '+' || *p == '*') {
				if (style) style->Css.Value = p;
				p++;
				continue;
			} else if (*p == '~') { 
				/* ........ WTF  ???  I can't find this but Slashdot uses it, so */
				/* I'm treating it like it was a normal alpha char seems to work */
				if (style) style->Css.Value = "";
				p++;
				continue;
			} else if (*p == ':') { /* pseudo format, to be ignored */
				continue;
			}

			if (skip) {
				if (style) {
					while (style->Link) {
						STYLE link = style->Link;
						style->Link = link->Link;
						free (link);
					}
					free (style);
				}
				if (*p_style == style) {
					*p_style = NULL;
					style    = NULL;
				} else {
					b_lnk->Next = NULL;
					style       = b_lnk;
				}
				skip = FALSE;
			}
			if (next(&p) == '/') continue;
			else if (*p  == ',') p++;
			else                 break;
		}

		if (err || *p != '{') {
			err       = TRUE;
			p         = tok;
			beg = end = NULL;
		} else /* if (*p == '{') */ {
			int bracket_count = 1;

			while (isspace (*(++p)));
			beg = p;
			while (*p ) {
				if (*p == '{') { 
					bracket_count += 1;
					p++;
					continue;
				}
				if (*p == '\'' || *p == '"') { 
					char q = *p;
					
					/* old line for reference */
					/*while (*(++p) && *p != q && !(*p == '\\' && !*(++p)));*/
					
					/* does the following routine do everything it is supposed to?
					 * definately doesn't have the bug the old line had
					 */
					while (*(++p) && *p != q && *p != '\\');
					
					if (*p) p++;
					end = NULL;

					continue;
				}
				if (!isspace (*p)) {
					end = NULL;
				} else if (!end) {
					end = p;
				}
				if (*p == '}') { 
					bracket_count -= 1;
					if (bracket_count < 1)
						break;
				}
				p++;
			} /* end while (*p) */
			
			if (!end) {
				end = p;
			}
			if (end[-1] == ';') {
				end--; /* cut off trailing semicolon to detect empty rules */
			}
			if (*p) while (isspace (*(++p)));
		} /* end else if (*p == '{') */
		
		if (style) {
			style = *p_style;
			
			if (end > beg) {   /* setup all styles to the rule string */
				unsigned len = (unsigned)(end - beg);
				do {
					style->Css.Len   = len;
					style->Css.Value = beg;
					p_style = &style->Next;
				} while ((style = *p_style) != NULL);
			} else do {   /* no rule string, delete styles */
				while (style->Link) {
					STYLE link = style->Link;
					style->Link = link->Link;
					free (link);
				}
				*p_style = style->Next;
				free (style);
			} while ((style = *p_style) != NULL);
		}

		if (!*p || err) {
			if (err && *p != '-' && *p != '<') {
				int n  = 0, ln = 0, cn = 0;
				while (p[n]) {
					if (p[n] == '\n') {
						if (++ln == 2) {
							n--;
							break;
						} else {
							cn = 0;
						}
					} else {
						if (++cn == 79) {
							break;
						}
					}
					n++;
				}
				/* todo : output should probably be sent to a logfile */
				(void)done;
#if 0
				if (!done)
				{
					printf("parse_css(): stopped at\n%.*s\n", n, p);
				}
#endif
			}
			if (prsdata->Stack[0].Ptr) {
				free_location (&loc);
				err = FALSE;
				stack_pop (prsdata, &loc, &p);
			}
		}

		/* ignore erroneous trailing ';' */
		if (*p == ';') {
			p++;
		}
	} while (*p && !err);

	free_location (&loc);
	
	return p;
}
