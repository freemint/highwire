#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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
#include "ovl_sys.h"
#include "cache.h"


/*----------------------------------------------------------------------------*/
static void
about_modules (TEXTBUFF current, ENCODING enc)
{
	char buf[100];
	MODULINF mod = NULL;
	size_t   num = module_info (&mod);
	
	if (mod && num) {
		size_t count = num;
		while (count--) {
	#if 01
			struct ovl_info_t * info = (*mod->Meth->ovl_version)();
			sprintf (buf, "%s: %s by %s",
			         mod->File, info->name, info->author);
			render_text (current, buf);
			current->word->line_brk = BRK_LN;
			sprintf (buf, "kill=0x%08lX", (long)mod->Meth);
			font_byType (-1, FNT_BOLD, font_step2size (NULL, 2), current->word);
			form_buttn (current, buf, "&times;", enc, 'S');
			font_byType (-1, 0x0000, font_step2size (NULL, 3), current->word);
	#else
			sprintf (buf, "%s\r", mod->File);
			render_text (current, buf);
	#endif
			mod++;
		}
		if (num > 0) {
			render_hrule (current, ALN_LEFT, -512, 2);
		}
		free (mod - num);
	
	} else {
		render_text (current, "(none)");
	}
}


/*----------------------------------------------------------------------------*/
static BOOL
about_cache (TEXTBUFF current, ENCODING enc, CACHEINF info, size_t num)
{
	BOOL unused = FALSE;
	char buf[1000];
	
	if (info) {
		struct s_line {
			WORDITEM word; UWORD len;
		}    * mem    = malloc (sizeof(struct s_line) * num), * line = mem;
		UWORD  max_ln = 0;
		size_t count  = num;
		while (count--) {
			WORDITEM w = current->word;
			char source[500] ="";
			location_FullName (info->Source, source +2, sizeof(source)-2);
			render_text (current, source);
			render_text (current, "")->line_brk = BRK_LN;
			w = current->word;
			if (info->Ident) {
				int ic = (int)(info->Ident >>24) & 0x00FF; /* bgnd colour */
				int iw = (int)(info->Ident >>12) & 0x0FFF; /* width       */
				int ih = (int) info->Ident       & 0x0FFF; /* height      */
				sprintf (buf, "Memory:%i*%i,%02X", iw, ih, ic);
				render_text (current, buf);
			} else if (!info->Local) {
				render_text (current, "(busy)");
			} else {
				sprintf (buf, "Disk:%s", info->Local->File);
				render_text (current, buf);
			}
			sprintf (buf, " %li", info->Size);
			render_text (current, buf);
			if (mem) {
				line->len = w->word_width;
				do {
					line->word = w;
					w          = w->next_word;
					line->len += w->word_width;
				} while (w != current->prev_wrd);
				if (max_ln < line->len) {
					max_ln = line->len;
				}
				line++;
			}
			if (info->Used || !info->Object) {
				sprintf (buf, "[%li]", info->Used);
				render_text (current, buf);
			} else {
				sprintf (buf, "clear=0x%08lX", (long)info->Object);
				font_byType (-1, FNT_BOLD, font_step2size (NULL, 1), current->word);
				form_buttn (current, buf, "&times;", enc, 'S');
				current->prev_wrd->word_height -= 2;
				font_byType (-1, 0x0000, font_step2size (NULL, 3), current->word);
				unused = TRUE;
			}
			if (info->Date) {
				struct tm * tm = localtime ((time_t*)&info->Date);
				sprintf (buf, "%04i-%02i-%02i %02i:%02i:%02i",
				         tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
				         tm->tm_hour, tm->tm_min, tm->tm_sec);
				render_text (current, buf);
				if (info->Expires) {
					tm = localtime ((time_t*)&info->Expires);
					sprintf (buf, "expires:%04i-%02i-%02i %02i:%02i:%02i",
					         tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
					         tm->tm_hour, tm->tm_min, tm->tm_sec);
					render_text (current, buf);
				}
			}
			current->prev_wrd->line_brk = BRK_LN;
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
		free (info - num);
	}
	return unused;
}


/*----------------------------------------------------------------------------*/
static void
about_highwire (TEXTBUFF current, WORD link_color)
{
#ifndef USE_OVL
	const char * i_net = inet_info();
#endif
	char     buf[100];
	WORDITEM list[10], * w = &list[-1];
	WORD     tab   = 0;
	size_t   m_num = module_info (NULL);
	
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
	#ifndef USE_OVL
	if (i_net) {
		sprintf (buf, "(%s support enabled)", i_net);
		render_text (current, buf);
	}
	#endif
	render_hrule (current, ALN_LEFT, -512, 2);
	
	sprintf (buf, "Screen Mode: '%s'\r", image_dispinfo());
	render_text (current, buf);
	
	render_link (current, "modules: ", "about:modules", link_color);
	if (m_num) {
		sprintf (buf, "%li loaded\r", m_num);
		render_text (current, buf);
	} else {
		render_text (current, "(none)\r");
	}
	
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
	size_t   c_mem   = 0, c_num = 0;
	BOOL     clrable = TRUE; /* guess there is anything cached to be cleared */
	
	if (invalidated) {
		delete_parser (parser);
			
		return FALSE;
	}
	
	current->paragraph->alignment = ALN_CENTER;
	font_byType (header_font, FNT_BOLD, font_step2size (NULL, 6), current->word);
	
	if (strncmp ("modules", title, 7) == 0) {
		if (strncmp (title +7, "?kill=", 6) == 0) {
			char * rest;
			long   ovl = strtol (title +13, &rest, 16);
			if (ovl > 0 && *rest == '=') {
				kill_ovl ((void*)ovl);
			}
		}
		containr_notify (parser->Target, HW_SetTitle, "About: Modules");
		title = "Modules loaded:";
		mode  = 2;
	
	} else if (strncmp ("cache", title, 5) == 0) {
		if (strncmp (title +5, "?clear", 6) == 0) {
			if (title[11] == '\0' || strcmp (title +11, "=clear") == 0) {
				cache_clear (NULL);
			} else if (title[11] == '=') {
				char * rest;
				long   obj = strtol (title +12, &rest, 16);
				if (obj > 0 && (!*rest || *rest == '=')) {
					cache_clear ((CACHED)obj);
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
	font_switch (current->word->font, NULL);
	render_text  (current, title);
	render_hrule (current, ALN_CENTER, -1024, 2);
	
	current->paragraph->alignment = ALN_LEFT;
	font_byType (normal_font, 0x0000, font_step2size (NULL, 3), current->word);
	
	if (mode <= 1) {
		current->form = new_form (frame, NULL, strdup ("about:cache"), "GET");
		c_num = cache_info (&c_mem, (mode == 1 ? &info : NULL));
	}
	switch (mode) {
		case 2:
			current->form = new_form (frame, NULL, strdup("about:modules"), "GET");
			about_modules (current, frame->Encoding);
			break;
		case 1:
			clrable = about_cache (current, frame->Encoding, info, c_num);
			break;
		default:
			about_highwire (current, frame->link_color);
	}
	
	if (c_num) {
		sprintf (buf, "%lu object%s, %lu bytes ",
		         c_num, (c_num > 1 ? "s" : ""), c_mem);
		render_text (current, buf);
		form_buttn (current, "clear", "clear", frame->Encoding, 'S');
		if (!clrable) {
			input_disable (current->prev_wrd->input, TRUE);
		}
	} else if (mode <= 1) {
		render_text (current, "(empty)");
	}
	
	delete_parser (parser);
		
	return FALSE;
}
