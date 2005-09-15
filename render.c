/* @(#)highwire/render.c
 */
#if defined (__PUREC__)
# include <tos.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h> 
#include <setjmp.h> 

#include "token.h" /* must be included before gem/gemx.h */
#include <gemx.h>

#include "global.h"
#include "scanner.h"
#include "Loader.h"
#include "parser.h"
#include "Containr.h"
#include "Location.h"
#include "Logging.h"
#include "Table.h"
#include "Form.h"
#include "fontbase.h"
#include "http.h"
#include "cache.h"


/* parser function flags */
#define PF_NONE   0x0000
#define PF_START  0x0001 /* no slash in tag   */
#define PF_PRE    0x0002 /* preformatted text */
#define PF_SPACE  0x0004 /* ignore spaces     */
#define PF_FONT   0x0008 /* font has changed  */
#define PF_ENCDNG 0x0010 /* charset encoding has changed   */
#define PF_SCRIPT 0x0020 /* <noscrip> inside a script area */

static jmp_buf resume_jbuf; /* used for parser interuption */


/*----------------------------------------------------------------------------*/
static WORD
list_indent (WORD type)
{
	switch (type)
	{
		case 0:
			return (font_size * 2);
		case 1:
			return (font_size * 3);
		case 2:
			return (font_size * 4);
	}
	return (0);
}


/*----------------------------------------------------------------------------*/
static H_ALIGN
get_h_align (PARSER parser, H_ALIGN dflt)
{
	char out[10];
	if (get_value (parser, KEY_ALIGN, out, sizeof(out))) {
		if      (stricmp (out, "left")    == 0) dflt = ALN_LEFT;
		else if (stricmp (out, "right")   == 0) dflt = ALN_RIGHT;
		else if (stricmp (out, "center")  == 0) dflt = ALN_CENTER;
		else if (stricmp (out, "justify") == 0) dflt = ALN_JUSTIFY;
	}
	return dflt;
}

/*----------------------------------------------------------------------------*/
static V_ALIGN
get_v_align (PARSER parser, V_ALIGN dflt)
{
	char out[10];
	if (get_value (parser, KEY_VALIGN, out, sizeof(out))) {
		if      (stricmp (out, "top")      == 0)  dflt = ALN_TOP;
		else if (stricmp (out, "middle")   == 0)  dflt = ALN_MIDDLE;
		else if (stricmp (out, "bottom")   == 0)  dflt = ALN_BOTTOM;
	/*	else if (stricmp (out, "baseline") == 0) dflt = ALN_BASELINE;*/
	}
	return dflt;
}


/*----------------------------------------------------------------------------*/
static short
font_face (char * buf, short dflt)
{
	char * beg = buf, * nxt = NULL;
	short  fnt = dflt;
	do {
		char dlm = *beg;
		if (dlm == '\'' || dlm == '"') {
			while (isspace(*(++beg)));
			if ((nxt = strchr (beg, dlm)) != NULL) {
				char * end = nxt++;
				while (isspace(*(--end)));
				end[1] = '\0';
			}
		} else if ((nxt = strchr (beg, ',')) != NULL) {
			char * end = nxt++;
			while (isspace(*(--end)));
			end[1] = '\0';
		}
		if (stricmp (beg, "sans-serif") == 0 ||
		    stricmp (beg, "helvetica")  == 0 ||
		    stricmp (beg, "verdana")    == 0 ||
		    stricmp (beg, "arial")      == 0) {
			fnt = header_font;
			break;
		} else if (stricmp  (beg, "serif")    == 0 ||
		           strnicmp (beg, "times", 5) == 0) {
			fnt = normal_font;
			break;
		} else if (stricmp  (beg, "monospace")  == 0 ||
		           strnicmp (beg, "courier", 7) == 0) {
			fnt = pre_font;
			break;
		} else if ((beg = nxt) == NULL) {
			break;
		}
		while (isspace(*beg)) beg++;
	} while (*beg);
	
	return fnt;
}


/*----------------------------------------------------------------------------*/
static short
numerical (const char * buf, char ** tail, short em, short ex)
{
	UWORD  unit;
	long   size;
	if (!scan_numeric (&buf, &size, &unit)) {
		size = -1;
	
	} else switch (unit) {
		case 0x4558: /* EX */
			size *= ex;
			goto case_PT; /* approximated */
		
		case 0x434D: /* MM */
			size *= 10;
		case 0x4D4D: /* CM */
			size *= 10;
			size = (size +127) /254;
		case 0x494E: /* IN, inch */
			size *= 6;
		case 0x5043: /* PC, pica */
			size *= 12;
		case 0x5058: /* PX, pixel */
			/* assume 72 dpi */
			goto case_PT;
		
		case '%':
			size = (size +50) /100;
		case 0x454D: /* EM */
			size *= em;
		case_PT:
		case 0x5054: /* PT, point */
			size = (size + 128) >> 8;
			break;
		default:
			size = 0;
	}
	if (tail) {
		union { const char * c; char * v; } ptr;
		ptr.c = buf;
		*tail = ptr.v;
	}
	return (short)size;
}

/*----------------------------------------------------------------------------*/
static FNTSTACK
css_text_styles (PARSER parser, FNTSTACK fstk)
{
	TEXTBUFF current = &parser->Current;
	short    step    = -1;
	char output[100];
	
	if (get_value (parser, CSS_FONT, output, sizeof(output))) {
		WORD comma = 0;
		char *   p = output;
		if (!fstk) {
			fstk = fontstack_push (current, -1);
		}
		while (*p) {
			long color = -1;
			
			if (isdigit (*p)) {
				char * tail = p;
				short size = numerical (p, &tail, current->font->Size,
			                           current->word->font->SpaceWidth);
				if (size > 0) {
					fontstack_setSize (current, size);
					p = tail;
				} else {
					break;
				}
			
			} else if (*p == '#') {
				short len = 0;
				while (isxdigit (p[++len]));
				if ((color = scan_color (p, len)) >= 0) {
					p += len;
				} else {
					break;
				}
			} else if (isalpha (*p) || *p =='\'') {
				char * tail;
				short  len = 0;
				if (*p =='\'') {
					tail = strchr (++p, '\'');
					len  = (short)((tail ? tail++ : strchr (p, '\0')) -p);
				} else {
					while (isalpha (p[++len]) || p[len] == '-');
					tail = p + len;
				}
				if ((color = scan_color (p, len)) < 0) {
					WORD fstep = -1, ftype = -1;
					
					/* font-style */
					if (strnicmp (p, "italic",  len) == 0 ||
					    strnicmp (p, "oblique", len) == 0) {
						fontstack_setItalic (current);
						/* n/i: normal */
					
					/* font-weight */
					} else if (strnicmp (p, "bold",   len) == 0 ||
					           strnicmp (p, "bolder", len) == 0) {
						fontstack_setBold (current);
						/* n/i: lighter, medium */
					
					/* font-decoration */
					} else if (strnicmp (p, "line-through", len) == 0) {
						fontstack_setStrike (current);
					} else if (strnicmp (p, "underline",    len) == 0) {
						fontstack_setUndrln (current);
						/* n/i: overline */
					
					/* font-size */
					} else if (strnicmp (p, "xx-small", len) == 0) {
						step = 0;
					} else if (strnicmp (p, "x-small",  len) == 0) {
						step = 1;
					} else if (strnicmp (p, "small",    len) == 0) {
						step = 2;
					} else if (strnicmp (p, "medium",   len) == 0) {
						step = 3;
					} else if (strnicmp (p, "large",    len) == 0) {
						step = 4;
					} else if (strnicmp (p, "x-large",  len) == 0) {
						step = 5;
					} else if (strnicmp (p, "xx-large", len) == 0) {
						step = 6;
						/* n/i: smaller, larger */
					
					/* font-family */
					} else if (strnicmp (p, "sans-serif", len) == 0 ||
					           strnicmp (p, "helvetica",  len) == 0 ||
					           strnicmp (p, "verdana",    len) == 0 ||
					           strnicmp (p, "arial",      len) == 0) {
						ftype = (comma > 0 ? -1 : header_font);
						comma = +1;
					} else if (strnicmp (p, "serif",      len) == 0 ||
					           strnicmp (p, "times",      len) == 0) {
						ftype = (comma > 0 ? -1 : normal_font);
						comma = +1;
					} else if (strnicmp (p, "monospace",  len) == 0 ||
					           strnicmp (p, "courier",    len) == 0) {
						ftype = (comma > 0 ? -1 : pre_font);
						comma = +1;
					} else {
						/* other family, not yet supported */
						if (!comma) comma = -1;
					}
					
					/* n/i: font-variant */
					
					if (fstep >= 0 && fstk) {
						fstk->Size = font_step2size (fstk->Step = fstep);
						word_set_point (current, fstk->Size);
					}
					if (ftype >= 0 && ftype != TAgetFont (current->word->attr)) {
						fontstack_setType (current, ftype);
					}
					if (*tail != ',') {
						p = tail;
						while (isspace (*p)) p++;
						if (*p == ',') tail = p;
					}
				}
				p = tail;
			}
			if (color >= 0 && !ignore_colours) {
				fontstack_setColor (current, remap_color (color));
			}
			if (isspace (*p)) {
				comma = 0;
			} else if (*p != ',' || !comma) {
				break;
			}
			while (isspace (*(++p)));
		}
	}
	
	if (get_value (parser, CSS_FONT_SIZE, output, sizeof(output))) {
	
		if (isdigit (output[0])) {
			short size = numerical (output, NULL, current->font->Size,
			                        current->word->font->SpaceWidth);
			if (size > 0) {
				if (!fstk) {
					fstk = fontstack_push (current, -1);
				}
				fontstack_setSize (current, size);
			}
		
		} else {   /* !isdigit() */
			
			if (stricmp (output, "smaller") == 0) {
				step = (current->font->Step > 1 ? current->font->Step -1 : 0);
			
			} else if (stricmp (output, "larger") == 0) {
				step = (current->font->Step < 6 ? current->font->Step +1 : 7);
			
			} else {
				static const char * arr[] = {
					"xx-small", "x-small", "small", "medium",
					"large", "x-large", "xx-large" };
				step = (int)numberof(arr);
				while (step-- && stricmp (output, arr[step]));
			}
			if (step >= 0 && fstk) {
				word_set_point (current,
				                fstk->Size = font_step2size (fstk->Step = step));
			}
		}
	}
	if (!fstk) {
		fstk = fontstack_push (current, step);
	}
	
	if (get_value (parser, KEY_FACE, output, sizeof(output))) {
		short fnt = font_face (output, TAgetFont (current->word->attr));
		/* n/i: cursive, fantasy */
		if (fnt != TAgetFont (current->word->attr)) {
			fontstack_setType (current, fnt);
		}
	}
	
	if (get_value (parser, CSS_FONT_STYLE, output, sizeof(output))
	    && (stricmp (output, "italic") == 0 ||
	        stricmp (output, "oblique") == 0)) {
		/* n/i: normal */
		fontstack_setItalic (current);
	}
	
	if (get_value (parser, CSS_FONT_WEIGHT, output, sizeof(output))) {
		BOOL bold;
		if (isdigit (output[0])) {
			long n = (int)strtoul (output, NULL, 10);
			bold = (n >= 700);
		} else {
			bold = (stricmp (output, "bold") == 0 ||
			        stricmp (output, "bolder") == 0);
		}
		/* n/i: lighter, medium, 100..300 */
		if (bold) fontstack_setBold (current);
	}
	
	if (get_value (parser, CSS_TEXT_DECORATION, output, sizeof(output))) {
		if (stricmp (output, "line-through") == 0) {
			fontstack_setStrike (current);
		} else if (stricmp (output, "underline") == 0) {
			fontstack_setUndrln (current);
		}
		/* n/i: overline */
	}
	
	if (get_value (parser, CSS_WHITE_SPACE, output, sizeof(output))) {
		if (stricmp (output, "nowrap") == 0) {
			fontstack_setNoWrap (current);
		}
		/* n/i: normal, pre */
	}
	
	if (!ignore_colours) {
		WORD color = get_value_color (parser, KEY_COLOR);
		if (color >= 0) {
			fontstack_setColor (current, color);
		}
	}
	
	return fstk;
}


/*----------------------------------------------------------------------------*/
static void
box_frame (PARSER parser, TBLR * bf, HTMLCSS key)
{
	short em = parser->Current.font->Size;
	short ex = parser->Current.word->font->SpaceWidth;
	char  out[100];
	short val;
	if (get_value (parser, key, out, sizeof(out))) {
		char * ptr = out;
		if ((val = numerical (ptr, &ptr, em, ex)) >= 0) {
			bf->Top = bf->Rgt = bf->Bot = bf->Lft = val;
			if ((val = numerical (ptr, &ptr, em, ex)) >= 0) {
				bf->Rgt = bf->Lft = val;
				if ((val = numerical (ptr, &ptr, em, ex)) >= 0) {
					bf->Bot = val;
					if ((val = numerical (ptr, &ptr, em, ex)) >= 0) {
						bf->Lft = val;
					}
				}
			}
		}
	}
	if (get_value (parser, key+1, out, sizeof(out)) &&
	    (val = numerical (out, NULL, em, ex)) >= 0) {
		bf->Bot = val;
	}
	if (get_value (parser, key+2, out, sizeof(out)) &&
	    (val = numerical (out, NULL, em, ex)) >= 0) {
		bf->Lft = val;
	}
	if (get_value (parser, key+3, out, sizeof(out)) &&
	    (val = numerical (out, NULL, em, ex)) >= 0) {
		bf->Rgt = val;
	}
	if (get_value (parser, key+4, out, sizeof(out)) &&
	    (val = numerical (out, NULL, em, ex)) >= 0) {
		bf->Top = val;
	}
}

/*----------------------------------------------------------------------------*/
static void
css_box_styles (PARSER parser, DOMBOX * box, H_ALIGN align)
{
	char out[100];
	
	if (get_value (parser, CSS_BORDER, out, sizeof(out))) {
		char * p = out;
		while (*p) {
			long color = -1;
			if (isdigit (*p)) {
				char * tail = p;
				short width = numerical (p, &tail, parser->Current.font->Size,
			                            parser->Current.word->font->SpaceWidth);
				if (width >= 0 && tail > p) {
					box->BorderWidth = width;
					p = tail;
				} else {
					break;
				}
			} else if (*p == '#') {
				short len = 0;
				while (isxdigit (p[++len]));
				if ((color = scan_color (p, len)) >= 0) {
					p += len;
				} else {
					break;
				}
			} else if (isalpha (*p)) {
				short len = 0;
				while (isalpha (p[++len]));
				color = scan_color (p, len);
				p += len;
			}
			if (color >= 0 && !ignore_colours) {
				box->BorderColor = remap_color (color);
			}
			if (!isspace (*p)) {
				break;
			}
			while (isspace (*(++p)));
		}
	}
	if (!ignore_colours) {
		WORD color = get_value_color (parser, KEY_BGCOLOR);
		if (color >= 0 && color != parser->Current.backgnd) {
			box->Backgnd = parser->Current.backgnd = color;
		}
		if ((color = get_value_color (parser, CSS_BORDER_COLOR)) >= 0) {
			box->BorderColor = color;
		}
	}
	box->BorderWidth = get_value_unum (parser, KEY_BORDER, box->BorderWidth);
	
	box_frame (parser, &box->Margin,  CSS_MARGIN);
	box_frame (parser, &box->Padding, CSS_PADDING);

	if ((int)(align = get_h_align (parser, align)) >= 0) {
		box->TextAlign = align;
	}

	if (get_value (parser, CSS_TEXT_INDENT, out, sizeof(out))) {
		char * tail   = out;
		short  indent = numerical (out, &tail, parser->Current.font->Size,
		                           parser->Current.word->font->SpaceWidth);
		if (tail > out) {
			box->TextIndent = indent;
		}
	}
	if (get_value (parser, KEY_WIDTH, out, sizeof(out))) {
		short width = numerical (out, NULL, parser->Current.font->Size,
		                         parser->Current.word->font->SpaceWidth);
		if (width > 0) {
			box->SetWidth = width;
		}
	}
	if (get_value (parser, KEY_HEIGHT, out, sizeof(out))) {
		short height = numerical (out, NULL, parser->Current.font->Size,
		                          parser->Current.word->font->SpaceWidth);
		if (height > 0) {
			box->SetHeight = height;
		}
	}
	
	/* devl stuff, activate in cfg: DEVL_FLAGS = CssPosition */
	if (box->HtmlCode == TAG_DIV) {
		static BOOL __once = FALSE, _CssPosition = FALSE;
		if (!__once) {
			_CssPosition = (devl_flag ("CssPosition") != NULL);
		}
	if (_CssPosition &&   get_value (parser, CSS_POSITION, out, sizeof(out))) {
		UWORD mask = (stricmp (out, "absolute") == 0 ? 0x103 :
		              stricmp (out, "relative") == 0 ? 0x003 : 0);
		if (mask) {
			if (get_value (parser, CSS_LEFT, out, sizeof(out))) {
				short lft = numerical (out, NULL, parser->Current.font->Size,
			                          parser->Current.word->font->SpaceWidth);
				if (lft >= (mask & 0x100 ? 0 : 1)) box->SetPos.p_x = lft;
				else                               mask           &= ~0x001;
			}
			if (get_value (parser, CSS_TOP, out, sizeof(out))) {
				short top = numerical (out, NULL, parser->Current.font->Size,
			                          parser->Current.word->font->SpaceWidth);
				if (top >= (mask & 0x100 ? 0 : 1)) box->SetPos.p_y = top;
				else                               mask           &= ~0x002;
			}
			if (mask & 0x003) {
				DOMBOX * parent = box->Parent;
				box->SetPosMsk  = mask;
				while (parent) {
					parent->SetPosCld |= mask;
					parent = parent->Parent;
				}
			}
		}
	}
	} /* devl stuff */
	
	if (get_value (parser, CSS_FLOAT, out, sizeof(out))) {
		if      (stricmp (out, "right") == 0) box->Floating = FLT_RIGHT;
		else if (stricmp (out, "left")  == 0) box->Floating = FLT_LEFT;
	}
	if (get_value (parser, CSS_CLEAR, out, sizeof(out))) {
		if      (stricmp (out, "right") == 0) box->ClearFlt = BRK_RIGHT;
		else if (stricmp (out, "left")  == 0) box->ClearFlt = BRK_LEFT;
		else if (stricmp (out, "both")  == 0) box->ClearFlt = BRK_ALL;
	}
}

/*----------------------------------------------------------------------------*/
static void
box_anchor (PARSER parser, DOMBOX * box, BOOL force)
{
	TEXTBUFF current = &parser->Current;
	char     out[100];
	if (get_value (parser, KEY_ID, out, sizeof(out))
	    && dombox_setId (box, out, force)) {
		ANCHOR anchor = new_named_location (box->IdName, box);
		if (anchor) {
			anchor->offset.Y = 0;
			*current->anchor = anchor;
			current->anchor  = &anchor->next_location;
		}
	}
	if (get_value (parser, KEY_CLASS, out, sizeof(out))) {
		dombox_setClass (box, out, force);
	}
}

/*----------------------------------------------------------------------------*/
static DOMBOX *
group_box (PARSER parser, HTMLTAG tag, H_ALIGN align)
{
	TEXTBUFF current = &parser->Current;
	DOMBOX * box     = create_box (current, BC_GROUP, 0);
	
	box->HtmlCode = tag;
	
	css_box_styles (parser, box, align);

	current->paragraph->Box.TextAlign  = box->TextAlign;
	current->paragraph->Box.TextIndent = box->TextIndent;
	
	css_text_styles (parser, current->font);
	
	box_anchor (parser, box, TRUE);
	
	return box;
}


/*============================================================================*/
DOMBOX *
create_box (TEXTBUFF current, BOXCLASS bc, WORD par_top)
{
	DOMBOX * box = dombox_ctor (malloc (sizeof(DOMBOX)), current->parentbox, bc);
	PARAGRPH par = add_paragraph (current, par_top);
	
	box->Margin.Top = par->Box.Margin.Top;
	par->Box.Margin.Top = 0;
	
	dombox_adopt (current->parentbox = box, &par->Box);
	
	fontstack_push (current, -1);
	
	return box;
}

/*============================================================================*/
DOMBOX *
leave_box (TEXTBUFF current, WORD tag)
{
	DOMBOX * box = current->parentbox;
	
	if (box->HtmlCode != tag) {
		box = NULL;
	
	} else {
		PARAGRPH par = add_paragraph (current, 0);
		current->parentbox = box->Parent;
		dombox_adopt (box->Parent, &par->Box);
		par->Box.TextAlign = box->Parent->TextAlign;
		
		fontstack_pop (current);
		
		{	/* find the next box below with valid background color */
			DOMBOX * tmp = box;
			do if (tmp->Backgnd >= 0) {
				current->backgnd = tmp->Backgnd;
				break;
			} while ((tmp = tmp->Parent) != NULL);
		}
	}
	return box;
}


/*----------------------------------------------------------------------------*/
static BOOL
anc_correct (char * anc)
{
	char * p = anc;
	
	while (*p == '#') {         /* skip leading hashes */
		p++;
	}
	while (*p && *p <= ' ') {   /* skip leading blanks */
		p++;
	}
	if (p > anc) {
		char * q = anc;
		while ((*(q++) = *(p++)) != '\0');
		p = anc;
	}
	
	while (*p != '%' && *p != '&' && *p > ' ') { /* search for something to do */
		p++;
	}
	
	/* encode '%xx' and named entities, compact white spaces
	 * and remove control characters
	*/
	if (*p) {
		BOOL   b = FALSE;
		char * q = p, c;
		while ((c = *(p++)) != '\0') {
			if (c == '%' && isxdigit(p[0]) && isxdigit(p[1])) {
				char buf[4]; buf[0] = *(p++); buf[1] = *(p++); buf[2] = '\0';
				c = strtoul (buf, NULL, 16);
			} else if (c == '&') {
				const char ** pp = (const char **)&p;
				char         uni[16];
				p--; /* set back to the '&' */
				if (scan_namedchar (pp, uni, TRUE, MAP_UNICODE) == uni +2
				    && !uni[0] && p[-1] == ';') {
					c = uni[1];
				} else {
					p++;
				}
			}
			if (isspace(c) && !b) {
				*(q++) = ' ';
				b      = TRUE;
			} else if (c > ' ') {
				*(q++) = c;
				b      = FALSE;
			}
		}
		if (b && q > anc) {
			q--;
		}
		*q = '\0';
	}
	
	return (*anc != '\0');
}

/*------------------------------------------------------------------------------
 * get rid of html entities in an url
*/
static char *
url_correct (char * url)
{
	char * p = url;

	if (*p && *p <= ' ') { /* skip leading control characters */
		char * q = url;
		while (*(++p) && *q <= ' ');
		while ((*(q++) = *(p++)) != '\0');
		p = url;
	}
	
	if (*p && *p != '#') do {
		if (*p == '&') {
			const char * src = p;
			char         uni[16];
			if (scan_namedchar (&src, uni, TRUE, MAP_UNICODE) == uni +2
			    && !uni[0] && src[-1] == ';') {
				char * dst = p +1;
				while ((*(dst++) = *(src++)) != '\0');
				*p = uni[1];
			}
		}
	} while (*(++p) && *p != '#');
	
	if (*p != '#') {
		while (--p >= url && *p <= ' ') {
			*p = '\0';
		}
	} else if (!anc_correct (p +1)) {
		*p = '\0';
	}

	return url;
}


/*----------------------------------------------------------------------------*/
static const char *
enc_to_sys (char * dst, size_t max_len,
            const char * src, ENCODING enc, HTMLTAG closer, BOOL ignore)
{
	ENCODER_C encoder = encoder_char (enc);
	char * beg = dst;
	char * end = dst + max_len -2;
	BOOL   spc = TRUE;
	char   c;
	
	while ((c = *src) != '\0') {
		if (c == '<') {
			if (!closer) {
				if (!ignore) break;
			
			} else {
				const char * sym = src +1;
				HTMLTAG      tag;
				BOOL         slash;
				if (*sym == '/') {
					sym++;
					slash = TRUE;
				} else {
					if (!ignore) break;
					slash = FALSE;
				}
				if ((tag = parse_tag (NULL, &sym)) == TAG_Unknown) {
					src = sym;
				} else if (tag != closer) {
					if (!ignore) break;
					src = sym;
				} else { /* equal */
					src = sym;
					if (slash) break;
				}
				continue;
			}
		}
		if (dst >= end) {
			src++;
			continue;
		}
		if (c <= ' ') {
			if (!spc) {
				*(dst++) = ' ';
				spc = TRUE;
			}
			src++;
			continue;
		}
		if (c >= 0x7F) {
			if (dst >= end -4) {
				dst = end;
			} else {
				dst = (*encoder)(&src, dst);
			}
		} else if (c == '&') {
			if (dst >= end -4) {
				dst = end;
			} else {
				dst = scan_namedchar (&src, dst, FALSE,0);
			}
		} else {
			src++;
			*(dst++) = c;
		}
		spc = FALSE;
	}
	if (spc && dst > beg) {
		dst--;
	}
	*dst = '\0';
	
	return src;
}


/*******************************************************************************
 *
 * Head And Structure Elements
*/

#ifdef __PUREC__  /* keep quiet on unused arguments */
#	define UNUSED(v) v = v
#else
#	define UNUSED(v)
#endif

/*------------------------------------------------------------------------------
 * Document Head
 */
static UWORD
render_HTML_tag (PARSER parser, const char ** text, UWORD flags)
{
	if (flags & PF_START) {
		char output[28];
		
		if (get_value (parser, KEY_LANG, output, sizeof(output))
		    && !isalpha(output[2])) {
			parser->Current.quot_lang =
			parser->Frame->Language
			                = ((UWORD)toupper(output[0]) <<8) | toupper(output[1]);
		}
	
	} else if (!strstr (*text, "</html>") && !strstr (*text, "</HTML>")) {
		*text = strchr (*text, '\0');
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Title
 *
 * parses a title tag from a file 
 * and sets the window's title to this value
 * also watches for ISO Latin entities in the Title string
 * Baldrick Dec. 6, 2001
 *
 * Modified to use new ISO scanning routines
 * AltF4 Dec. 19, 2001
*/
static UWORD
render_TITLE_tag (PARSER parser, const char ** text, UWORD flags)
{
	if (flags & PF_START) {
		char   title[128] = "";
		size_t size       = min (sizeof(title), aes_max_window_title_length);
		*text = enc_to_sys (title, size,
		                    *text, parser->Frame->Encoding, TAG_TITLE, TRUE);
		if (title[0]) {
			containr_notify (parser->Target, HW_SetTitle, title);
			font_switch (parser->Current.word->font, NULL);
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Frameset Tag
 *
 * it parses and processes the frameset information
 * until done and then returns to the main parser with
 * a pointer to the end of the frameset
 * baldrick - August 8, 2001
 *
 * There are some printf's in this section that are rem'd out
 * they are useful checks on the frame parsing and I'm not
 * certain that it's 100% correct yet.  So I have left them
 * baldrick - August 14, 2001
 *
 * Major reworks by AltF4 in late January - early Feb 2002
*/
static UWORD
render_FRAMESET_tag (PARSER parser, const char ** text, UWORD flags)
{
	const char * symbol    = *text;
	CONTAINR     container = parser->Target;
	FRAME        frame     = parser->Frame;
	LOCATION     base      = frame->BaseHref;
	BOOL    update = FALSE;
	HTMLTAG tag    = TAG_FRAMESET;
	BOOL    slash  = FALSE;
	int     depth  = 0;

	if (!container) {
		hwUi_fatal ("render_frameset()",
		            "No container in '%.160s'!", frame->Location->File);
	} else if (container->Mode) {
		hwUi_fatal ("render_frameset()",
		            "Container not cleared in '%.160s'!", frame->Location->File);
	}

	if (flags & PF_START) do {
	
		if (!slash) {

			/* only work on an empty container.  if not empty there was either a
			 * forgotten </framset> or more tags than defined in the frameset.
			 */
			if (!container->Mode) {

				if (tag == TAG_FRAMESET) {

					char cols[100], rows[100];
					
					WORD border = get_value_unum (parser, KEY_BORDER, -1);
					if (border >= 0) {
						if (border || !force_frame_controls) {
							container->BorderSize = border;
						}
					} else {
						char out[4];
						if (get_value (parser, KEY_FRAMEBORDER, out, sizeof(out))) {
							if (out[0] == '0' || stricmp (out, "NO") == 0) {
								border = 0;
							} else {
								border = get_value_unum (parser,
								                         KEY_FRAMESPACING, BORDER_SIZE);
							}
						} else {
							border = get_value_unum (parser, KEY_FRAMESPACING,
							                         container->BorderSize);
						}
						if (border || !force_frame_controls) {
							container->BorderSize = border;
						}
					}
					if (!update && container->BorderSize && container->Sibling) {
						update = TRUE;
					}
					
					if (!ignore_colours) {
						WORD color;
						if ((color = get_value_color (parser, KEY_BORDERCOLOR)) >= 0)
							container->BorderColor = color;
					}
					
					/* ok the first thing we do is look for ROWS or COLS
					 * since when we are dumped here we are looking at a
					 * beginning of a FRAMESET tag
					 */
					if ((!get_value (parser, KEY_COLS, cols, sizeof(cols))
					     || !strchr (cols, ',')) &&
					      get_value (parser, KEY_ROWS, rows, sizeof(rows))) {
						cols[0] = '\0';
					}
					
					if (*cols) {
						containr_fillup (container, cols, TRUE);
						container = container->u.Child;
						depth++;
					}

					else /* at the moment settings of either COLS _and_ ROWS aren't
					      * working yet, so we make it mutual exlusive for now   */

					if (*rows) {
						containr_fillup (container, rows, FALSE);
						container = container->u.Child;
						depth++;
					}
				
				} else if (tag == TAG_FRAME) {
					char frame_file[HW_PATH_MAX];

					container->Mode = CNT_FRAME;
					container->Name = get_value_str (parser, KEY_NAME);

					if (!force_frame_controls) {
						char out[100];
						
						if (get_value_exists (parser, KEY_NORESIZE)) {
							container->Resize = FALSE;
						}
						
						if (get_value (parser, KEY_SCROLLING, out, sizeof(out))) {
							if (stricmp (out, "yes") == 0) {
								container->Scroll = SCROLL_ALWAYS;
							} else if (stricmp (out, "no")  == 0) {
								container->Scroll = SCROLL_NEVER;
							}
						}
					}

					if (get_value (parser, KEY_SRC, frame_file,sizeof(frame_file))) {
						LOADER ldr = start_page_load (container,
						                              url_correct (frame_file),
						                              base, FALSE, NULL);
						if (ldr) {
							short lft = get_value_unum (parser, KEY_MARGINWIDTH,  -1);
							short top = get_value_unum (parser, KEY_MARGINHEIGHT, -1);
							if (lft >= 0) ldr->MarginW = lft;
							if (top >= 0) ldr->MarginH = top;
						}
					} else {
						update = TRUE;
					}

					if (container->Sibling) {
						container = container->Sibling;
					}
				}
			} /* endif (!container->Mode) */

		} else if (tag == TAG_FRAMESET) { /* && slash */

			container = containr_resume (container);

			if (--depth <= 0) {
				symbol = strchr (symbol, '\0');
				break;
			}
			if (container->Sibling) {
				container = container->Sibling;
			}
		}

		/* skip junk until the next <...> */

		while (*symbol && *(symbol++) != '<');
		if (*symbol) {
			slash = (*symbol == '/');
			if (slash) symbol++;
			tag = parse_tag (parser, &symbol);
			continue;
		}
	}
	while (*symbol);
	
	while (container) {
		container = containr_resume (container);
	}
	if (update) {
		containr_calculate (parser->Target, NULL);
		containr_notify    (parser->Target, HW_PageUpdated, NULL);
	}
	
	*text = symbol;
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Base URL And Target
*/
static UWORD
render_BASE_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		char output[HW_PATH_MAX];
		if (get_value (parser, KEY_HREF, output, sizeof(output)-1)) {
			LOCATION base;
			size_t   len = strlen (output);
			if (len && output[len -1] != '/') {
				char buff[sizeof(output)];
				location_FullName (parser->Frame->Location, buff, sizeof(buff));
				if (strcmp (output, buff) != 0) {
					strcat (output, "/");
				}
			}
			base = new_location (output, parser->Frame->Location);
			if (base) {
				free_location (&parser->Frame->BaseHref);
				parser->Frame->BaseHref = base;
			}
		} else {
			char * target = get_value_str (parser, KEY_TARGET);
			if (target) {
				if (parser->Frame->base_target) {
					free (parser->Frame->base_target);
				}
				parser->Frame->base_target = target;
			}
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Meta Descriptions
*/
static UWORD
render_META_tag (PARSER parser, const char ** text, UWORD flags)
{
	char output[1024];
	UNUSED (text);
	
	if ((flags & PF_START)
	    && get_value (parser, KEY_HTTP_EQUIV, output, sizeof(output))) {
		
		if (stricmp (output, "Content-Type") == 0) {
			if (!parser->Frame->ForceEnc
			    && get_value (parser, KEY_CONTENT, output, sizeof(output))) {
				ENCODING cset = http_charset (output, strlen(output), NULL);
				if (cset) {
					parser->Frame->Encoding = cset;
					flags |= PF_ENCDNG;
				}
				/* http://www.iana.org/assignments/character-sets */
			}
		
		} else if (stricmp (output, "refresh") == 0) {
			if (get_value (parser, KEY_CONTENT, output, sizeof(output))) {
				char * url = output;
				long   sec = strtoul (output, &url, 10);
				if (sec < 30) {
					if    (ispunct (*url)) url++;
					while (isspace (*url)) url++;
					if (strnicmp (url, "URL=", 4) == 0) {
						url += 4;
						if (*url == '"') {
							char * end = strchr (++url, '\0') -1;
							if (*end == '"') *end = '\0';
						} else if (*url == '\'') {
							char * end = strchr (++url, '\0') -1;
							if (*end == '\'') *end = '\0';
						}
						if (*url) {
							start_page_load (parser->Target, url,
							                 parser->Frame->BaseHref, FALSE, NULL);
						}
					}
				}
			}
		} else if (stricmp (output, "expires") == 0) {
			if (PROTO_isRemote (parser->Loader->Location->Proto) &&
			    parser->Loader->Date &&
			    get_value (parser, KEY_CONTENT, output, sizeof(output))) {
				long date;
				if (isdigit (*output)) { /* invalid format but often used */
					if ((date = strtol (output, NULL, 10)) >= 0) {
						date += parser->Loader->Date;
					}
				} else {
					date = http_date (output);
				}
				if (date > 0 && !parser->Loader->PostBuf) {
					date += parser->Loader->Tdiff;
					cache_expires (parser->Loader->Location, date);
				}
			}
		} else if (stricmp (output, "pragma") == 0) {
			if (parser->Loader->Date && !parser->Loader->PostBuf &&
			    get_value (parser, KEY_CONTENT, output, sizeof(output)) &&
			    stricmp (output, "no-cache") == 0) {
			    	cache_expires (parser->Loader->Location, -1);
			}
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Style Area
*/
static UWORD
render_STYLE_tag (PARSER parser, const char ** text, UWORD flags)
{
	if (flags & PF_START) {
		char out[100];
		const char * line = *text;
		
		if ((!get_value (parser, KEY_TYPE, out, sizeof(out)) ||
		     mime_byString (out, NULL) == MIME_TXT_CSS) &&
		    (!get_value (parser, KEY_MEDIA, out, sizeof(out)) ||
		     strstr (out, "all") || strstr (out, "screen"))) {
			
			if (!parser->ResumeSub) { /* initial call */
				while (isspace (*line)) line++;
				
				if (*line == '<') {        /* skip leading '<!--' */
					if (*(++line) == '!') {
						while (*(++line) == '-');
					} else {
						line--;
					}
				}
				if (!cfg_UseCSS) {
					char * p = strchr (line, '<');
					if (p) line = p;
				
				} else if (*line != '<') {
					line = parse_css (parser, line, NULL);
				}
			
			} else {
				line = (parser->ResumeErr ? NULL : parser->ResumeSub);
				line = parse_css (parser, line, NULL);
			}
			
			if (!line) {
				parser_resume (parser, render_STYLE_tag, *text, NULL);
				longjmp (resume_jbuf, 1);
			}
			
			if (*line == '-') {        /* skip trailing '-->' */
				while (*(++line) == '-');
				if (*line == '>') {
					while (isspace (*(++line)));
				}
			}
			/* should be positioned just before the next tag now */
			do {
				while (*line && *(line++) != '<');
				if (isalpha (*line)) {
					line--; /* faulty HTML, the </style> tag is missing */
					break;
				} else {
					BOOL slash = (*line == '/');
					if (slash) line++;
					if (parse_tag (parser, &line) == TAG_STYLE && slash) {
						break;
					}
				}
			} while (*line);
			
			*text = line;
		}
	}
	return flags;
}


/*------------------------------------------------------------------------------
 * Logical Reference
*/
static UWORD
render_LINK_tag (PARSER parser, const char ** text, UWORD flags)
{
	char out[HW_PATH_MAX];

	if ((flags & PF_START) && get_value(parser,KEY_REL,out,sizeof(out))) {
		if (stricmp (out, "StyleSheet") == 0) {
			if (cfg_UseCSS
			    && (!get_value (parser, KEY_TYPE, out, sizeof(out)) ||
			        mime_byString (out, NULL) == MIME_TXT_CSS)
		       && (!get_value (parser, KEY_MEDIA, out, sizeof(out)) ||
			        strstr (out, "all") || strstr (out, "screen"))
			    && get_value (parser, KEY_HREF, out, sizeof(out))) {
				
				LOCATION     loc  = NULL;
				const char * line = NULL;
				char       * file = NULL;
				BOOL         call = FALSE;
				BOOL         rsum = FALSE;
				BOOL         jump = FALSE;
				
				if (!parser->ResumeSub) { /* initial call */
					size_t size = 0;
					loc = new_location (out, parser->Frame->BaseHref);
					if (PROTO_isLocal (loc->Proto)) {
						file = load_file (loc, &size, &size);
					} else {
						struct s_cache_info info;
						CRESULT res = cache_query (loc, 0, &info);
						if (res & CR_LOCAL) {
							file = load_file (info.Local, &size, &size);
						} else {
							rsum = !(res & CR_BUSY);
							jump = TRUE;
						}
					}
					if (file) {
						free_location (&loc);
						if (size <= 0) {
							free (file);
						} else {
							call = TRUE;;
						}
					}
				
				} else {
					line = (parser->ResumeErr ? NULL : parser->ResumeSub);
					call = TRUE;
				}
				
				if ((call && !parse_css (parser, line, file)) || rsum) {
					parser_resume (parser, render_LINK_tag, *text, loc);
					jump = TRUE;
				}
				if (loc)  free_location (&loc);
				if (jump) longjmp (resume_jbuf, 1);
			}
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Background Sound
 *
 * Currently it simply fires up a new loader job and lets
 * the loader find out what to do.
 *
 * AltF4 - Mar. 01, 2002
*/
static UWORD
render_BGSOUND_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		char snd_file[HW_PATH_MAX];

		if (get_value (parser, KEY_SRC, snd_file, sizeof(snd_file))) {
			start_objc_load (parser->Target, snd_file, parser->Frame->BaseHref,
			                 (int(*)(void*,long))NULL, NULL);
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Document Body
 *
 * modifications for new color routines
 * AltF4 - December 26, 2001
 *
 * AltF4 - Jan. 11, 2002:  modifications for yet another color routine
*/
static UWORD
render_BODY_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START && !parser->Frame->Page.HtmlCode) {
		FRAME frame = parser->Frame;
		WORD  margin;
		
		frame->Page.HtmlCode = TAG_BODY;
		
		if (!ignore_colours) {
			WORD color;
	
			if ((color = get_value_color (parser, KEY_TEXT)) >= 0) {
				fontstack_setColor (&parser->Current, color);
				frame->text_color = color;
			}
			if ((color = get_value_color (parser, KEY_BGCOLOR)) >= 0) {
				frame->Page.Backgnd = parser->Current.backgnd = color;
			}
			if ((color = get_value_color (parser, KEY_LINK)) >= 0) {
				frame->link_color = color;
			}
		}
		
		if ((margin = get_value_unum (parser, KEY_MARGINHEIGHT, -1)) >= 0 ||
		    (margin = get_value_unum (parser, KEY_TOPMARGIN, -1)) >= 0) {
			frame->Page.Padding.Top = frame->Page.Padding.Bot = margin;
		}
		if ((margin = get_value_unum (parser, KEY_MARGINWIDTH, -1)) >= 0 ||
		    (margin = get_value_unum (parser, KEY_LEFTMARGIN, -1)) >= 0) {
			frame->Page.Padding.Lft = frame->Page.Padding.Rgt = margin;
		}
		if (parser->hasStyle) {
			box_frame (parser, &frame->Page.Padding, CSS_MARGIN);
		}
		box_anchor (parser, &frame->Page, TRUE);
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Script Area
 *
 * actually skips everything until </SCRIPT> or <NOSCRIPT> tag
*/
static UWORD
render_SCRIPT_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (parser);
	
	if (flags & PF_START) {
		const char * line = *text;
		char         quot = '\0', c;
		
		while ((c = *line) != '\0') {
			line++;
			
			/* normal javascript code level */
			
			if (c == '<') {
				*text = line; /* save this */
				if (*line == '!') {        /* just a comment tag, simply skip it */
					while (*(++line) == '-');
					continue;
				}
				if (*line == '/' || parse_tag (NULL, &line) == TAG_NOSCRIPT) {
					line = *text -1; /* ending tag found, reset pointer to the '<' */
					break;
				}
				/* else still code, continue just after the '<' */
				line = *text;
			}
			if (c != '\'' && c != '\"') {          /* normal character, go ahead */
				continue;
			
			} else {               /* quotation ".." or '..' found, fall through */
				quot = c;
			}
			
			/* quotation scanner */
			
			while (quot) {
				if ((c = *(line++)) == '\0') {
					line--;   /* ran out of data */
					break;
				}
				if (c == quot) { /* quotation end found, quit this inner loop */
					quot = '\0';
				
				} else if (c == '\\') { /* escape character, skip next char */
					if (!*line) break;   /* ran out of data */
					else        line++;  /* skip this */
				}
			}
		}
		*text = line;
	
	} else {
		flags &= ~PF_SCRIPT;
	}

	return flags;
}

/*------------------------------------------------------------------------------
 * NoScript Area
 *
 * actually skips everything until </SCRIPT> or <NOSCRIPT> tag
*/
static UWORD
render_NOSCRIPT_tag (PARSER parser, const char ** text, UWORD flags)
{
	if ((flags & (PF_START|PF_SCRIPT)) == PF_SCRIPT) {
		flags = render_SCRIPT_tag (parser, text, flags);
	}
	return flags;
}


/*******************************************************************************
 *
 * Font style and phrase elements
*/

/*------------------------------------------------------------------------------
 * Bold Text
*/
static UWORD
render_B_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	word_set_bold (&parser->Current, (flags & PF_START));
	return flags;
}

/*------------------------------------------------------------------------------
 * Base Font Setting
*/
static UWORD
render_BASEFONT_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		TEXTBUFF current = &parser->Current;
		FNTSTACK fstk    = &current->fnt_stack;
		char output[10];
		
		if (get_value (parser, KEY_SIZE, output, sizeof(output))) {
			if (*output == '+') {
				if (isdigit(output[1])) {
					fstk->Step += output[1]  - '0';
				}
			} else {
				if (*output == '-') {
					if (isdigit(output[1])) {
						fstk->Step -= output[1]  - '0';
					}
				} else if (isdigit(output[0])) {
					fstk->Step = output[0]  - '0';
				}
				if (fstk->Step < 1) fstk->Step = 1;
			}
			if (fstk->Step > 7) fstk->Step = 7;
			
			fstk->Size = font_step2size (fstk->Step);
			if (fstk == current->font) {
				word_set_point (current, fstk->Size);
			}
		}

		if (!ignore_colours) {
			WORD color = get_value_color (parser, KEY_COLOR);
			if (color >= 0) {
				fstk->Color = color;
				if (fstk == current->font) {
					word_set_color (current, parser->Frame->text_color = color);
				}
			}
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Big Font Size
*/
static UWORD
render_BIG_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (flags & PF_START) {
		fontstack_push (current, current->font->Step +1);
		if (parser->hasStyle) {
			css_text_styles (parser, current->font);
		}
	} else {
		fontstack_pop (current);
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Citation
*/
#define render_CITE_tag   render_I_tag

/*----------------------------------------------------------------------------*/
static UWORD
render_CODE_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	word_set_font (current, (flags & PF_START ? pre_font : current->font->Type));
	TAsetCondns   (current->word->attr, (flags & PF_START));
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Deleted Text Style (same as NN ò 6 and IE ò 5)
*/
#define render_DEL_tag render_STRIKE_tag

/*------------------------------------------------------------------------------
 *  Instance Definition
*/
#define render_DFN_tag   render_I_tag

/*------------------------------------------------------------------------------
 *  Emphasis Text
*/
#define render_EM_tag   render_I_tag

/*------------------------------------------------------------------------------
 * Font Tag
 *
 * color handling modified to new routines
 * AltF4 - Dec 26, 2001
 *
 * AltF4 - Jan. 11, 2002:  modifications for yet another color routine
 *
 * Baldrick - March 1, 2002: modifications to always store a font step
 *            so that we don't loose our size on complex sites
*/
static UWORD
render_FONT_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		char output[100];
		WORD step = current->font->Step;

		if (get_value (parser, KEY_SIZE, output, sizeof(output))) {
		
			if (*output == '+') {
				if (isdigit(output[1])) {
					step += output[1]  - '0';
				}
			} else {
				if (*output == '-') {
					if (isdigit(output[1])) {
						step -= output[1]  - '0';
					}
				} else if (isdigit(output[0])) {
					step = output[0]  - '0';
				}
				if (step < 1) step = 1;
			}
		}
		fontstack_push (current, step);
		
		if (parser->hasStyle) {
			css_text_styles (parser, current->font);
		
		} else {
			if (get_value (parser, KEY_FACE, output, sizeof(output))) {
				short fnt = font_face (output, TAgetFont (current->word->attr));
				if (fnt != TAgetFont (current->word->attr)) {
					fontstack_setType (current, fnt);
				}
			}
			
			if (!ignore_colours) {
				WORD color = get_value_color (parser, KEY_COLOR);
				if (color >= 0) {
					fontstack_setColor (current, color);
				}
			}
		}
	
	} else {
		fontstack_pop (current);
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 *  General Text Element
*/
static UWORD
render_SPAN_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		css_text_styles (parser, NULL);
	
	} else {
		fontstack_pop (&parser->Current);
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Italic Text
*/
static UWORD
render_I_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	word_set_italic (&parser->Current, (flags & PF_START));
	return flags;
}

/*------------------------------------------------------------------------------
 * Inserted Text Style (same as NN ò 6 and IE ò 5)
*/
#define render_INS_tag render_U_tag

/*------------------------------------------------------------------------------
 *  Keyboard Input Text Style
*/
#define render_KBD_tag render_TT_tag

/*------------------------------------------------------------------------------
 *  Strike Through Text Style
*/
#define render_S_tag   render_STRIKE_tag

/*------------------------------------------------------------------------------
 *  Sample Code Or Script Text Style
*/
#define render_SAMP_tag   render_TT_tag

/*------------------------------------------------------------------------------
 * Small Font Size
*/
static UWORD
render_SMALL_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (flags & PF_START) {
		fontstack_push (current, current->font->Step -1);
		if (parser->hasStyle) {
			css_text_styles (parser, current->font);
		}
	} else {
		fontstack_pop (current);
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Strike Through Text Style
*/
static UWORD
render_STRIKE_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	word_set_strike (&parser->Current, (flags & PF_START));
	return flags;
}

/*------------------------------------------------------------------------------
 * Strong Text Style
*/
#define render_STRONG_tag   render_B_tag

/*------------------------------------------------------------------------------
 * Subscript Text
*/
static UWORD
render_SUB_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (flags & PF_START) {
		fontstack_push (current, current->font->Step -1);
		current->word->vertical_align = ALN_BELOW;
		if (parser->hasStyle) {
			css_text_styles (parser, current->font);
		}
	} else {
		fontstack_pop (current);
		current->word->vertical_align = ALN_BOTTOM;
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Superscript Text
*/
static UWORD
render_SUP_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (flags & PF_START) {
		fontstack_push (current, current->font->Step -1);
		current->word->vertical_align = ALN_ABOVE;
		if (parser->hasStyle) {
			css_text_styles (parser, current->font);
		}
	} else {
		fontstack_pop (current);
		current->word->vertical_align = ALN_BOTTOM;
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Teletype Text Style
*/
static UWORD
render_TT_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	word_set_font (current, (flags & PF_START ? pre_font : current->font->Type));
	return flags;
}

/*------------------------------------------------------------------------------
 * Underlined Text
*/
static UWORD
render_U_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	word_set_underline (&parser->Current, (flags & PF_START));
	return flags;
}

/*------------------------------------------------------------------------------
 *  Variable Name Text Style
*/
#define render_VAR_tag render_I_tag

/*------------------------------------------------------------------------------
 *  Example Text Style
*/
#define render_XMP_tag render_TT_tag


/*******************************************************************************
 *
 * Special Text Level Elements
*/

/*------------------------------------------------------------------------------
 * Anchor Or Link
*/
static UWORD
render_A_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	WORDITEM word    = current->word;
	UNUSED  (text);
	
	if (flags & PF_START) {
		char * output;

		if ((output = get_value_str (parser, KEY_HREF)) != NULL)
		{
			char * p = url_correct (output);
			FRAME frame = parser->Frame;
			
			char * target = get_value_str (parser, KEY_TARGET);
			if (!target && frame->base_target) {
				target = strdup (frame->base_target);
			}
			
			if (*p && *p != '#') {
				BOOL  skip = FALSE;
				char * dst = p;
				do if ((*dst = *(p)) > ' ' || skip) {
					skip |= (*(dst++) == '#');
				} while (*(p++));
			}
			
			if (!word->link) {
				word_set_underline (current, TRUE);
			}
			word_set_color (current, frame->link_color);

			if ((word->link = new_url_link (word, output, TRUE, target)) != NULL) {
				char out2[30];
				if (get_value (parser, KEY_CHARSET, out2, sizeof(out2))) {
					/* ENCODING_WINDOWS1252 as default, which is commen practice. */
					word->link->encoding = scan_encoding(out2, ENCODING_WINDOWS1252);
				}
			}
			
			/* Since HTML 4.0 links ca also be used for image maps
			 * (not even supported by IE5, Netscape, CAB)
			 */
			if (current->maparea) {
				char coords[500], * href = NULL;
				if (get_value (parser, KEY_COORDS, coords, sizeof(coords))
				    && (href = strdup (output)) != NULL) {
					MAPAREA area;
					char    shape[10];
					if (!get_value (parser, KEY_SHAPE,  shape,  sizeof(shape))) {
						strcpy (shape, "rect");
					}
					target = get_value_str (parser, KEY_TARGET);
					area   = new_maparea (shape, coords, href, target, NULL);
					if (area) {
						*current->maparea = area;
						current->maparea  = &area->Next;
					} else {
						if (target) free (target);
						if (href)   free (href);
					}
				}
			}
			
		} else if (((output = get_value_str (parser, KEY_NAME)) != NULL ||
		            (output = get_value_str (parser, KEY_ID))   != NULL)
		           && anc_correct (output)) {
			struct url_link * presave = word->link;
			if ((word->link = new_url_link (word, output, FALSE, NULL)) != NULL) {
				ANCHOR anchor = new_named_location (word->link->address,
				                                    &current->paragraph->Box);
				if (!anchor) {  /* memory exhausted! */
					free (word->link);
					word->link = NULL;
				
				} else {
					word->link->u.anchor = anchor;
					*current->anchor     = anchor;
					current->anchor      = &anchor->next_location;
					
					/* prevent from getting deleted if empty */
					*(current->text++) = font_Nobrk (word->font);
					new_word (current, TRUE);
					word->word_width = 0;
					TAsetUndrln (word->attr, FALSE);
					current->word->link = presave;
				}
			}
		}	
	} else if (word->link) {
		word_set_color     (current, current->font->Color);
		word_set_underline (current, FALSE);
		word->link = NULL;
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 *  Image Map definition
*/
static UWORD
render_MAP_tag (PARSER parser, const char ** _text, UWORD flags)
{
	(void)_text;
	
	if (flags & PF_START) {
		char     name[100];
		if (get_value (parser, KEY_NAME, name, sizeof(name)) && strlen (name)) {
			IMAGEMAP map = create_imagemap (&parser->Frame->MapList, name, TRUE);
			if (map) {
				parser->Current.maparea = &map->Areas;
			}
		}
	
	} else {
		parser->Current.maparea = NULL;
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 *  Area definition for an Image Map
*/
static UWORD
render_AREA_tag (PARSER parser, const char ** _text, UWORD flags)
{
	(void)_text;
	
	if ((flags & PF_START) && parser->Current.maparea) {
		char shape[10], coords[500], * href = NULL;
		if (get_value (parser, KEY_SHAPE,  shape,  sizeof(shape)) &&
		    get_value (parser, KEY_COORDS, coords, sizeof(coords)) &&
		    (href = get_value_str (parser, KEY_HREF)) != NULL) {
			char * target = get_value_str (parser, KEY_TARGET);
			char * alt    = get_value_str (parser, KEY_ALT);
			MAPAREA area = new_maparea (shape, coords, href, target, alt);
			if (area) {
				*parser->Current.maparea = area;
				parser->Current.maparea  = &area->Next;
			} else {
				if (target) free (target);
				if (alt)    free (alt);
				if (href)   free (href);
			}
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Language Dependent Quotation Marks (in-paragraph citation)
 *
 * Source: The Unicode Standard Version 3.0, section 6.1 and own inventions
 * BUG: Should work for nested quotes, alternating double and single.  This
 * needs information about language dependent starting with single or double.
 *
 * Rainer Seitel, 2002-03-29
*/
static UWORD
render_Q_tag (PARSER parser, const char ** _text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	WORD     mapping = current->word->font->Base->Mapping;
	LANGUAGE language;
	char buf[28];
	(void)_text;

	if (flags & PF_START) {
		if (get_value (parser, KEY_LANG, buf, sizeof(buf)) && !isalpha(buf[2])) {
			current->quot_lang = ((UWORD)toupper(buf[0]) <<8) | toupper(buf[1]);
		} else {
			current->quot_lang = parser->Frame->Language;
		}
	}
	switch ((language = current->quot_lang)) {
		case LANG_CS:  /* Czech */
		case LANG_DE:  /* German, also using ¯® in books
		                * de-CH Swiss German using ®¯
		                * wen   Sorbic using German quotes, 3-letter language
		                *       tag not covered by HTML */
		case LANG_SK:  /* Slovak */
			if (flags & PF_START) {
				/* U+201E DOUBLE LOW-9 QUOTATION MARK */
				current->text = unicode_to_wchar (0x201E, current->text, mapping);
			} else {
				/* U+201C LEFT DOUBLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x201C, current->text, mapping);
			}
			break;
		case LANG_CA:  /* Catalan */
		case LANG_ET:  /* Estonian */
		case LANG_EL:  /* Greek */
		case LANG_FR:  /* French */
		case LANG_RU:  /* Russian, using as ®I will - he said - go home.¯
		                  with U+2015 HORIZONTAL BAR */
		case LANG_SQ:  /* Albanian */
			if (flags & PF_START) {
				/* U+00AB LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x00AB, current->text, mapping);
				if (language == LANG_FR)
					current->text = unicode_to_wchar (NOBRK_UNI,
					                                  current->text, mapping);
			} else {
				if (language == LANG_FR)
					current->text = unicode_to_wchar (NOBRK_UNI,
					                                  current->text, mapping);
				/* U+00BB RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x00BB, current->text, mapping);
			}
			break;
		case LANG_HU:  /* Hungarian */
		case LANG_PL:  /* Polish */
		case LANG_RO:  /* Rumanian */
			if (flags & PF_START) {
				/* U+201E DOUBLE LOW-9 QUOTATION MARK */
				current->text = unicode_to_wchar (0x201E, current->text, mapping);
			} else {
				/* U+201D RIGHT DOUBLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x201D, current->text, mapping);
			}
			break;
		case LANG_HR:  /* Croatian */
		case LANG_SH:  /* Serbo-Croatian */
		case LANG_SL:  /* Slovenian */
		case LANG_SR:  /* Serbian, also using U+201EU+201C and U+201EU+201D */
			if (flags & PF_START) {
				/* U+00BB RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x00BB, current->text, mapping);
			} else {
				/* U+00AB LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x00AB, current->text, mapping);
			}
			break;
		case LANG_DA:  /* Danish, also using ¯¯ in books */
		case LANG_FI:  /* Finnish, also using ¯¯ in books */
		case LANG_NB:  /* Bokm†l Norwegian */
		case LANG_NN:  /* Nynorsk Norwegian */
		case LANG_NO:  /* Norwegian, also using ¯¯ in books */
		case LANG_SV:  /* Swedish, also using ¯¯ in books */
			/* U+201D RIGHT DOUBLE QUOTATION MARK */
			current->text = unicode_to_wchar (0x201D, current->text, mapping);
			break;
		case LANG_EN:  /* English, single */
		case LANG_ES:  /* Spanish */
		case LANG_IT:  /* Italian, also using ®¯ and ®  ¯ */
		case LANG_NL:  /* Dutch, single, also using U+201E for start */
		case LANG_PT:  /* Portuguese, also using >< */
		case LANG_TR:  /* Turkish, also using ®¯ */
			if (flags & PF_START) {
				/* U+201C LEFT DOUBLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x201C, current->text, mapping);
			} else {
				/* U+201D RIGHT DOUBLE QUOTATION MARK */
				current->text = unicode_to_wchar (0x201D, current->text, mapping);
			}
			break;
		case LANG_HE:  /* Hebrew */
		case LANG_YI:  /* Yiddisch */
			/* U+05F4 HEBREW PUNCTUATION GERSHAYIM */
			current->text = unicode_to_wchar (0x05F4, current->text, mapping);
			break;
		case LANG_JA:  /* Japanese */
		case LANG_KO:  /* Korean */
		case LANG_ZH:  /* Chinese */
			if (flags & PF_START) {
				/* U+301D REVERSED DOUBLE PRIME QUOTATION MARK */
				current->text = unicode_to_wchar (0x301D, current->text, mapping);
			} else {
				/* There are some fonts without U+301E for horizontal text.
				 * Therefore use U+301F for vertical text. */
				/* U+301E DOUBLE PRIME QUOTATION MARK */
				current->text = unicode_to_wchar (0x301F, current->text, mapping);
			}
			break;
		default:  /* ASCII/typewriter quotes */
			current->text = unicode_to_wchar (0x0022, current->text, mapping);
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Processes embedded multi media objects, normally to be handled by an external
 * application.
 *
 * actually only sound is supported.
*/
static UWORD
render_EMBED_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		char src[HW_PATH_MAX], type[100];
		if (get_value (parser, KEY_SRC, src, sizeof(src))) {
			MIMETYPE mime = (get_value (parser, KEY_TYPE, type, sizeof(type))
		                 ? mime_byString   (type, NULL)
		                 : mime_byExtension (src, NULL, NULL));
			if (MIME_Major(mime) == MIME_AUDIO) {
				start_objc_load (parser->Target, src, parser->Frame->BaseHref,
				                 (int(*)(void*,long))NULL, NULL);
			}
		}
	}
	return flags;
}


/*------------------------------------------------------------------------------
 * Proprietary IOD4 Browser Tag
 *
 * see http://www.backspace.org/iod/Winhelp.html
 *
 * Matthias Jaap, 2002-07-22
*/
static UWORD
render_IOD_tag (PARSER parser, const char ** text, UWORD flags)
{
	char   output[128];
	UNUSED (text);

	if (flags & PF_START)
	{
		if (get_value (parser, KEY_SHOUT, output, sizeof(output)))
			containr_notify (parser->Target, HW_SetInfo, output);
	}

	return flags;
}

/*------------------------------------------------------------------------------
 * Image
*/
static UWORD
render_IMG_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		FRAME    frame    = parser->Frame;
		TEXTBUFF current  = &parser->Current;
		H_ALIGN  floating = get_h_align (parser, ALN_NO_FLT);
		V_ALIGN  v_align  = ALN_BOTTOM;
		TEXTATTR word_attr   = current->word->attr;
		short word_height    = current->word->word_height;
		short word_tail_drop = current->word->word_tail_drop;
		short word_v_align   = current->word->vertical_align;
		
		WORDITEM word;
		char output[100];
		char img_file[HW_PATH_MAX];
	
		if (alternative_text_is_on) {
			if (get_value (parser, KEY_ALT, output, sizeof(output))) {
				scan_string_to_16bit (output, frame->Encoding, &current->text,
				                      current->word->font->Base->Mapping);
			}
			return flags;
		}
	
		if (floating == ALN_NO_FLT) {
			if (get_value (parser, CSS_FLOAT, output, sizeof(output))) {
				if      (stricmp (output, "right") == 0) floating = FLT_RIGHT;
				else if (stricmp (output, "left")  == 0) floating = FLT_LEFT;
			}
			
		}
		if (floating != ALN_NO_FLT) {
			H_ALIGN  align = current->paragraph->Box.TextAlign;
			PARAGRPH par   = add_paragraph (current, 0);
			par->Box.BoxClass  = BC_SINGLE;
			par->Box.HtmlCode  = TAG_IMG;
			par->Box.TextAlign = align;
			if (floating == ALN_CENTER) {
				par->Box.Floating = floating;
			} else {
				par->Box.Floating = floating|FLT_MASK;
			}
			box_frame  (parser, &par->Box.Margin, CSS_MARGIN);
			box_anchor (parser, &par->Box, TRUE);
		
		} else if (get_value (parser, KEY_ALIGN, output, sizeof(output))) {
			if      (stricmp (output, "top")    == 0) v_align = ALN_TOP;
			else if (stricmp (output, "middle") == 0) v_align = ALN_MIDDLE;
			/* else                                   v_align = ALN_BOTTOM */
		}
		word = current->word;
		word->vertical_align = v_align;
		
		if (get_value (parser, KEY_ALT, output, sizeof(output))) {
			char * alt = output;
			while (isspace (*alt)) alt++;
			if (*alt) {
				scan_string_to_16bit (alt, frame->Encoding, &current->text, 
				                      word->font->Base->Mapping);
				font_byType (header_font, 0x0000, 10, word);
				flags |= PF_FONT;
			}
		}
		
		if (get_value (parser, KEY_SRC, img_file, sizeof(img_file))) {
			char * p = url_correct (img_file);

			if (*p && *p != '#') {
				BOOL  skip = FALSE;
				char * dst = p;
				do if ((*dst = *(p)) > ' ' || skip) {
					skip |= (*(dst++) == '#');
				} while (*(p++));
			}
		}
		new_image (frame, current, img_file, frame->BaseHref,
		           get_value_size (parser, KEY_WIDTH),
		           get_value_size (parser, KEY_HEIGHT),
		           get_value_size (parser, KEY_VSPACE),
		           get_value_size (parser, KEY_HSPACE));
		font_switch (current->word->font, NULL);
		
		new_word (current, TRUE);
		current->word->attr           = word_attr;
		current->word->word_height    = word_height;
		current->word->word_tail_drop = word_tail_drop;
		current->word->vertical_align = word_v_align;
	
		if (get_value (parser, KEY_USEMAP, output, sizeof(output))) {
			IMAGEMAP map;
			char   * p = output;
			while (*p == '#') p++;
			if (*p && (map = create_imagemap(&frame->MapList, p, FALSE)) != NULL) {
				if (word->link && word->link->start == word) {
					word->link->start = current->word;
				}
				word->link       = &map->Link;
				word->image->map = map;
			}
		}
		
		if (floating == ALN_NO_FLT) {
			if (!current->nowrap) {
				if (current->prev_wrd) {
					current->prev_wrd->wrap = TRUE;
				}
				current->word->wrap = TRUE;
			}
			flags &= ~PF_SPACE;
		} else {
			add_paragraph (current, 0);
			current->paragraph->Box.TextAlign = current->prev_par->Box.TextAlign;
			flags |= PF_SPACE;
		}
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Line BReak
*/
static UWORD
render_BR_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		char output[10];
		TEXTBUFF current = &parser->Current;
		L_BRK    clear   = BRK_LN;
		BOOL     css_ext = FALSE;
	
		if (get_value (parser, KEY_CLEAR, output, sizeof(output)) ||
		    get_value (parser, CSS_CLEAR, output, sizeof(output))) {
			if      (stricmp (output, "right") == 0) clear = BRK_RIGHT;
			else if (stricmp (output, "left")  == 0) clear = BRK_LEFT;
			else if (stricmp (output, "all")   == 0) clear = BRK_ALL;
			else if (stricmp (output, "both")  == 0) clear = BRK_ALL; /* CSS */
		} else if (get_value_exists (parser, KEY_CLEAR)) {
			clear = BRK_ALL; /* old style */
		}
		
		if (current->prev_wrd) {
			if (current->prev_wrd->line_brk) {
				css_ext = parser->hasStyle;
			}
			current->prev_wrd->line_brk = clear;
			current->word->vertical_align = ALN_BOTTOM;
		
		} else if (current->prev_par) {
			css_ext = parser->hasStyle;
		}
		
		if (css_ext) {     /* with CSS styles a <br> can be used as a spacer */
			short size = 0; /* instead of an ordinary line break              */
			if (get_value (parser, CSS_FONT_SIZE, output, sizeof(output))
			    && isdigit (output[0])) {
				size = numerical (output, NULL, current->font->Size,
			                     current->word->font->SpaceWidth);
			}
			if (size > 0) {
				if (current->prev_wrd) {
					*(current->text++) = font_Nobrk (current->word->font);
					new_word (current, TRUE);
					current->prev_wrd->word_height    = size;
					current->prev_wrd->word_tail_drop = 0;
					current->prev_wrd->line_brk = clear;
				} else {
					DOMBOX * box = &current->prev_par->Box;
					while (box) {
						if (box->Sibling == &current->paragraph->Box) {
							box->Margin.Bot += size;
							break;
						}
						box = box->Sibling;
					}
					if (!box) {
						current->paragraph->Box.Margin.Top += size;
					}
				}
			}
		}
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Word BReak
*/
static UWORD
render_WBR_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		if (current->text > current->buffer) {
			new_word (current, TRUE);
		}
		current->word->wrap = TRUE;
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * NO BReaking area
*/
static UWORD
render_NOBR_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		current->nowrap++;
	
	} else if (current->nowrap) {
		current->nowrap--;
	}
	
	return flags;
}


/*******************************************************************************
 *
 * Block Level Elements
*/

/*------------------------------------------------------------------------------
 * Headings
*/
static UWORD
render_H_tag (PARSER parser, short step, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	PARAGRPH par     = current->paragraph;
	
	if (flags & PF_START) {
		
		/* Prevent a heading paragraph just behind a <LI> tag.
		 */
		if (!current->lst_stack || !current->lst_stack->ListItem ||
		    current->lst_stack->ListItem->item->next_word->next_word->next_word) {
			par = add_paragraph (current, 2);
			if (current->parentbox->BoxClass == BC_LIST) {
				par->Box.Margin.Lft = current->lst_stack->Hanging;
			}
		}
		
		fontstack_push (current, step);
		fontstack_setType (current, header_font);

		if (parser->hasStyle) {
			if (!current->lst_stack) {
				css_box_styles (parser, &par->Box, current->parentbox->TextAlign);
			}
			css_text_styles (parser, current->font);
		} else {
			par->Box.TextAlign = get_h_align (parser,
			                                  current->parentbox->TextAlign);
		}
		box_anchor (parser, &par->Box, FALSE);
		
		if (!current->font->setBold) {
			fontstack_setBold (current);
		}
		
	} else {
		par = add_paragraph (current, 0);

		par->Box.TextAlign = current->parentbox->TextAlign;

		fontstack_pop (current);
	}
	
	return (flags|PF_SPACE);
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
static UWORD
render_H1_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	flags = render_H_tag (parser, 7, flags);
	parser->Current.paragraph->Box.HtmlCode = TAG_H1;
	return flags;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
static UWORD
render_H2_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	flags = render_H_tag (parser, 6, flags);
	parser->Current.paragraph->Box.HtmlCode = TAG_H2;
	return flags;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
static UWORD
render_H3_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	flags = render_H_tag (parser, 5, flags);
	parser->Current.paragraph->Box.HtmlCode = TAG_H3;
	return flags;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
static UWORD
render_H4_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	flags = render_H_tag (parser, 4, flags);
	parser->Current.paragraph->Box.HtmlCode = TAG_H4;
	return flags;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
static UWORD
render_H5_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	flags = render_H_tag (parser, 3, flags);
	parser->Current.paragraph->Box.HtmlCode = TAG_H5;
	return flags;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
static UWORD
render_H6_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	flags = render_H_tag (parser, 2, flags);
	parser->Current.paragraph->Box.HtmlCode = TAG_H6;
	return flags;
}

/*------------------------------------------------------------------------------
 * Horizontal Ruler
 *
 * Completely reworked:
 * - hr_wrd->space_width:    line width (absolute or negative percent)
 * - hr_wrd->word_height:    vertical distance to the line
 * - hr_wrd->word_tail_drop: line size (height, negative for 'noshade')
 *
 * AltF4 - July 21, 2002
*/
static UWORD
render_HR_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		short size    = get_value_unum   (parser, KEY_SIZE, 0);
		BOOL  noshade = get_value_exists (parser, KEY_NOSHADE);
		DOMBOX * box;
		
		if (size == 0 && (size = get_value_unum (parser, KEY_HEIGHT, 0)) == 0) {
			size = 2;
		} else if (size == 1) {
			noshade = TRUE;
		}
		box = render_hrule (&parser->Current, get_h_align (parser, ALN_CENTER),
		                    get_value_size (parser, KEY_WIDTH), size, !noshade);
		if (noshade || size >= 2) {
			WORD color = get_value_color (parser, KEY_COLOR);
			if (color < 0) {
				color = get_value_color (parser, KEY_BGCOLOR);
			}
			if (color < 0 && noshade) {
				box->Backgnd = G_LBLACK;
			} else if (color != parser->Current.backgnd) {
				box->Backgnd = color;
			}
		}
		flags |= PF_SPACE;
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Paragraph
*/
static UWORD
render_P_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	PARAGRPH par     = current->paragraph;
	UNUSED  (text);
		
	if (flags & PF_START) {
		
		/* Ignore a paragraph start just behind a <LI> tag.
		 * Else valid <LI><P>...</P><P>...</P></LI> looks bad.
		 */
		if (!current->lst_stack || !current->lst_stack->ListItem ||
		    current->lst_stack->ListItem->item->next_word->next_word->next_word) {
			par = add_paragraph (current, 2);
			par->Box.HtmlCode = TAG_P;
			if (current->parentbox->BoxClass == BC_LIST) {
				par->Box.Margin.Lft = current->lst_stack->Hanging;
			}
		}

		css_box_styles (parser, &par->Box, current->parentbox->TextAlign);
		box_anchor (parser, &par->Box, FALSE);

		if (!ignore_colours) {
			WORD color = get_value_color (parser, KEY_COLOR);
			if (color >= 0) {
				word_set_color (current, color);
			}
		}
	
	} else {
		par = add_paragraph (current, 2);

		par->Box.TextAlign = current->parentbox->TextAlign;

		if (!ignore_colours) {
			word_set_color (current, current->font->Color);
		}
	}

	current->word->vertical_align = ALN_BOTTOM;
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Center Aligned Text
*/
#define render_C_tag   render_CENTER_tag

static UWORD
render_CENTER_tag (PARSER parser, const char ** text, UWORD flags)
{
	(void)text;
	
	if (flags & PF_START) {
		group_box (parser, TAG_CENTER, ALN_CENTER);
	
	} else {
		leave_box (&parser->Current, TAG_CENTER);
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Quoted Passage
 *
 * A block quotation is a special paragraph, that consists of citated text.
 * A HTML browser can indent, surround with quotation marks, or use italic for
 * this paragraph.
 * http://purl.org/ISO+IEC.15445/Users-Guide.html#blockquote
*/
static UWORD
render_BLOCKQUOTE_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	add_paragraph (current, 2);
	
	if (flags & PF_START) {
		char output[100];
		DOMBOX * box = group_box (parser, TAG_BLOCKQUOTE, ALN_LEFT);
		box->Margin.Lft = box->Margin.Rgt = list_indent (2);

		/* BUG: TITLE support not ready.  This try leads to another problem:
		 * We need the entity and encoding conversion as a separate function.
		 * Or we do it complete before parsing.  But then PLAINTEXT is not
		 * possible.  It seems, this is this the reason, why it's deprecated.
		*/
		if (get_value (parser, KEY_TITLE, output, sizeof(output))) {
			word_set_bold (current, TRUE);
			font_byType (-1, -1, -1, current->word);
			scan_string_to_16bit (output, parser->Frame->Encoding, &current->text, 
				                   current->word->font->Base->Mapping);
			add_paragraph(current, 1);
			word_set_bold (current, FALSE);
			flags |= PF_FONT;
		}
	
	} else {
		leave_box (current, TAG_BLOCKQUOTE);
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Document Divisions
 *
 * implemented as a table with one singe cell
*/
static UWORD
render_DIV_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED  (text);
	
	if (flags & PF_START) {
		group_box (parser, TAG_DIV, ALN_LEFT);
		
	} else {
		DOMBOX * box = leave_box (&parser->Current, TAG_DIV);
		DOMBOX * cld = (box && box->Floating == ALN_NO_FLT &&
		                box->ChildBeg == box->ChildEnd ? box->ChildBeg : NULL);
		if (cld && (cld->Floating == (FLT_LEFT) || cld->Floating == (FLT_RIGHT))
		        && dombox_MinWidth (cld) == dombox_MaxWidth (cld)) {
			box->SetWidth = dombox_MinWidth (box);
			box->Floating = cld->Floating;
		}
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Preformatted Text
*/
static UWORD
render_PRE_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (flags & PF_START) {
		add_paragraph (current, 2)->Box.HtmlCode = TAG_PRE;
		current->paragraph->Box.TextAlign = ALN_LEFT;
		word_set_font (current, pre_font);
		if (get_value_unum (parser, KEY_WIDTH, 80) > 80) {
			TAsetCondns (current->word->attr, TRUE);
		}
		flags |= PF_PRE;
	} else {
		word_set_font (current, current->font->Type);
		TAsetCondns (current->word->attr, FALSE);
		flags &= ~PF_PRE;
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Plain Text
*/
static UWORD
render_PLAINTEXT_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		TEXTBUFF current = &parser->Current;
		add_paragraph (current, 2)->Box.HtmlCode = TAG_PLAINTEXT;
		/* from now on plain text, never ending */
/*		parse_text (*text, frame);*/
		*text = strchr (*text, '\0');
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Listing Text
*/
static UWORD
render_LISTING_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (flags & PF_START) {
		add_paragraph (current, 2)->Box.HtmlCode = TAG_LISTING;
		current->paragraph->Box.TextAlign = ALN_LEFT;
		word_set_font (current, pre_font);
		TAsetCondns (current->word->attr, TRUE);
		flags |= PF_PRE;
	} else {
		add_paragraph (current, 2);
		word_set_font (current, current->font->Type);
		TAsetCondns (current->word->attr, FALSE);
		flags &= ~PF_PRE;
	}
	
	return (flags|PF_SPACE);
}


/*******************************************************************************
 *
 * Lists
*/

/*----------------------------------------------------------------------------*/
static BULLET
list_bullet (PARSER parser, BULLET dflt)
{
	char buf[22];
	int  bullet = -1;
	
	if (get_value (parser, CSS_LIST_STYLE_TYPE, buf, sizeof(buf))
	    && strcmp (buf, "none") == 0) {
		bullet = LT_NONE;
	
	 } else if (dflt >= LT_DECIMAL) {
		if (buf[0]) {
			if      (stricmp (buf, "decimal")     == 0) bullet = LT_DECIMAL;
			else if (stricmp (buf, "lower-alpha") == 0) bullet = LT_L_ALPHA;
			else if (stricmp (buf, "upper-alpha") == 0) bullet = LT_U_ALPHA;
			else if (stricmp (buf, "lower-roman") == 0) bullet = LT_L_ROMAN;
			else if (stricmp (buf, "upper-roman") == 0) bullet = LT_U_ROMAN;
		}
		if (bullet < 0) switch (get_value_char (parser, KEY_TYPE)) {
			case 'a': bullet = LT_L_ALPHA; break;
			case 'A': bullet = LT_U_ALPHA; break;
			case 'i': bullet = LT_L_ROMAN; break;
			case 'I': bullet = LT_U_ROMAN; break;
			case '1': bullet = LT_DECIMAL; break;
			default:  bullet = dflt;
		}
	
	} else {
		if (buf[0]) {
			if      (stricmp (buf, "disc")   == 0) bullet = LT_DISC;
			else if (stricmp (buf, "square") == 0) bullet = LT_SQUARE;
			else if (stricmp (buf, "circle") == 0) bullet = LT_CIRCLE;
		}
		if (bullet < 0) switch (toupper(get_value_char (parser, KEY_TYPE))) {
			case 'D': bullet = LT_DISC;   break;
			case 'S': bullet = LT_SQUARE; break;
			case 'C': bullet = LT_CIRCLE; break;
			default:  bullet = dflt;
		}
	}
	return bullet;
}

/*------------------------------------------------------------------------------
 * Ordered List
*/
static UWORD
render_OL_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		short  counter = get_value_unum (parser, KEY_START, 1);
		BULLET bullet  = list_bullet    (parser, LT_DECIMAL);
		list_start (current, (bullet ? bullet : LT_DECIMAL), counter, TAG_OL);
		css_box_styles  (parser, current->parentbox, ALN_LEFT);
		css_text_styles (parser, current->font);
		box_anchor (parser, current->parentbox, TRUE);
	
	} else if (current->lst_stack) {
		list_finish (current);
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Unordered List
*/
static UWORD
render_UL_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		BULLET bullet = (current->lst_stack
		                 ? current->lst_stack->BulletStyle %3 +1 : LT_DISC);
		list_start (current, list_bullet (parser, bullet), 0, TAG_UL);
		css_box_styles  (parser, current->parentbox, ALN_LEFT);
		css_text_styles (parser, current->font);
		box_anchor (parser, current->parentbox, TRUE);
	
	} else if (current->lst_stack) {
		list_finish (current);
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Menu List
 *
 * it's also possible that we could map this onto render_UL_tag
 * baldrick - December 13, 2001
*/
static UWORD
render_MENU_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		list_start (current, LT_DISC, 0, TAG_MENU);
		css_box_styles  (parser, current->parentbox, ALN_LEFT);
		css_text_styles (parser, current->font);
		box_anchor (parser, current->parentbox, TRUE);
	
	} else if (current->lst_stack) {
		list_finish (current);
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Directory List
*/
#define render_DIR_tag   render_MENU_tag

/*------------------------------------------------------------------------------
 * List Item
*/
static UWORD
render_LI_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (!current->lst_stack) {
		if (flags & PF_START) {
			if (current->prev_wrd) {
				current->prev_wrd->line_brk = BRK_LN;
			}
		}
	
	} else if (flags & PF_START) {
		short  counter = get_value_unum (parser, KEY_VALUE,
		                                 current->lst_stack->Counter);
		BULLET bullet = list_bullet (parser, current->lst_stack->BulletStyle);
		list_marker (current, bullet, counter);
		if (current->lst_stack->FontStk != current->font) {
			fontstack_pop (current);
		}
		if (parser->hasStyle) {
			fontstack_push (current, -1);
			css_box_styles  (parser, &current->paragraph->Box, ALN_LEFT);
			css_text_styles (parser, current->font);
		}
		box_anchor (parser, &current->paragraph->Box, TRUE);
		flags |= PF_FONT;
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Definition List
*/
static UWORD
render_DL_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	if (flags & PF_START) {
		list_start (current, LT_NONE, 0, TAG_DL);
		if (parser->hasStyle) {
			css_box_styles  (parser, current->parentbox, ALN_LEFT);
			css_text_styles (parser, current->font);
		}
		box_anchor (parser, current->parentbox, TRUE);
	
	} else if (current->lst_stack) {
		list_finish (current);
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Definition Term
*/
static UWORD
render_DT_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);

	if (flags & PF_START && current->lst_stack){
		PARAGRPH paragraph = add_paragraph (current, 0);
		paragraph->Box.TextAlign  = ALN_LEFT;
		paragraph->Box.Margin.Lft = 0;
		if (current->lst_stack->FontStk != current->font) {
			fontstack_pop (current);
		}
		if (parser->hasStyle) {
			fontstack_push (current, -1);
			css_box_styles  (parser, &paragraph->Box, ALN_LEFT);
			css_text_styles (parser, current->font);
		}
		box_anchor (parser, &current->paragraph->Box, TRUE);
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Definition Text
*/
static UWORD
render_DD_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);

	if (flags & PF_START && current->lst_stack){
		PARAGRPH paragraph = add_paragraph (current, 0);
		paragraph->Box.TextAlign  = ALN_LEFT;
		paragraph->Box.Margin.Lft = current->lst_stack->Hanging;
		if (current->lst_stack->FontStk != current->font) {
			fontstack_pop (current);
		}
		if (parser->hasStyle) {
			fontstack_push (current, -1);
			css_box_styles  (parser, &paragraph->Box, ALN_LEFT);
			css_text_styles (parser, current->font);
		}
		box_anchor (parser, &current->paragraph->Box, TRUE);
	}
	return (flags|PF_SPACE);
}


/*******************************************************************************
 *
 * Tables
*/

/*------------------------------------------------------------------------------
 * Start Or End
*/
static UWORD
render_TABLE_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		WORD    border   = get_value_unum (parser, KEY_BORDER, -1);
		WORD    padding  = get_value_unum (parser, KEY_CELLPADDING, -1);
		H_ALIGN floating = get_h_align    (parser, ALN_NO_FLT);
		WORD    height   = 0;
		WORD    width    = 0;
		WORD    min_wid  = 0;

		if (border < 0) {
			border = (get_value_exists (parser, KEY_BORDER) ? 1 : 0);
		}
		if (padding < 0) {
			padding = get_value_unum (parser, CSS_PADDING, 1);
		}
		if (floating != ALN_NO_FLT) {
			if (floating != ALN_CENTER) floating |= FLT_MASK;
		} else if (get_v_align (parser, -1) == ALN_MIDDLE) {
			floating = ALN_CENTER; /* patch for invalid key value */
		} else if (parser->Current.paragraph->Box.TextAlign == ALN_RIGHT ||
			        parser->Current.paragraph->Box.TextAlign == ALN_CENTER) {
			floating = parser->Current.paragraph->Box.TextAlign;
		} else if (parser->Current.parentbox->TextAlign == ALN_RIGHT ||
			        parser->Current.parentbox->TextAlign == ALN_CENTER) {
			floating = parser->Current.parentbox->TextAlign;
		} else {
			floating = ALN_LEFT;
		}
		if (parser->hasStyle) {
			short em = parser->Current.word->font->Ascend;
			short ex = parser->Current.word->font->SpaceWidth;
			char  out[100];
			short val;
			if (get_value (parser, KEY_HEIGHT, out, sizeof(out))
			    && (val = numerical (out, NULL, em, ex)) >= 0) {
				height = val;
			}
			if (get_value (parser, KEY_WIDTH, out, sizeof(out))
			    && (val = numerical (out, NULL, em, ex)) >= 0) {
				width = val;
			}
			if (get_value (parser, CSS_MIN_WIDTH, out, sizeof(out))
			    && (val = numerical (out, NULL, em, ex)) >= 0) {
				min_wid = val;
			}
		}
		if (!height) {
			height = get_value_size  (parser, KEY_HEIGHT);
		}
		if (!width) {
			width = get_value_size  (parser, KEY_WIDTH);
		}
		table_start (parser,
		             get_value_color (parser, KEY_BGCOLOR), floating,
		             height, width, min_wid,
		             get_value_unum  (parser, KEY_CELLSPACING, 2),
		             padding, border);
		if (parser->hasStyle) {
			DOMBOX * box = parser->Current.parentbox->ChildEnd;
			box_frame (parser, &box->Margin, CSS_MARGIN);
			if (box->BorderWidth) {
				short color = get_value_color (parser, CSS_BORDER_COLOR);
				if (color >= 0) box->BorderColor = color;
			}
		}
		box_anchor (parser, parser->Current.parentbox->ChildEnd, TRUE);
	
	} else {
		TEXTBUFF current = &parser->Current;
		if (current->tbl_stack) {
			table_finish (parser);
			font_switch (current->word->font, NULL);
		}
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Table Row
*/
static UWORD
render_TR_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (current->tbl_stack) {
		if (flags & PF_START) {
			table_row (current,
			           get_value_color (parser, KEY_BGCOLOR),
			           get_h_align     (parser, ALN_LEFT), 
			           get_v_align     (parser, ALN_MIDDLE),
			           get_value_size  (parser, KEY_HEIGHT), TRUE);
		} else {
			table_row (current, -1,0,0,0, FALSE);
		}
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Table Data Cell
*/
static UWORD
render_TD_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START && current->tbl_stack) {
		table_cell (parser,
		            get_value_color (parser, KEY_BGCOLOR),
			         get_h_align     (parser, current->tbl_stack->AlignH),
			         get_v_align     (parser, current->tbl_stack->AlignV),
			         get_value_size  (parser, KEY_HEIGHT),
			         get_value_size  (parser, KEY_WIDTH),
			         get_value_unum  (parser, KEY_ROWSPAN, 1),
			         get_value_unum  (parser, KEY_COLSPAN, 1));
		current->nowrap = get_value_exists (parser, KEY_NOWRAP);
		current->parentbox->HtmlCode = TAG_TD;
		box_anchor (parser, current->parentbox, TRUE);
		flags |= PF_FONT;
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Table Header Cell
*/
static UWORD
render_TH_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START && current->tbl_stack) {
		table_cell (parser,
		            get_value_color (parser, KEY_BGCOLOR),
			         get_h_align     (parser, ALN_CENTER),
			         get_v_align     (parser, current->tbl_stack->AlignV),
			         get_value_size  (parser, KEY_HEIGHT),
			         get_value_size  (parser, KEY_WIDTH),
			         get_value_unum  (parser, KEY_ROWSPAN, 1),
			         get_value_unum  (parser, KEY_COLSPAN, 1));
		current->nowrap = get_value_exists (parser, KEY_NOWRAP);
		fontstack_setBold (current);
		current->parentbox->HtmlCode = TAG_TH;
		box_anchor (parser, current->parentbox, TRUE);
		flags |= PF_FONT;
	}
	return (flags|PF_SPACE);
}


/*******************************************************************************
 *
 * Fill-out Formulars
*/

/*------------------------------------------------------------------------------
 * Input
*/
static UWORD
render_FORM_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED  (text);
	
	 if (current->form) {
		form_finish (current);
	}
	if (flags & PF_START) {
		char   output[10];
		char * method = (get_value (parser, KEY_METHOD, output, sizeof(output))
		                 ? output : NULL);
		current->form = new_form (parser->Frame,
		                          get_value_str (parser, KEY_TARGET),
		                          get_value_str (parser, KEY_ACTION), method);
	}
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Input
*/
static UWORD
render_INPUT_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		if (new_input (parser)) {
			flags |= PF_FONT;
			flags &= ~PF_SPACE;
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Text Area
*/
static UWORD
render_TEXTAREA_tag (PARSER parser, const char ** text, UWORD flags)
{
	if (flags & PF_START) {
		const char * beg = *text, * end;
		UWORD       rows = 1;
		while (isspace (*beg)) {
			beg++;
		}
		end = beg;
		while (*end && *end != '<') {
			if (*(end++) == '\n') rows++;
		}
		if (new_tarea (parser, beg, end, rows)) {
			*text =  end;
			flags |= PF_FONT;
			flags &= ~PF_SPACE;
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Selection List
*/
static UWORD
render_SELECT_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		char  name[100];
		get_value (parser, KEY_NAME, name, sizeof(name));
		if (form_selct (current, name, get_value_unum (parser, KEY_SIZE, 0),
		                get_value_exists (parser, KEY_DISABLED))) {
			flags |= (PF_FONT|PF_SPACE);
		}
	} else if (current->form) {
		selct_finish (current);
		flags &= ~PF_SPACE;
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Selection Item
*/
static UWORD
render_OPTION_tag (PARSER parser, const char ** text, UWORD flags)
{
	if (flags & PF_START) {
		ENCODING encoding = parser->Frame->Encoding;
		BOOL     disabled = get_value_exists (parser, KEY_DISABLED);
		BOOL     selected = get_value_exists (parser, KEY_SELECTED);
		char   * value    = get_value_str    (parser, KEY_VALUE);
		char     out[90];
		*text = enc_to_sys (out, sizeof(out), *text, encoding, TAG_OPTION, FALSE);
		selct_option (&parser->Current, out, disabled, encoding, value, selected);
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Selection Submenu
*/
static UWORD
render_OPTGROUP_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START) {
		char label[90], out[90];
		if (get_value (parser, KEY_LABEL, label, sizeof(label))) {
			ENCODING enc = parser->Frame->Encoding;
			enc_to_sys (out, sizeof(out), label, enc, TAG_OPTGROUP, TRUE);
			selct_option (&parser->Current, out, TRUE, enc, NULL, FALSE);
		}
	}
	return (flags|PF_SPACE);
}


/*==============================================================================
 * The HTML Parsing Routine
 *
 * AltF4 - Feb. 04, 2002: renamed to parse_html() because that matches more its
 *                        functionality, corresponding to parse_text().
 */
int
parse_html (void * arg, long invalidated)
{
	typedef UWORD (*RENDER)(PARSER, const char **, UWORD);
	
	time_t start_clock = clock();
	
	PARSER       parser    = arg;
	const char * symbol    = parser->Loader->Data;
	FRAME        frame     = parser->Frame;
	TEXTBUFF     current   = &parser->Current;
	WCHAR      * watermark = parser->Watermark;
	ENCODER_W    encoder;
	UWORD        flags;
	BOOL         linetoolong;
	
	if (invalidated || !symbol) {
		delete_parser (parser);
		return FALSE;
	}
	if (parser->ResumeErr == 2/*EBUSY*/ || setjmp (resume_jbuf)) {
		return -2; /* JOB_NOOP */
	}
	symbol = parser->ResumePtr;
	if (parser->ResumeFnc) {
		(*(RENDER)parser->ResumeFnc)(parser, &symbol, PF_START);
		parser_resumed (parser);
	}
	font_switch (current->word->font, NULL);
	flags       = PF_SPACE; /* skip leading spaces */
	linetoolong = FALSE;    /* "line too long" error printed? */
	encoder     = encoder_word (frame->Encoding,
	                            current->word->font->Base->Mapping);
	while (*symbol != '\0')
	{
		if (*symbol == '<')
		{
			static RENDER render[] = {
				NULL,
				#undef SMALL /* prevent an error because this
				              * is already defined in gem.h   */
				#define render_FRAME_tag NULL /* not needed */
				/* now create the function table */
				#define __TAG_ITEM(t)   render_##t##_tag
				#include "token.h"
			};
			HTMLTAG  tag;
			TEXTATTR attrib = current->word->attr;
			if (*(++symbol) != '/') {
				flags |=  PF_START;
			} else {
				flags &= ~PF_START;
				symbol++;
			}
			
			if (current->text > current->buffer) {
				new_word (current, TRUE);
			}
			tag = parse_tag ((flags & PF_START ? parser : NULL), &symbol);
			if (tag && render[tag]) {
				flags = (render[tag])(parser, &symbol, flags);
			}
			if (flags & PF_ENCDNG) {
				encoder = encoder_word (frame->Encoding,
				                        current->word->font->Base->Mapping);
				flags &= ~PF_ENCDNG;
			}
			if (flags & PF_FONT) {
				flags &= ~PF_FONT;
				attrib.packed = 0x300uL; /* force font setting */
			} else {
				attrib.packed ^= current->word->attr.packed;
				attrib.packed &= (TA_SizeMASK|TA_FaceMASK);
			}
			if (attrib.packed) {
				font_byType (-1, -1, -1, current->word);
				encoder = encoder_word (frame->Encoding,
				                        current->word->font->Base->Mapping);
			}

			continue;
		} /* end if (*symbol == '<') */

		switch (*symbol)
		{
			case '&':
				if (current->text >= watermark) {
					goto line_too_long;
				}
				current->text = scan_namedchar (&symbol, current->text, TRUE,
				                                current->word->font->Base->Mapping);
				flags &= ~PF_SPACE;
				break;
			
			case ' ':
				if (flags & PF_PRE) {
					if (current->text >= watermark) {
						goto line_too_long;
					}
					*(current->text++) = font_Nobrk (current->word->font);
					symbol++;
					break;
				}
				goto white_space;
			
			case 9:  /* HT HORIZONTAL TABULATION */
				if (flags & PF_PRE) {
					if (current->text >= watermark -8) {
						goto line_too_long;
					}
					do {
						*(current->text++) = font_Nobrk (current->word->font);
					} while (((current->text - current->buffer) & 7) != 0);
					symbol++;
					break;
				}
				goto white_space;
			
			case 13: /* CR CARRIAGE RETURN */
				if (*(symbol + 1) == 10)  /* HTTP, TOS, or DOS text file */
					symbol++;
				/* else Macintosh text file */
			case 10: /* LF LINE FEED */
			case 11: /* VT VERTICAL TABULATION */
			case 12: /* FF FORM FEED */
				if (flags & PF_PRE) {
					/* BUG: empty paragrahps were ignored otherwise */
					*(current->text++) = font_Nobrk (current->word->font);
					add_paragraph(current, 0);
					symbol++;
					break;
				}
				/* else fall through */
				
			white_space: /* if (!(flags & PF_PRE)) */
				if (!(flags & PF_SPACE)) {
					if (current->text > current->buffer) {
						new_word (current, TRUE);
					}
					*(current->text++) = font_Space (current->word->font);
					flags |= PF_SPACE;
				}
				while (isspace (*(++symbol)));
				break;
			
			default:
				if (current->text < watermark) {
					current->text = (*encoder)(&symbol, current->text);
					flags &= ~PF_SPACE;
					break;
				}
			
			line_too_long:
				if (linetoolong == FALSE) {
					errprintf("parse_html(): Line too long in '%s'!\n",
					          frame->Location->File);
					linetoolong = TRUE;
				}
				flags &= ~PF_SPACE;
				new_word (current, TRUE);
		}
	} /* end while (*symbol != '\0') */
	
	logprintf(LOG_BLUE, "%ld ms for '%s%s'\n",
	          (clock() - start_clock) * 1000 / CLK_TCK,
	          location_Path (frame->Location, NULL), frame->Location->File);

	delete_parser (parser);
		
	return FALSE;
}


/*==============================================================================
 * Parse Plain Text
 *
 * this is the text parsing routine
 * for use with non formatted text files
 *
 * Nothing exciting just maps lines to paragraphs
 */

int
parse_plain (void * arg, long invalidated)
{
	time_t start_clock = clock();
	
	PARSER       parser    = arg;
	const char * symbol    = parser->Loader->Data;
	FRAME        frame     = parser->Frame;
	TEXTBUFF     current   = &parser->Current;
	WCHAR      * watermark = parser->Watermark;
	ENCODER_W    encoder   = encoder_word (frame->Encoding,
	                                       current->word->font->Base->Mapping);
	WORD         linefeed  = current->word->word_height
	                       + current->word->word_tail_drop;
	BOOL linetoolong = FALSE;  /* "line too long" error printed? */

	if (invalidated || !symbol) {
		delete_parser (parser);
			
		return FALSE;
	}
	
	while (*symbol != '\0') {
	
		switch (*symbol) {
		
			case 9: /* HT HORIZONTAL TABULATION */
				if (current->text < watermark) {
					size_t pos = (current->text - current->buffer) /2;
					UWORD  num = 4 - ((UWORD)pos & 0x0003);
					while (num--) {
						*(current->text++) = font_Nobrk (current->word->font);
					}
					symbol++;
					break;
				}
				goto line_too_long;
			
			default:
				if (current->text < watermark) {
					if (*symbol <= 32 || *symbol == 127) {
						*(current->text++) = font_Nobrk (current->word->font);
						symbol++;
					} else {
						current->text = (*encoder)(&symbol, current->text);
					}
					break;
				}
			line_too_long:
				if (!linetoolong) {
					errprintf ("parse_text(): Line too long in '%s'!\n",
					           frame->Location->File);
					linetoolong = TRUE;
				}
				goto line_break;
			
			case 13: /* CR CARRIAGE RETURN */
				if (*(symbol +1) == 10)  /* HTTP, TOS, or DOS text file */
					symbol++;
				/* else Macintosh text file */
			case 10: /* LF LINE FEED */
			case 11: /* VT VERTICAL TABULATION */
			case 12: /* FF FORM FEED */
				symbol++;
			line_break:
				if (current->text > current->buffer) {
					/* new text line for the current paragraph */
					current->word->line_brk = BRK_LN;
					new_word (current, TRUE);
				
				} else if (current->prev_wrd) {
					/* empty line after some text, creates a new paragraph */
					add_paragraph(current, 0);
					current->prev_par->Box.Margin.Bot += linefeed;
				
				} else if (current->prev_par) {
					/* subsequent empty lines spread the gap between the previous *
					 * paragraph with text and the actual empty paragraphp        */
					current->prev_par->Box.Margin.Bot += linefeed;
				
				} else {
					/* empty lines at the very beginning of the text increase the *
					 * top margin of the yet still empty only paragraph           */
					current->paragraph->Box.Margin.Top += linefeed;
				}
		}
	}
	
	logprintf (LOG_BLUE, "%ld ms for '%s%s'\n",
	           (clock() - start_clock) * 1000 / CLK_TCK,
	          location_Path (frame->Location, NULL), frame->Location->File);

	delete_parser (parser);
		
	return FALSE;
}


/*==============================================================================
 * Create a page for just a single image.
 *
 */
int
parse_image (void * arg, long invalidated)
{
	PARSER   parser  = arg;
	FRAME    frame   = parser->Frame;
	TEXTBUFF current = &parser->Current;
	LOCATION loc;
	
	if (invalidated) {
		delete_parser (parser);
			
		return FALSE;
	}
	
	font_byType (normal_font, 0x0000, -1, current->word);
	
	if (parser->Loader->Referer) {
		loc = frame->Location;
		frame->Location = location_share (parser->Loader->Referer);
	} else {
		loc = location_share (frame->Location);
	}
	new_image (frame, current, NULL, loc, 0,0, 0,0);
	free_location (&loc);

	delete_parser (parser);
		
	return FALSE;
}


/*============================================================================*/
DOMBOX *
render_hrule (TEXTBUFF current, H_ALIGN align, short w, short size, BOOL shade)
{
	PARAGRPH par = add_paragraph(current, 0);
	DOMBOX * box = dombox_ctor (malloc (sizeof (DOMBOX)),
	                            current->parentbox, BC_SINGLE);
	box->HtmlCode = TAG_HR;
	
	if (shade) {
		box->BorderWidth = 1;
		box->BorderColor = -2;
		size -= 2;
	}
	box->Padding.Top = size;
	box->Padding.Lft = 3; /* minimum width */
	
	if (par->item->word_height > size) {
		box->Margin.Top = box->Margin.Bot = (par->item->word_height - size +1) /2;
	} else {
		box->Margin.Top = box->Margin.Bot = par->item->word_tail_drop;
	}
	box->Margin.Lft = box->Margin.Rgt = 1;
	
	box->SetWidth = w;
	box->Floating = align;
	
	dombox_reorder (&par->Box, box);
	
	return box;
}


/*============================================================================*/
WORDITEM
render_text (TEXTBUFF current, const char * text)
{
	WORDITEM  word    = NULL;
	ENCODER_W encoder = encoder_word (ENCODING_ATARIST,
	                                  current->word->font->Base->Mapping);
	while (*text) {
		if (*text > 32) {
			current->text = (*encoder)(&text, current->text);
		
		} else {
			BOOL  change = FALSE;
			short font   = -1;
			short style  = -1;
			
			switch (*(text++)){
				case ' ':
					if (current->text > current->buffer) {
						text--;
						change = TRUE;
					} else {
						while (*text == ' ') text++;
						*(current->text++) = font_Space (current->word->font);
					}
					break;
				
				case '': /* 0x05, &nbsp; */
					*(current->text++) = font_Nobrk (current->word->font);
					break;
				
				case '': /* 0x03, force new word */
					change = TRUE;
					break;
				
				case '\r': /* line break */
					if (current->text > current->buffer) {
						current->word->line_brk = BRK_LN;
						change = TRUE;
					} else if (current->prev_wrd) {
						current->prev_wrd->line_brk = BRK_LN;
					}
					break;
				
				case '\n': /* new paragraph */
					add_paragraph (current, 2);
					break;
				
				/* 0x10.. 0x12 */
				case '': change = TRUE; font = normal_font; break;
				case '': change = TRUE; font = header_font; break;
				case '': change = TRUE; font = pre_font;    break;
				/* 0x14 .. 0x17 */
				case '': change = TRUE; style = 0x0000;              break;
				case '': change = TRUE; style = FNT_BOLD;            break;
				case '': change = TRUE; style = FNT_ITALIC;          break;
				case '': change = TRUE; style = FNT_BOLD|FNT_ITALIC; break;
			}
			if (change) {
				if (current->text > current->buffer) {
					if (!word) word = current->word;
					new_word (current, TRUE);
				}
				if (font >= 0 || style >= 0) {
					font_byType (font, style, -1, current->word);
					encoder = encoder_word (ENCODING_ATARIST,
					                        current->word->font->Base->Mapping);
				}
			}
		}
	}
	return (word ? word : current->word);
}


/*============================================================================*/
WORDITEM
render_link (TEXTBUFF current, const char * text, const char * url, short color)
{
	WORDITEM word = render_text (current, text);
	word->link = new_url_link (word, strdup (url), TRUE, NULL);
	TAsetUndrln (word->attr, TRUE);
	TA_Color    (word->attr) = color;
	
	return word;
}
