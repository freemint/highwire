#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined (__PUREC__)
# include <tos.h>
# include <ext.h>

#elif defined (LATTICE)
# include <dos.h>
# include <mintbind.h>
# define DTA      struct FILEINFO
# define d_attrib attr
# define d_length size
# define d_time   time
# define d_fname  name

#else /*__GNUC__*/
# include <sys/stat.h>
# include <mintbind.h>
# define DTA      _DTA
# define d_attrib dta_attribute
# define d_length dta_size
# define d_time   dta_time
# define d_date   dta_date
# define d_fname  dta_name
#endif

#ifndef S_ISDIR
#	define S_ISDIR(mode) (((mode) & 0170000) == (0040000))
#endif

#include "global.h"
#include "token.h"
#include "Loader.h"
#include "parser.h"
#include "Location.h"
#include "fontbase.h"


/*==============================================================================
 * Create directory listing
 *
*/
BOOL
parse_dir (void * arg, long invalidated)
{
	PARSER   parser  = arg;
	FRAME    frame   = parser->Frame;
	TEXTBUFF current = &parser->Current;
	
	char buf[HW_PATH_MAX *2 +3];
	
	LOCATION     loc   = frame->Location;
	const char * dir   = loc->File -1;
	char         delim = *loc->Path;
	char         sort  = (loc->Anchor ? toupper(*loc->Anchor) : '\0');
	WORD         color = frame->link_colour;
	
	struct dir_ent {
		struct dir_ent * next;
		WORDITEM word;
		union {
			struct {
				unsigned year :7;
				unsigned mon  :4;
				unsigned day  :5;
				unsigned hour :5;
				unsigned min  :6;
				unsigned sec  :5;
			}     x;
			UWORD us[2];
			ULONG ul; /* for sorting */
			
		}        date;
		size_t   size;
		BOOL     dir;
		char     name[2];
	} * list = NULL;
	
	WORDITEM wrd_name, wrd_size;
	short    max_name, max_size;
	
	long dh;
	
	if (invalidated) {
		delete_parser (parser);
			
		return FALSE;
	}
	
	if ((dh = Dopendir (loc->FullName, 0)) == -32L) {
		LOCATION tmp = new_location ("*.*", loc);
		DTA    * old = Fgetdta();
		DTA      dta;
		char *   name = dta.d_fname;
		Fsetdta (&dta);
		if (Fsfirst (tmp->FullName, 0x10) == 0) do {
			if (name[0] != '.' || (name[1] && (name[1] != '.' || name[2]))) {
				size_t           len = strlen (name);
				struct dir_ent * ent = malloc (sizeof(struct dir_ent) + len);
				memcpy (ent->name, name, len);
				if ((ent->dir = ((dta.d_attrib & 0x10) != 0)) == TRUE) {
					ent->name[len++] = delim;
				}
				ent->name[len] = '\0';
				ent->size = dta.d_length;
			#ifdef LATTICE
				ent->date.ul = (ULONG)dta.d_time;
			#else
				ent->date.us[0] = dta.d_date;
				ent->date.us[1] = dta.d_time;
			#endif
				ent->next = list;
				list = ent;
			}
		} while (Fsnext() == 0);
		Fsetdta (old);
		free_location (&tmp);
	
	} else if ((ULONG)dh < 0xFF000000uL) {
		char * name = buf +4;
		struct xattr xattr;
		long   xret;
		
#if defined (__PUREC__) && !defined (__MINT_EXT__)
		char   path[HW_PATH_MAX];
		char * p_nm = strchr (strcpy (path, loc->FullName), '\0');
		while (Dreaddir (sizeof(buf), dh, buf) == 0) {
			strcpy (p_nm, name);
			xret = Fxattr (0, path, &xattr);
#else
		while (Dxreaddir (sizeof(buf), dh, buf, &xattr, &xret) == 0) {
#endif
			if (name[0] != '.' || (name[1] && (name[1] != '.' || name[2]))) {
				size_t           len = strlen (name);
				struct dir_ent * ent = malloc (sizeof(struct dir_ent) + len);
				memcpy (ent->name, name, len);
				if ((ent->dir = (S_ISDIR (xattr.mode) != 0)) == TRUE) {
					ent->name[len++] = delim;
				}
				ent->name[len] = '\0';
				ent->size = xattr.st_size;
				ent->date.us[0] = xattr.mdate;
				ent->date.us[1] = xattr.mtime;
				ent->next = list;
				list = ent;
			}
		}
		Dclosedir (dh);
	}
	
	font_byType (header_font, FNT_BOLD, font_step2size (NULL, 6), current->word);
	render_text (current, "Index of ");
	if (!sort || !strchr ("NDS", sort)) {
		sort = 'N';
	}
	while (dir > loc->FullName && *(--dir) != delim);
	if (dir > loc->FullName) {
		size_t len = ++dir - loc->FullName;
		char * lnk = buf + sizeof(buf) -4 - len;
		memcpy (lnk, loc->FullName, len);
		lnk[len]   = '\0';
		sprintf (buf, "%s", lnk);
		lnk[len++] = '#';
		lnk[len++] = sort;
		lnk[len]   = '\0';
		render_link (current, buf, lnk, color);
	}
	sprintf (buf, "%s\n", dir);
	render_text (current, buf);
	
	font_byType (normal_font, -1, font_step2size (NULL, 3), current->word);
	strcat (strcpy (buf, loc->FullName), "# ");
	if (sort == 'N') {
		wrd_name = render_text (current, "Name");
	} else {
		strchr (buf, '#')[1] = 'N';
		wrd_name = render_link (current, "Name", buf, color);
	}
	max_name = wrd_name->word_width;
	if (sort == 'S') {
		wrd_size = render_text (current, "Size");
	} else {
		strchr (buf, '#')[1] = 'S';
		wrd_size = render_link (current, "Size", buf, color);
	}
	max_size = wrd_size->word_width;
	if (sort == 'D') {
		render_text (current, "Date");
	} else {
		strchr (buf, '#')[1] = 'D';
		render_link (current, "Date", buf, color);
	}
	current->word->word_height = 2;
	render_hrule (current, ALN_CENTER, -1024, 2);

	font_byType (-1, -1, font_step2size (NULL, 3), current->word);
	if (list) {
		WORDITEM         w;
		struct dir_ent * ent, * to_do;
		
		/* sort entries
		*/
		if ((to_do = list->next) != NULL) {
			list->next = NULL;
			
			if (sort == 'D') { /* sort by date */
				do {
					struct dir_ent ** pptr = &list;
					ent   = to_do;
					to_do = to_do->next;
					while (*pptr && ent->date.ul > (*pptr)->date.ul) {
						pptr = &(*pptr)->next;
					}
					ent->next = *pptr;
					*pptr     = ent;
				} while (to_do);
			
			} else if (sort == 'S') { /* sort by size */
				do {
					struct dir_ent ** pptr = &list;
					ent   = to_do;
					to_do = to_do->next;
					while (*pptr && ent->size > (*pptr)->size) {
						pptr = &(*pptr)->next;
					}
					ent->next = *pptr;
					*pptr     = ent;
				} while (to_do);
			
			} else { /* 'N', sort by name */
				do {
					struct dir_ent ** pptr = &list;
					ent   = to_do;
					to_do = to_do->next;
					while (*pptr && strcmp (ent->name, (*pptr)->name) > 0) {
						pptr = &(*pptr)->next;
					}
					ent->next = *pptr;
					*pptr     = ent;
				} while (to_do);
			}
		}
		
		/* create rows and get widths
		*/
		ent = list;
		do {
			char * lnk;
			if (ent->dir) {
				size_t len = strlen (ent->name);
				lnk = memcpy (buf + sizeof(buf) -4 - len, ent->name, len);
				lnk[len++] = '#';
				lnk[len++] = sort;
				lnk[len]   = '\0';
				render_text (current, "");
				if (ent->size) {
					sprintf (buf, "%s%lu", ent->name, ent->size);
				} else {
					sprintf (buf, "%s-", ent->name);
				}
			} else {
				lnk = ent->name;
				render_text (current, "");
				sprintf (buf, "%s%lu", ent->name, ent->size);
			}
			w = ent->word = render_link (current, buf, lnk, color);
			max_name = max (max_name, w->word_width);
			w = w->next_word->next_word;
			max_size = max (max_size, w->word_width);
			sprintf (buf, "%u-%02u-%02u %02u:%02u:%02u\r",
			         ent->date.x.year +1980, ent->date.x.mon, ent->date.x.day,
			         ent->date.x.hour, ent->date.x.min, ent->date.x.sec <<1);
			render_text (current, buf);
		} while ((ent = ent->next) != NULL);
		
		/* align columns and delete list
		*/
		wrd_name->word_width = max_name;
		wrd_size->word_width = max_size;
		ent = list;
		do {
			list = ent->next;
			w = ent->word;
			w->word_width = max_name;
			w = w->next_word;
			w->word_width += max_size - w->next_word->word_width;
			free (ent);
		} while ((ent = list) != NULL);
		
	}

	delete_parser (parser);
		
	return FALSE;
}
