#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "token.h" /* must be included before gem/gemx.h */
#include <gem.h>

#include "global.h"
#include "Loader.h"
#include "parser.h"
#include "fontbase.h"
#include "Containr.h"
#include "Location.h"
#include "Form.h"
#include "inet.h"
#include "cache.h"


/*----------------------------------------------------------------------------*/
static BOOL
about_cache (TEXTBUFF current, ENCODING enc, CACHEINF info, size_t num)
{
	size_t unused = FALSE;
	char   buf[100];
	
	if (info) {
		struct s_line {
			WORDITEM word; UWORD len;
		} * mem = malloc (sizeof(struct s_line) * num), * line = mem;
		UWORD  max_ln = 0;
		size_t count  = num;
		while (count--) {
			WORDITEM w = current->word;
			int iw = (int)(info->Hash >>12) & 0x0FFF;
			int ih = (int) info->Hash       & 0x0FFF;
			sprintf (buf, "%s (%i*%i)", info->File, iw, ih);
			render_text (current, buf);
			line->word = current->prev_wrd;
			line->len  = w->word_width;
			sprintf (buf, " %li", info->Size);
			render_text (current, buf);
			current->word->line_brk = BRK_LN;
			if (info->Used) {
				sprintf (buf, "(%li)", info->Used);
				render_text (current, buf);
			} else {
				sprintf (buf, "clear=0x%08lX", (long)info->Object);
				font_byType (-1, FNT_BOLD, font_step2size (NULL, 2), current->word);
				form_buttn (current, buf, "&times;", enc, 'S');
				font_byType (-1, 0x0000, font_step2size (NULL, 3), current->word);
				unused = TRUE;
			}
			while ((w = w->next_word) != current->word) {
				line->len += w->word_width;
			}
			if (max_ln < line->len) {
				max_ln = line->len;
			}
			line++;
			info++;
		}
		if (mem) {
			while (line-- > mem) {
				line->word->word_width += max_ln - line->len;
			}
			free (mem);
		}
		if (num > 0) {
			render_hrule (current, ALN_LEFT, -512, 2);
		}
		free (info);
	}
	return unused;
}


/*----------------------------------------------------------------------------*/
static void
about_highwire (TEXTBUFF current, WORD link_color)
{
	const char * i_net = inet_info();
	char     buf[100];
	WORDITEM list[10], * w = &list[-1];
	WORD tab = 0;
	
	#ifdef GEM_MENU
		*(++w) = render_text (current, """GEMMenu:""""0.4""\r");
		tab    = (*w)->word_width;
	#endif
	
	*(++w) = render_text (current, """Core:"""
	                      _HIGHWIRE_VERSION_ " " _HIGHWIRE_BETATAG_ "\n");
	tab    = max (tab, (*w)->word_width);
	
	font_byType (-1, -1, font_step2size (NULL, 2), current->word);
	
	#ifdef LIBGIF
	{
		#define w /* avoid warnings due to shadowed */
		#include <gif_lib.h>
		#undef w
		const char * p = GIF_LIB_VERSION;
		while (*p && *p != '.' && !isdigit(*p)) p++;
		if (*p) {
			char * b = buf;
			*(++w) = render_text (current, """gif-lib:""");
			tab    = max (tab, (*w)->word_width);
			while (*p == '.' || isdigit(*p)) *(b++) = *(p)++;
			*(b++) = '';
			*(b++) = '\r';
			*(b)   = '\0';
			render_text (current, buf);
		}
	}
	#endif
	
	sprintf (buf, """GEMlib:""""%i.%02i.%i"" " __GEMLIB_BETATAG__ "\r",
	         __GEMLIB_MAJOR__, __GEMLIB_MINOR__, __GEMLIB_REVISION__);
	*(++w) = render_text (current, buf);
	tab    = max (tab, (*w)->word_width);
	
	#ifdef __MINTLIB__
		sprintf (buf,
		         """MiNTlib:""""%i.%02i.%i"" " __MINTLIB_BETATAG__ "\r",
		         __MINTLIB_MAJOR__, __MINTLIB_MINOR__, __MINTLIB_REVISION__);
		*(++w) = render_text (current, buf);
		tab    = max (tab, (*w)->word_width);
	#endif
	
	*(++w) = render_text (current, """compiled:""");
	tab    = max (tab, (*w)->word_width);
	#if defined (__GNUC__)
		sprintf (buf, "Gnu C """"%i.%02i, ", __GNUC__, __GNUC_MINOR__);
		render_text (current, buf);
	#elif defined (__PUREC__)
		sprintf (buf, "Pure C """"%i.%02x, ",
		         __PUREC__ >>8, __PUREC__ & 0xFF);
		render_text (current, buf);
	#elif defined (LATTICE)
		render_text (current, "Lattice C\n");
	#endif
	render_text (current, ""__DATE__ ", " __TIME__"\n");
	
	/* align left column */
	do {
		(*w)->word_width = tab;
	} while (--w >= list);
	
	font_byType (-1, -1, font_step2size (NULL, 3), current->word);
	if (i_net) {
		sprintf (buf, "(%s support enabled)", i_net);
		render_text (current, buf);
	}
	render_hrule (current, ALN_LEFT, -512, 2);
	
	render_link (current, "cached: ", "about:cache", link_color);
}


/*==============================================================================
 * Create 'about' page
*/
BOOL
parse_about (void * arg, long invalidated)
{
	PARSER   parser  = arg;
	FRAME    frame   = parser->Frame;
	TEXTBUFF current = &parser->Current;
	char buf[100];
	const char * title = frame->Location->File;
	WORD     mode; /* 0 = normal, 1 = cache */
	CACHEINF info    = NULL;
	BOOL     clrable = TRUE; /* guess there is anything cached to be cleared */
	size_t   c_mem, c_num;
	
	if (invalidated) {
		delete_parser (parser);
			
		return FALSE;
	}
	
	current->paragraph->alignment = ALN_CENTER;
	font_byType (header_font, FNT_BOLD, font_step2size (NULL, 6), current->word);
	
	if (strncmp ("cache", title, 5) == 0) {
		if (strncmp (title +5, "?clear", 6) == 0) {
			if (title[11] == '\0' || strcmp (title +11, "=clear") == 0) {
				cache_clear (NULL);
				mode = 1;
			} else if (title[11] == '=') {
				char * rest;
				long   obj = strtol (title +12, &rest, 16);
				if (obj > 0 && (!*rest || *rest == '=')) {
					cache_clear ((CACHED)obj);
					mode = 1;
				}
			}
		}
		containr_notify (parser->Target, HW_SetTitle, "About: Cache");
		title = "Objects in cache:";
		mode  = 1;
	
	} else {
		containr_notify (parser->Target, HW_SetTitle, "About: HighWire");
		title = "Version Information";
		mode  = 0;
	}
	render_text  (current, title);
	render_hrule (current, ALN_CENTER, -1024, 2);
	
	current->paragraph->alignment = ALN_LEFT;
	font_byType (normal_font, 0x0000, font_step2size (NULL, 3), current->word);
	current->form = new_form (frame, NULL, strdup ("about:cache"), "GET");
	
	c_num = cache_info (&c_mem, (mode == 1 ? &info : NULL));
	
	switch (mode) {
		case 1:
			clrable = about_cache (current, frame->Encoding, info, c_num);
			break;
		default:
			about_highwire (current, frame->link_colour);
	}
	
	if (c_num) {
		sprintf (buf, "%lu object%s, %lu bytes ",
		         c_num, (c_num > 1 ? "s" : ""), c_mem);
		render_text (current, buf);
		form_buttn (current, "clear", "clear", frame->Encoding, 'S');
		if (!clrable) {
			input_disable (current->prev_wrd->input, TRUE);
		}
	} else {
		render_text (current, "(empty)");
	}
	
	delete_parser (parser);
		
	return FALSE;
}
