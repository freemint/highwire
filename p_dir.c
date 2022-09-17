#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __PUREC__
# include <ext.h>
#endif

#include "file_sys.h"
#include "global.h"
#include "Loader.h"
#include "parser.h"
#include "Location.h"
#include "fontbase.h"


/*==============================================================================
 * Create directory listing
 *
*/
int
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
	WORD         color = frame->link_color;
	
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
	
	if ((dh = Dopendir (loc->FullName, 0)) == -EINVFN) {
		LOCATION tmp = new_location ("*.*", loc);
		DTA    * old = Fgetdta();
		DTA      dta;
		char *   name = dta.d_fname;
		Fsetdta (&dta);
		if (Fsfirst (tmp->FullName, 0x10) == E_OK) do {
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
		} while (Fsnext() == E_OK);
		Fsetdta (old);
		free_location (&tmp);
	
	} else if ((ULONG)dh < 0xFF000000uL) {
		char * name = buf +4;
		struct xattr xattr;
		long   xret;
		
#if defined (__PUREC__) && !defined (__MINT_EXT__)
		char   path[HW_PATH_MAX];
		char * p_nm = strchr (strcpy (path, loc->FullName), '\0');
		while (Dreaddir (sizeof(buf), dh, buf) == E_OK) {
			strcpy (p_nm, name);
			xret = F_xattr (0, path, &xattr);
#else
		while (D_xreaddir (sizeof(buf), dh, buf, &xattr, &xret) == E_OK) {
#endif
			if (name[0] != '.' || (name[1] && (name[1] != '.' || name[2]))) {
				size_t           len = strlen (name);
				struct dir_ent * ent = malloc (sizeof(struct dir_ent) + len);
				memcpy (ent->name, name, len);
				if ((ent->dir = (S_ISDIR (xattr.mode) != 0)) == TRUE) {
					ent->name[len++] = delim;
				}
				ent->name[len] = '\0';
				ent->size = xattr.size;
				ent->date.us[0] = xattr.mdate;
				ent->date.us[1] = xattr.mtime;
				ent->next = list;
				list = ent;
			}
		}
		Dclosedir (dh);
	}
	
	font_byType (header_font, FNT_BOLD, font_step2size (6), current->word);
	render_text (current, "Index of \003");
	if (!sort || !strchr ("NDS", sort)) {
		sort = 'N';
	}
	while (dir > loc->FullName && *(--dir) != delim);
	if (dir > loc->FullName) {
		size_t len = ++dir - loc->FullName;
		char * lnk = buf;
		memcpy (lnk, loc->FullName, len);
		lnk[len++]   = '\003';
		lnk[len++] = '#';
		lnk[len++] = sort;
		lnk[len]   = '\0';
		render_link (current, buf, lnk, color);
	}
	sprintf (buf, "%s\n\005\003", dir);
	render_text (current, buf);
	
	font_byType (normal_font, -1, font_step2size (3), current->word);
	strcat (strcpy (buf, loc->FullName), "# ");
	if (sort == 'N') {
		wrd_name = render_text (current, "Name\003\005\003");
	} else {
		strchr (buf, '#')[1] = 'N';
		wrd_name = render_link (current, "Name\003\005\003", buf, color);
	}
	max_name = wrd_name->word_width;
	if (sort == 'S') {
		wrd_size = render_text (current, "Size\003\005\005\003");
	} else {
		strchr (buf, '#')[1] = 'S';
		wrd_size = render_link (current, "Size\003\005\005\003", buf, color);
	}
	max_size = wrd_size->word_width;
	if (sort == 'D') {
		render_text (current, "Date\024");
	} else {
		strchr (buf, '#')[1] = 'D';
		render_link (current, "Date\024", buf, color);
	}
	current->word->word_height = 2;
	render_hrule (current, ALN_CENTER, -1024, 2, TRUE);

	font_byType (-1, -1, font_step2size (3), current->word);
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
			char * lnk, *p = buf;
			if (ent->dir) {
				size_t len = strlen (ent->name);
				lnk = memcpy (buf + sizeof(buf) -4 - len, ent->name, len);
				lnk[len++] = '#';
				lnk[len++] = sort;
				lnk[len]   = '\0';
				render_text (current, "\005\025");
				if (ent->size) {
					sprintf (buf, "%s\024\005\003%lu\003", ent->name, ent->size);
				} else {
					sprintf (buf, "%s\024\005\022-\020", ent->name);
				}
			} else {
				lnk = ent->name;
				render_text (current, "\005\003");
				sprintf (buf, "%s\003\005\003%lu\003", ent->name, ent->size);
			}
			while (*p >= ' ') {
				if (*p == ' ') *p = '\005';
				p++;
			}
			w = ent->word = render_link (current, buf, lnk, color);
			max_name = max (max_name, w->word_width);
			w = w->next_word->next_word;
			max_size = max (max_size, w->word_width);
			sprintf (buf, " \005%u-%02u-%02u\005%02u:%02u:%02u\r",
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
