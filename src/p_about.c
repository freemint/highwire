#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <gem.h>
#include <cflib.h>

#include "version.h"
#include "global.h"
#include "Loader.h"
#include "parser.h"
#include "fontbase.h"
#include "Containr.h"
#include "Location.h"
#include "Form.h"
#include "inet.h"
#include "cache.h"

#ifdef LIBPNG
  #include "png.h"
#endif

#ifdef LIBGIF
  #include "gif_lib.h"
#endif

#ifdef LIBJPG
  #define XMD_H /* avoid redefining INT16 and INT32, already done in gemlib */
#undef INLINE
#undef LOCAL
#undef GLOBAL
    #include <jpeglib.h>
  #undef XMD_H
#endif



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
			char source[500] ="\022\025\005";
			location_FullName (info->Source, source +3, sizeof(source)-3);
			render_text (current, source);
			render_text (current, "\024\003\005\005\005\020")->line_brk = BRK_LN;
			w = current->word;
			if (info->Ident) {
				int ic = (int)(info->Ident >>24) & 0x00FF; /* bgnd colour */
				int iw = (int)(info->Ident >>12) & 0x0FFF; /* width       */
				int ih = (int) info->Ident       & 0x0FFF; /* height      */
				sprintf (buf, "Memory:\005\021%i*%i,%02X\020", iw, ih, ic);
				render_text (current, buf);
			} else if (!info->Cached) {
				TA_Color (render_text (current, "\026(busy)\024")->attr) = G_RED;
			} else {
				sprintf (buf, "Disk:\005\022%s\020", info->Cached);
				render_text (current, buf);
			}
			sprintf (buf, " \005%li\005\003", info->Size);
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
				sprintf (buf, "[%li]\003", info->Used);
				render_text (current, buf);
			} else {
				sprintf (buf, "clear=0x%08lX", (long)info->Object);
				font_byType (-1, FNT_BOLD, font_step2size (1), current->word);
				form_buttn (current, buf, "&times;", enc, 'S');
				current->prev_wrd->word_height -= 2;
				font_byType (-1, FNT_NORMAL, font_step2size (3), current->word);
				unused = TRUE;
			}
			if (info->Date) {
				struct tm * tm = localtime ((time_t*)&info->Date);
				sprintf (buf, "\005\005\021%04i-%02i-%02i %02i:%02i:%02i\020",
				         tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
				         tm->tm_hour, tm->tm_min, tm->tm_sec);
				render_text (current, buf);
				if (info->Expires > 0) {
					tm = localtime ((time_t*)&info->Expires);
					sprintf (buf, "\005\005expires:\005\021%04i-%02i-%02i %02i:%02i:%02i\020",
					         tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
					         tm->tm_hour, tm->tm_min, tm->tm_sec);
					render_text (current, buf);
				} else if (info->Expires) { /* < 0 */
					render_text (current, "\005\005(no-cache)\003");
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
			render_hrule (current, ALN_LEFT, -512, 2, TRUE);
		}
		free (info - num);
	}
	return unused;
}


/*----------------------------------------------------------------------------*/
static void
about_highwire (TEXTBUFF current, WORD link_color)
{
	const char * i_net = inet_info();
	char     buf[100];
	size_t   m_num = (inet_type() > 0 ? 1 : 0);
 
  if (ldg_mbedtls_loaded()) { m_num++; }
	
  render_text (current, "Core:\005" _HIGHWIRE_VERSION_ "\n");
		
  #ifdef __GEMLIB_MAJOR__
	  sprintf (buf, "GEMlib:\005%i.%02i.%i\005" __GEMLIB_BETATAG__ "\r", __GEMLIB_MAJOR__, __GEMLIB_MINOR__, __GEMLIB_REVISION__);
	  render_text (current, buf);
  #endif
	
	#ifdef __MINTLIB__
		sprintf (buf, "MiNTlib:\005%i.%02i.%i\005" __MINTLIB_BETATAG__ "\r", __MINTLIB_MAJOR__, __MINTLIB_MINOR__, __MINTLIB_REVISION__);
    render_text (current, buf);
	#endif
	
	#ifdef __CFLIB__
		sprintf (buf, "CFlib:\005%i.%02i.%i\005" __CFLIB_BETATAG__ "\r", __CFLIB_MAJOR__, __CFLIB_MINOR__, __CFLIB_REVISION__);
		render_text (current, buf);
	#endif

  #ifdef LIBPNG
    # ifdef PNG_LIBPNG_VER_STRING
		sprintf (buf, "libpng:\005%s\r", PNG_LIBPNG_VER_STRING);
		render_text (current, buf);
    #endif
  #endif

  #ifdef LIBGIF
    #if defined(GIF_LIB_VERSION)
		sprintf (buf, "gif_lib:\005\%s\r", GIF_LIB_VERSION);
		render_text (current, buf);
    #elif defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && defined(GIFLIB_RELEASE)
		sprintf (buf, "gif_lib:\005%d.%d.%d\r", GIFLIB_MAJOR, GIFLIB_MINOR, GIFLIB_RELEASE);
		render_text (current, buf);
    #endif
  #endif

  #ifdef LIBJPG
    #ifdef JPEG_LIB_VERSION
		sprintf (buf, "jpeg_lib:\005%d\r", JPEG_LIB_VERSION);
		render_text (current, buf);
    #endif
  #endif

	#if defined (__GNUC__)
    #ifdef __VERSION__
    sprintf (buf, "compiled:\005Gnu\005C\005%s\005"__DATE__ ",\005" __TIME__"\n", __VERSION__);
    #else
    sprintf (buf, "compiled:\005Gnu\005C\005%i.%02i,\005"__DATE__ ",\005" __TIME__"\n", __GNUC__, __GNUC_MINOR__);
    #endif
		render_text (current, buf);
	#elif defined (__PUREC__)
		sprintf (buf, "compiled:\005Pure\005C\005%i.%02x,\005"__DATE__ ", " __TIME__"\n",__PUREC__ >>8, __PUREC__ & 0xFF);
		render_text (current, buf);
	#elif defined (LATTICE)
		render_text (current, "compiled:\005Lattice\005C,\005"__DATE__ ", " __TIME__"\n");
	#endif
	
	sprintf (buf, "Screen\005Mode:\005%s\r", image_dispinfo());
	render_text (current, buf);
	
  if (!cfg_File)
  {
    sprintf (buf, "Config\005File:\005(none)\n");
  	render_text (current, buf);
  }
  else
  {
  	snprintf (buf, (int)sizeof(buf) -16, "Config\005File:\003\005%s\n", cfg_File);
    render_link (current, buf, cfg_File, link_color);
  }
	
  if (!m_num)
  {
    sprintf (buf, "modules:\005:\005(none)\r");
  }
  else
  {
  	sprintf (buf, "modules:\005%li\005loaded\r", m_num);
  }
	render_text (current, buf);
	 	
	if (i_net)
  {
		sprintf (buf, "%s\005support\005enabled%s", i_net, m_num > 1 ? "\r" : "\n");
		render_text (current, buf);
	 
    if (ldg_mbedtls_loaded())
    {
      sprintf (buf, "mbedTLS/PolarSSL handles %s to %s\n", SecProt[cfg_SecProtMin], SecProt[cfg_SecProtMax]);
      render_text (current, buf);
    }
  }
  
 
	render_link (current, "cache:\003\005", "about:cache", link_color);
}


/*==============================================================================
 * Create 'about' page
*/
int
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
		
  if (strncmp ("cache", title, 5) == 0) {
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
		mode  = 1;
	
	} else {
		containr_notify (parser->Target, HW_SetTitle, "About: HighWire");
		mode  = 0;
	}
	font_switch (current->word->font, NULL);
	
	current->paragraph->Box.TextAlign = ALN_LEFT;
	font_byType (normal_font, FNT_NORMAL, font_step2size (3), current->word);
	
	if (mode <= 1) {
		c_num = cache_info (&c_mem, (mode == 1 ? &info : NULL));
	}
	switch (mode) {
		case 1:
			clrable = about_cache (current, frame->Encoding, info, c_num);
			break;
		default:
			about_highwire (current, frame->link_color);
			break;
	}
	
	if (c_num) {
		sprintf (buf, "%lu object%s,\005%lu\005bytes\005 ",
		         c_num, (c_num > 1 ? "s" : ""), c_mem);
		render_text (current, buf);
		form_buttn (current, "clear\0005", "clear", frame->Encoding, 'S');
		if (!clrable) {
			input_disable (current->prev_wrd->input, TRUE);
		}
	} else if (mode <= 1) {
		render_text (current, "(empty)");
	}
	
	delete_parser (parser);
		
	return FALSE;
}
