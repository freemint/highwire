/* @(#)highwire/render.c
 */
#if defined (__PUREC__)
# include <tos.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h> 

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


/* parser function flags */
#define PF_NONE   0x0000
#define PF_START  0x0001 /* no slash in tag   */
#define PF_PRE    0x0002 /* preformatted text */
#define PF_SPACE  0x0004 /* ignore spaces     */
#define PF_FONT   0x0008 /* font has changed  */
#define PF_ENCDNG 0x0010 /* charset encoding has changed   */
#define PF_SCRIPT 0x0020 /* <noscrip> inside a script area */


static void
step_push (TEXTBUFF current, WORD step)
{
	current->font_step = add_step       (current->font_step);
	current->font_size = font_step2size (current->font_step, step);
}

static void
step_pop (TEXTBUFF current)
{
	current->font_step = destroy_step   (current->font_step);
	current->font_size = font_step2size (NULL, current->font_step->step);
}


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


/* get_align()
 *
 * retrieves the alignment of a paragraph
 * based on frame->alignment (CENTER) or if it's in a Table
 */

static H_ALIGN
get_align (PARSER parser)
{
	FRAME frame = parser->Frame;
	
	if (frame->Page.Alignment == ALN_CENTER &&
	    parser->Current.paragraph->alignment == ALN_CENTER) {
		return (ALN_CENTER);
	}
	if (parser->Current.tbl_stack && parser->Current.tbl_stack->WorkCell) {
		return (parser->Current.tbl_stack->WorkCell->Content.Alignment);
	}
	return (frame->Page.Alignment);
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
	
	} else {
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
		ENCODER_C encoder = encoder_char (parser->Frame->Encoding);
		char   window_title[128 +5] = "";
		char * title     = window_title;
		char * watermark = window_title + aes_max_window_title_length;
		BOOL   ready     = FALSE;
		BOOL   space     = TRUE;
		const char * symbol = *text;
		
		do switch (*symbol) {
		
			case '\0':
				ready = TRUE;
				break;
			
			case '\t':
			case '\r':
			case '\n':
			case ' ':
				if (!space && title < watermark) {
					const char * sym = " ";
					title = (*encoder)(&sym, title);
					space = TRUE;
				}
				symbol++;
				break;
			
			case '&':
				if (title < watermark) {
					title = scan_namedchar (&symbol, title, FALSE,0);
					space = FALSE;
				} else {
					symbol++;
				}
				break;
			
			case '<':
				if (symbol[1] == '/') {
					const char * sym = symbol + 2;
					if (parse_tag (parser, &sym) == TAG_TITLE) {
						symbol = sym;
						ready  = TRUE;
						break;
					}
				}
			default:
				if (title < watermark) {
					title = (*encoder)(&symbol, title);
					space = FALSE;
				} else {
					symbol++;
				}
		} while (!ready);
		
		if (space) {
			title--;
		}
		if (title > window_title) {
			if (title < watermark) *title     = '\0';
			else                   *watermark = '\0';
			containr_notify (parser->Target, HW_SetTitle, window_title);
		}
		*text = symbol;
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
	LOCATION     base      = frame->Location;
	HTMLTAG tag   = TAG_FRAMESET;
	BOOL    slash = FALSE;
	int     depth = 0;

	if (!container) {
		errprintf ("render_frameset(): NO CONTAINER in '%s'!\n", base->File);
		exit(EXIT_FAILURE);
	} else if (container->Mode) {
		errprintf ("render_frameset(): container not cleared in '%s'!\n", base->File);
		exit(EXIT_FAILURE);
	}

	if (flags & PF_START) do {
	
		if (!slash) {

			/* only work on an empty container.  if not empty there was either a
			 * forgotten </framset> or more tags than defined in the frameset.
			 */
			if (!container->Mode) {

				if (tag == TAG_FRAMESET) {

					char cols[100], rows[100];
					BOOL border = (container->Parent
					               ? container->Parent->Border : TRUE);
					container->Border
					        = (get_value_unum (parser, KEY_FRAMEBORDER, border) > 0);

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
					char output[100];
					char frame_file[HW_PATH_MAX];

					container->Mode   = CNT_FRAME;
					container->Name   = get_value_str (parser, KEY_NAME);
					container->Border = container->Parent->Border;

					if (get_value (parser, KEY_NORESIZE, NULL,0)) {
						container->Resize = FALSE;
					} else {
						container->Resize = TRUE;
					}

					if (get_value (parser, KEY_SCROLLING, output, sizeof(output))) {
						if (stricmp (output, "yes") == 0) {
							container->Scroll = SCROLL_ALWAYS;
						} else if (stricmp (output, "no")  == 0) {
							container->Scroll = SCROLL_NEVER;
						}
					}

					if (get_value (parser, KEY_SRC, frame_file, sizeof(frame_file))) {
						LOADER loader = new_loader_job (frame_file, base, container);
						short margn_w = get_value_unum (parser, KEY_MARGINWIDTH,  -1);
						short margn_h = get_value_unum (parser, KEY_MARGINHEIGHT, -1);
						if (margn_w >= 0 || margn_h >= 0) {
							loader_setParams (loader, 0, margn_w, margn_h);
						}
					} else {
						containr_notify (container, HW_PageStarted, "");
						containr_calculate (container, NULL);
						containr_notify (container,
						                 HW_PageFinished, &container->Area);
					}

					if (container->Sibling) {
						container = container->Sibling;
					}
				}
			} /* endif (!container->Mode) */

		} else if (tag == TAG_FRAMESET) { /* && slash */

			container = container->Parent;

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

	#if 0
	{
		extern void containr_debug (CONTAINR);
		containr_debug (container);
	}
	#endif

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
		parser->Frame->base_target = get_value_str (parser, KEY_TARGET);
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Meta Descriptions
*/
static UWORD
render_META_tag (PARSER parser, const char ** text, UWORD flags)
{
	char output[100];
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
							new_loader_job (url,
							                parser->Frame->Location, parser->Target);
						}
					}
				}
			}
		}
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Script Area
 *
 * skip everything until </STYLE> tag
*/
static UWORD
render_STYLE_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (parser);
	
	if (flags & PF_START) {
		const char * line = *text;
		do {
			BOOL slash;
			while (*line && *(line++) != '<');
			slash = (*line == '/');
			if (slash) line++;
			if (parse_tag (parser, &line) == TAG_STYLE && slash) {
				break;
			}
		} while (*line);
	
		*text = line;
	}
	return flags;
}

/*------------------------------------------------------------------------------
 * Background Sound
 *
 * Processes embedded multi media objects.
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
			new_loader_job (snd_file, parser->Frame->Location, NULL);
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
	
	if (flags & PF_START) {
		FRAME frame = parser->Frame;
		WORD  margin;
		
		if (!ignore_colours)
		{
			WORD color;
	
			if ((color = get_value_color (parser, KEY_TEXT)) >= 0)
			{
				frame->text_colour = parser->Current.font_step->colour = color;
				TA_Color(parser->Current.word->attr)                   = color;
			}
			if ((color = get_value_color (parser, KEY_BGCOLOR)) >= 0)
			{
				frame->Page.Backgnd = parser->Current.backgnd = color;
			}
			if ((color = get_value_color (parser, KEY_LINK)) >= 0)
			{
				frame->link_colour = color;
			}
		}
		
		if ((margin = get_value_unum (parser, CSS_MARGIN, -1)) >= 0) {
			frame->Page.MarginTop = frame->Page.MarginBot =
			frame->Page.MarginLft = frame->Page.MarginRgt = margin;
			
		} else {
			if ((margin = get_value_unum (parser, KEY_MARGINHEIGHT, -1)) >= 0 ||
			    (margin = get_value_unum (parser, KEY_TOPMARGIN, -1)) >= 0) {
				frame->Page.MarginTop = frame->Page.MarginBot = margin;
			}
			if ((margin = get_value_unum (parser, KEY_MARGINWIDTH, -1)) >= 0 ||
			    (margin = get_value_unum (parser, KEY_LEFTMARGIN, -1)) >= 0) {
				frame->Page.MarginLft = frame->Page.MarginRgt = margin;
			}
		}
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
		const char * line = *text, * save = NULL;
		do {
			BOOL    slash;
			HTMLTAG tag;
			while (*line && *(line++) != '<');
			slash = (*line == '/');
			if (slash) line++;
			else       save = line +1;
			tag = parse_tag (parser, &line);
			if (slash) {
				if (tag == TAG_SCRIPT) {
					flags &= ~PF_SCRIPT;
					break;
				}
			} else if (tag == TAG_NOSCRIPT) {
				flags |= PF_SCRIPT;
				break;
			} else {
				line = save;
			}
		} while (*line);
	
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
		TEXTBUFF          current = &parser->Current;
		struct font_step * f_step = current->font_step;
		char output[10];
		
		while (f_step->previous_font_step) {
			f_step = f_step->previous_font_step;
		}
		
		if (get_value (parser, KEY_SIZE, output, sizeof(output)))
		{
			if (*output == '+') {
				if (isdigit(output[1])) {
					f_step->step += output[1]  - '0';
				}
			} else {
				if (*output == '-') {
					if (isdigit(output[1])) {
						f_step->step -= output[1]  - '0';
					}
				} else if (isdigit(output[0])) {
					f_step->step = output[0]  - '0';
				}
				if (f_step->step < 1) f_step->step = 1;
			}
			if (f_step->step > 7) f_step->step = 7;
			
			if (f_step == current->font_step) {
				current->font_size = font_step2size (NULL, f_step->step);
				word_set_point (current, current->font_size);
			}
		}

		if (!ignore_colours) {
			WORD color = get_value_color (parser, KEY_COLOR);

			if (color >= 0) {
				f_step->colour = color;
				parser->Frame->text_colour = current->font_step->colour = color;
				word_set_color (current, color);
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
		step_push (current, current->font_step->step +1);
	} else {
		step_pop (current);
	}
	word_set_point (current, current->font_size);
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
	
	word_set_font (current, (flags & PF_START ? pre_font : normal_font));
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
	UNUSED (text);
	
	if (flags & PF_START) {
		char output[10];
		WORD step = parser->Current.font_step->step;

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
		step_push (&parser->Current, step);

		if (!ignore_colours) {
			WORD color = get_value_color (parser, KEY_COLOR);

			if (color >= 0)
			{
				parser->Current.font_step->colour = color;
				word_set_color (&parser->Current, color);
			}
		}
	
	} else {
		step_pop (&parser->Current);
		word_set_color (&parser->Current, (parser->Current.word->link
		                                  ? parser->Frame->link_colour
		                                  : parser->Current.font_step->colour));
	}
	word_set_point (&parser->Current, parser->Current.font_size);
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
		step_push (current, current->font_step->step -1);
	} else {
		step_pop (current);
	}
	word_set_point (current, current->font_size);
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
		step_push (current, current->font_step->step -1);
		current->word->vertical_align = ALN_BELOW;
	} else {
		step_pop (current);
		current->word->vertical_align = ALN_BOTTOM;
	}
	word_set_point (current, current->font_size);
	
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
		step_push (current, current->font_step->step -1);
		current->word->vertical_align = ALN_ABOVE;
	} else {
		step_pop (current);
		current->word->vertical_align = ALN_BOTTOM;
	}
	word_set_point (current, current->font_size);
	
	return flags;
}

/*------------------------------------------------------------------------------
 * Teletype Text Style
*/
static UWORD
render_TT_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	word_set_font (&parser->Current, (flags & PF_START ? pre_font : normal_font));
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
			char out2[30], * p = output, * amp = strchr (p, '&');
			FRAME frame = parser->Frame;
			
			char * target = get_value_str (parser, KEY_TARGET);
			if (!target && frame->base_target) {
				target = strdup (frame->base_target);
			}
			
			while (amp) { /* get rid of html entities in the url */
				const char * nxt = amp;
				char * end = scan_namedchar (&nxt, out2, TRUE, MAP_UNICODE);
				if (end == out2 +2 && !out2[0] && out2[1] && nxt[-1] == ';') {
					*(amp++) = out2[1];
					if (amp < nxt) {
						strcpy (amp, nxt);
					}
				} else {
					amp++;
				}
				amp = strchr (amp, '&');
			}
			
			while (*p > ' ') { /* remove controll characters from the url */
				p++;
			}
			if (*p) {
				char * dst = p;
				do if ((*dst = *(++p)) > ' ') {
					dst++;
				} while (*p);
			}
			
			if (!word->link) {
				word_set_underline (current, TRUE);
			}
			word_set_color (current, frame->link_colour);

			word->link = new_url_link (word, output, TRUE, target);
			
			if (get_value (parser, KEY_CHARSET, out2, sizeof(out2))) {
				/* ENCODING_WINDOWS1252 as default, which is commen practice. */
				word->link->encoding = scan_encoding(out2, ENCODING_WINDOWS1252);
			}
		}
		else if ((output = get_value_str (parser, KEY_NAME)) != NULL ||
		         (output = get_value_str (parser, KEY_ID))   != NULL)
		{
			word->link = new_url_link (word, output, FALSE, NULL);
			word->link->u.anchor = new_named_location (word->link->address,
			                                          &current->paragraph->Offset);
			*current->anchor = word->link->u.anchor;
			current->anchor  = &word->link->u.anchor->next_location;
			
			/* prevent from getting deleted if empty */
			*(current->text++) = font_Nobrk (word->font);
			new_word (current, TRUE);
			current->word->link = NULL;
			word->word_width = 0;
		}	
	
	} else if (word->link) {
		word_set_color     (current, current->font_step->colour);
		word_set_underline (current, FALSE);
		word->link = NULL;
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
		case LANG_RU:  /* Russian, using as ®I will - he said - go home.¯ with U+2015 HORIZONTAL BAR */
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

/*----------------------------------------------------------------------------*/
#define render_EMBED_tag   render_BGSOUND_tag


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
		H_ALIGN  floating = ALN_NO_FLT;
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
	
		if (get_value (parser, KEY_ALIGN, output, sizeof(output))) {
			if      (stricmp (output, "left")   == 0) floating = ALN_LEFT;
			else if (stricmp (output, "right")  == 0) floating = ALN_RIGHT;
			else if (stricmp (output, "center") == 0) floating = ALN_CENTER;
			
			else if (stricmp (output, "top")    == 0) v_align = ALN_TOP;
			else if (stricmp (output, "middle") == 0) v_align = ALN_MIDDLE;
			/* else                                   v_align = ALN_BOTTOM */
			if (floating != ALN_NO_FLT) {
				add_paragraph (current, 0);
				current->paragraph->paragraph_code = PAR_IMG;
				current->paragraph->floating       = floating;
			}
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
		
		get_value (parser, KEY_SRC, img_file, sizeof(img_file));
		
		new_image (frame, current, img_file, frame->Location,
		           get_value_size (parser, KEY_WIDTH),
		           get_value_size (parser, KEY_HEIGHT),
		           get_value_size (parser, KEY_VSPACE),
		           get_value_size (parser, KEY_HSPACE));
		
		new_word (current, TRUE);
		current->word->attr           = word_attr;
		current->word->word_height    = word_height;
		current->word->word_tail_drop = word_tail_drop;
		current->word->vertical_align = word_v_align;
	
		if (floating == ALN_NO_FLT) {
			flags &= ~PF_SPACE;
		} else {
			add_paragraph (current, 0);
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
	
		if (get_value (parser, KEY_CLEAR, output, sizeof(output))) {
			if      (stricmp (output, "right") == 0) clear = BRK_RIGHT;
			else if (stricmp (output, "left")  == 0) clear = BRK_LEFT;
			else if (stricmp (output, "all")   == 0) clear = BRK_ALL;
		} else if (get_value (parser, KEY_CLEAR, NULL,0)) {
			clear = BRK_ALL; /* old style */
		}
		
		if (current->prev_wrd) {
			current->prev_wrd->line_brk = clear;
			current->word->vertical_align = ALN_BOTTOM;
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
render_H_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	PARAGRPH par     = current->paragraph;
	UNUSED  (text);
	
	if (flags & PF_START) {
		H_ALIGN align = get_align (parser);
		char    buf[8];

		/* Prevent a heading paragraph just behind a <LI> tag.
		 */
		if (!current->lst_stack ||
		     current->lst_stack->Spacer->next_word != current->prev_wrd) {
			par = add_paragraph (current, 1);
		}

		if (get_value (parser, KEY_ALIGN, buf, sizeof(buf))) {
			if      (stricmp (buf, "right")   == 0) align = ALN_RIGHT;
			else if (stricmp (buf, "center")  == 0) align = ALN_CENTER;
			else if (stricmp (buf, "justify") == 0) align = ALN_JUSTIFY;
		}
		par->alignment = align;
		
		step_push (current, 7 - (get_value_char (parser, KEY_H_HEIGHT) - '0'));

		if (!ignore_colours) {
			WORD color = get_value_color (parser, KEY_BGCOLOR);
			if (color >= 0 && color != current->backgnd) {
				current->paragraph->Backgnd = color;
			}
			
			color = get_value_color (parser, KEY_COLOR);
			if (color < 0)	color = current->font_step->colour;
			if (color < 0)	color = parser->Frame->text_colour;
			current->font_step->colour = (color);
		}
		word_set_font (current, header_font);
		word_set_bold (current, TRUE);
	
	} else {
		par = add_paragraph (current, 0);

		par->alignment = get_align (parser);

		step_pop (current);
		word_set_font (current, normal_font);
		word_set_bold (current, FALSE);
	}
	word_set_point (current, current->font_size);
	word_set_color (current, current->font_step->colour);
	
	return (flags|PF_SPACE);
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
		TEXTBUFF current = &parser->Current;
		TEXTATTR attrib  = current->word->attr;
		char output[100];
		
		PARAGRPH hr_par = add_paragraph(current, 0);
		WORDITEM hr_wrd = current->word;
	
		new_word (current, TRUE);
		add_paragraph (current, 0);
	
		hr_par->paragraph_code = PAR_HR;
		hr_par->alignment      = ALN_CENTER;
		if (get_value (parser, KEY_ALIGN, output, sizeof(output))) {
			if      (stricmp (output, "left")  == 0) hr_par->alignment = ALN_LEFT;
			else if (stricmp (output, "right") == 0) hr_par->alignment = ALN_RIGHT;
		}
		
		if ((hr_wrd->word_tail_drop = get_value_unum (parser, KEY_SIZE, 0)) == 0 &&
		    (hr_wrd->word_tail_drop = get_value_unum (parser, KEY_HEIGHT, 0)) == 0) {
			hr_wrd->word_tail_drop = 2;
		} else if (hr_wrd->word_tail_drop > 100) {
			hr_wrd->word_tail_drop = 100;
		}
		hr_wrd->word_height = (hr_wrd->word_height + hr_wrd->word_tail_drop) /2;
		if (get_value (parser, KEY_NOSHADE, NULL,0)) {
			hr_wrd->word_tail_drop = -hr_wrd->word_tail_drop;
		}
		if ((hr_wrd->space_width = get_value_size (parser, KEY_WIDTH)) == 0) {
			hr_wrd->space_width = -1024;
		}
		if (!ignore_colours) {
			WORD color = get_value_color (parser, KEY_BGCOLOR);
			if (color >= 0 && color != current->backgnd) {
				hr_par->Backgnd = color;
			}
			color = get_value_color (parser, KEY_COLOR);
			TA_Color(hr_wrd->attr) = (color                  >= 0 ? color    :
			                          hr_wrd->word_tail_drop <= 1 ? G_LBLACK :
			                          hr_par->Backgnd        >= 0 ? hr_par->Backgnd :
			                                                        current->backgnd);
		} else if (hr_wrd->word_tail_drop <= 1) {
			TA_Color(hr_wrd->attr) = G_BLACK;
		}
		
		current->word->attr = attrib;
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
		H_ALIGN align = get_align (parser);
		char    buf[8];

		/* Ignore a paragraph start just behind a <LI> tag.
		 * Else valid <LI><P>...</P><P>...</P></LI> looks bad.
		 */
		if (!current->lst_stack ||
		     current->lst_stack->Spacer->next_word != current->prev_wrd) {
			par = add_paragraph (current, 1);
		}

		if (get_value (parser, KEY_ALIGN, buf, sizeof(buf))) {
			if      (stricmp (buf, "right")   == 0) align = ALN_RIGHT;
			else if (stricmp (buf, "center")  == 0) align = ALN_CENTER;
			else if (stricmp (buf, "justify") == 0) align = ALN_JUSTIFY;
		}
		par->alignment = align;

		if (!ignore_colours) {
			WORD color = get_value_color (parser, KEY_COLOR);
			if (color >= 0) {
				word_set_color (current, color);
			}
		}
	
	} else {
		par = add_paragraph (current, 1);

		par->alignment = get_align (parser);

		if (!ignore_colours) {
			word_set_color (current, current->font_step->colour);
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
	FRAME frame = parser->Frame;
	UNUSED (text);
	
	add_paragraph (&parser->Current, 0);

	if (flags & PF_START) {
		frame->Page.Alignment                = ALN_CENTER;
		parser->Current.paragraph->alignment = ALN_CENTER;
	} else {
		frame->Page.Alignment                = ALN_LEFT;
		parser->Current.paragraph->alignment = get_align (parser);
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
	
	add_paragraph (current, 1);

	if (flags & PF_START)
	{
		#if 1
		/* BUG: TITLE support not ready.  This try leads to another problem:
		 * We need the entity and encoding conversion as a separate function.
		 * Or we do it complete before parsing.  But then PLAINTEXT is not
		 * possible.  It seems, this is this the reason, why it's deprecated. */
		char output[100];
		#endif

		current->paragraph->alignment = ALN_LEFT;
		current->paragraph->Indent += list_indent (2);
		current->paragraph->Rindent += list_indent (2);

		#if 1
		if (get_value (parser, KEY_TITLE, output, sizeof(output))) {
			word_set_bold (current, TRUE);
			font_byType (-1, -1, -1, current->word);
			scan_string_to_16bit (output, parser->Frame->Encoding, &current->text, 
				                   current->word->font->Base->Mapping);
			add_paragraph(current, 1);
			word_set_bold (current, FALSE);
			flags |= PF_FONT;
		}
		#endif
	}
	else
	{
		current->paragraph->Indent  -= list_indent(2);
		current->paragraph->Rindent -= list_indent(2);
	}
	
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Document Divisions
 *
 * DIV is only partially implemented from my understanding
 * baldrick - December 14, 2001
*/
static UWORD
render_DIV_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START)
	{
		PARAGRPH paragraph = add_paragraph (&parser->Current, 1);

		switch (toupper (get_value_char (parser, KEY_ALIGN)))
		{
			case 'R': paragraph->alignment = ALN_RIGHT;  break;
			case 'C': paragraph->alignment = ALN_CENTER; break;
			case 'L': paragraph->alignment = ALN_LEFT;   break;
		}
	}
	else
	{
		add_paragraph(&parser->Current, 0)->alignment = get_align (parser);
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
		add_paragraph (current, 1);
		current->paragraph->alignment = ALN_LEFT;
		word_set_font (current, pre_font);
		if (get_value_unum (parser, KEY_WIDTH, 80) > 80) {
			TAsetCondns (current->word->attr, TRUE);
		}
		flags |= PF_PRE;
	} else {
		word_set_font (current, normal_font);
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
		add_paragraph (current, 1);
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
		add_paragraph (current, 1);
		current->paragraph->alignment = ALN_LEFT;
		word_set_font (current, pre_font);
		TAsetCondns (current->word->attr, TRUE);
		flags |= PF_PRE;
	} else {
		add_paragraph (current, 1);
		word_set_font (current, normal_font);
		TAsetCondns (current->word->attr, FALSE);
		flags &= ~PF_PRE;
	}
	
	return (flags|PF_SPACE);
}


/*******************************************************************************
 *
 * Lists
*/

/*------------------------------------------------------------------------------
 * Ordered List
*/
static UWORD
render_OL_tag (PARSER parser, const char ** text, UWORD flags)
{
	TEXTBUFF current = &parser->Current;
	UNUSED (text);
	
	if (flags & PF_START) {
		BULLET bullet;
		switch (get_value_char (parser, KEY_TYPE)) {
			case 'a': bullet = alpha; break;
			case 'A': bullet = Alpha; break;
			case 'i': bullet = roman; break;
			case 'I': bullet = Roman; break;
			default:  bullet = Number;
		}
		list_start (current, bullet, get_value_unum (parser, KEY_START, 1));
	
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
		                 ? (current->lst_stack->BulletStyle +1) %3 : disc);
		switch (toupper (get_value_char (parser, KEY_TYPE))) {
			case 'S': bullet = square; break;
			case 'C': bullet = circle; break;
			case 'D': bullet = disc;   break;
		}
		list_start (current, bullet, 0);
	
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
		list_start (current, disc, 0);
	
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
		BULLET bullet;
		switch (get_value_char (parser, KEY_TYPE)) {
			case 'a':           bullet = alpha;  break;
			case 'A':           bullet = Alpha;  break;
			case 'i':           bullet = roman;  break;
			case 'I':           bullet = Roman;  break;
			case '1':           bullet = Number; break;
			case 'd': case 'D': bullet = disc;   break;
			case 'c': case 'C': bullet = circle; break;
			case 's': case 'S': bullet = square; break;
			default:            bullet = current->lst_stack->BulletStyle;
		}
		list_marker (current, bullet, counter);
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
		list_start (current, -1,0);
	
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
	UNUSED (text);

	if (flags & PF_START && parser->Current.lst_stack){
		PARAGRPH paragraph = add_paragraph (&parser->Current, 0);
		paragraph->alignment = ALN_LEFT;
		paragraph->Indent = parser->Current.lst_stack->Indent;
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Definition Text
*/
static UWORD
render_DD_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);

	if (flags & PF_START && parser->Current.lst_stack){
		PARAGRPH paragraph = add_paragraph (&parser->Current, 0);
		paragraph->alignment = ALN_LEFT;
		paragraph->Indent = parser->Current.lst_stack->Indent
		                  + parser->Current.lst_stack->Hanging;
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
		table_start (parser);
	} else if (parser->Current.tbl_stack) {
		table_finish (parser);
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Table Row
*/
static UWORD
render_TR_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (parser->Current.tbl_stack) {
		table_row (parser, (flags & PF_START));
	}
	return (flags|PF_SPACE);
}

/*------------------------------------------------------------------------------
 * Table Data Cell
*/
static UWORD
render_TD_tag (PARSER parser, const char ** text, UWORD flags)
{
	UNUSED (text);
	
	if (flags & PF_START && parser->Current.tbl_stack) {
		table_cell (parser, FALSE);
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
	UNUSED (text);
	
	if (flags & PF_START && parser->Current.tbl_stack) {
		table_cell (parser, TRUE);
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
	
	if (flags & PF_START) {
		char   output[10];
		char * method = (get_value (parser, KEY_METHOD, output, sizeof(output))
		                 ? output : NULL);
		current->form = new_form (parser->Frame,
		                          get_value_str (parser, KEY_TARGET),
		                          get_value_str (parser, KEY_ACTION), method);
	} else {
		current->form = NULL;
	}
	
	return flags;
}

#define render_OPTION_tag   NULL
#define render_SELECT_tag   NULL
#define render_TEXTAREA_tag NULL

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


/*==============================================================================
 * The HTML Parsing Routine
 *
 * AltF4 - Feb. 04, 2002: renamed to parse_html() because that matches more its
 *                        functionality, corresponding to parse_text().
 */
BOOL
parse_html (void * arg, long invalidated)
{
	time_t start_clock = clock();
	
	PARSER       parser    = arg;
	const char * symbol    = parser->Loader->Data;
	FRAME        frame     = parser->Frame;
	TEXTBUFF     current   = &parser->Current;
	WCHAR      * watermark = parser->Watermark;
	ENCODER_W    encoder   = encoder_word (frame->Encoding,
	                                       current->word->font->Base->Mapping);
	BOOL space_found = TRUE;  /* skip leading spaces */
	BOOL in_pre      = FALSE;
	BOOL linetoolong = FALSE;  /* "line too long" error printed? */
	
	if (invalidated || !symbol) {
		delete_parser (parser);
			
		return FALSE;
	}
	
	while (*symbol != '\0')
	{
		if (*symbol == '<')
		{
			typedef UWORD (*RENDER)(PARSER, const char **, UWORD);
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
			BOOL     flags  = (in_pre      ? PF_PRE   : PF_NONE)
			                | (space_found ? PF_SPACE : PF_NONE);
			if (*(++symbol) != '/') {
				flags |= PF_START;
			} else {
				symbol++;
			}
			
			if (current->text > current->buffer) {
				new_word (current, TRUE);
			}
			tag = parse_tag (parser, &symbol);
			if (tag && render[tag]) {
				flags = (render[tag])(parser, &symbol, flags);
			}
			in_pre      = (flags & PF_PRE   ? TRUE : FALSE);
			space_found = (flags & PF_SPACE ? TRUE : FALSE);
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
				if (current->text < watermark) {
					current->text = scan_namedchar (&symbol, current->text, TRUE,
					                             current->word->font->Base->Mapping);
				} else if (linetoolong == FALSE) {
					errprintf("parse_html(): Line too long in '%s'!\n",
					          frame->Location->File);
					linetoolong = TRUE;
				}
				space_found = FALSE;
				break;
			
			case ' ':
				if (in_pre) {
					if (current->text < watermark) {
						*(current->text++) = font_Nobrk (current->word->font);
					} else if (linetoolong == FALSE) {
						errprintf("parse_html(): Line too long in '%s'!\n",
						          frame->Location->File);
						linetoolong = TRUE;
					}
					symbol++;
					break;
				}
				goto white_space;
			
			case 9:  /* HT HORIZONTAL TABULATION */
				if (in_pre) {
					if (current->text < watermark -8) {
						do {
							*(current->text++) = font_Nobrk (current->word->font);
						} while (((current->text - current->buffer) & 7) != 0);
					} else if (linetoolong == FALSE) {
						errprintf("parse_html(): Line too long in '%s'!\n",
						          frame->Location->File);
						linetoolong = TRUE;
					}
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
				if (in_pre) {
					/* BUG: empty paragrahps were ignored otherwise */
					*(current->text++) = font_Nobrk (current->word->font);
					add_paragraph(current, 0);
					symbol++;
					break;
				}
				/* else fall through */
				
			white_space: /* if (!in_pre) */
				if (!space_found) {
					if (current->text > current->buffer) {
						new_word (current, TRUE);
					}
					*(current->text++) = font_Space (current->word->font);
					space_found = TRUE;
				}
				while (isspace (*(++symbol)));
				break;
			
			default:
				if (current->text < watermark) {
					current->text = (*encoder)(&symbol, current->text);
				} else if (linetoolong == FALSE) {
					errprintf("parse_html(): Line too long in '%s'!\n",
					          frame->Location->File);
					linetoolong = TRUE;
					symbol++;
				}
				space_found = FALSE;
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

BOOL
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
	
	while (*symbol != '\0')
	{
		switch (*symbol)
		{
			case 9: /* HT HORIZONTAL TABULATION */
				if (current->text < watermark) {
					size_t pos = (current->text - current->buffer) /2;
					UWORD  num = 4 - ((UWORD)pos & 0x0003);
					while (num--) {
						*(current->text++) = font_Nobrk (current->word->font);
					}
				} else if (!linetoolong) {
					errprintf ("parse_text(): Line too long in '%s'!\n",
					           frame->Location->File);
					linetoolong = TRUE;
				}
				symbol++;
				break;
			
			case 13: /* CR CARRIAGE RETURN */
				if (*(symbol +1) == 10)  /* HTTP, TOS, or DOS text file */
					symbol++;
				/* else Macintosh text file */
			case 10: /* LF LINE FEED */
			case 11: /* VT VERTICAL TABULATION */
			case 12: /* FF FORM FEED */
				if (current->text > current->buffer) {
					current->word->line_brk = BRK_LN;
					new_word (current, TRUE);
				
				} else if (!current->prev_par) {
					*(current->text++) = font_Nobrk (current->word->font);
					current->word->line_brk = BRK_LN;
					new_word (current, TRUE);
				
				} else {
					if (current->prev_wrd) {
						add_paragraph(current, 0);
					}
					current->prev_par->eop_space += linefeed;
				}
				symbol++;
				break;
			
			default: {
				if (*symbol <= 32 || *symbol == 127) {
					if (current->text < watermark) {
						*(current->text++) = font_Nobrk (current->word->font);
					} else if (!linetoolong) {
						errprintf ("parse_text(): Line too long in '%s'!\n",
						           frame->Location->File);
						linetoolong = TRUE;
					}
					symbol++;
				
				} else {
					if (current->text < watermark) {
						current->text = (*encoder)(&symbol, current->text);
					} else if (!linetoolong) {
						errprintf ("parse_text(): Line too long in '%s'!\n",
						           frame->Location->File);
						linetoolong = TRUE;
						symbol++;
					}
				}
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
BOOL
parse_image (void * arg, long invalidated)
{
	PARSER   parser  = arg;
	FRAME    frame   = parser->Frame;
	TEXTBUFF current = &parser->Current;
	
	if (invalidated) {
		delete_parser (parser);
			
		return FALSE;
	}
	
	current->font_step = new_step (3, frame->text_colour);
	
	font_byType (normal_font, 0x0000, -1, current->word);
	
	new_image (frame, current, NULL,frame->Location, 0,0, 0,0);

	delete_parser (parser);
		
	return FALSE;
}


/*============================================================================*/
void
render_hrule (TEXTBUFF current, H_ALIGN align, short w, short h)
{
	PARAGRPH par  = add_paragraph (current, 0);
	WORDITEM word = current->word;
	
	new_word (current, TRUE);
	add_paragraph (current, 0);
	
	par->paragraph_code = PAR_HR;
	par->alignment      = align;
	word->word_tail_drop = h;
	word->word_height    = (word->word_height + word->word_tail_drop) /2;
	word->space_width    = w;
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
					add_paragraph (current, 1);
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
