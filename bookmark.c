/*
 * Bookmark.C
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h> /* isspace() */
#if defined (__PUREC__)
# include <ext.h>
#else
# include <sys/stat.h>
#endif

#include "file_sys.h"
#include "global.h"
#include "bookmark.h"


#define BKM_MAGIC "<!DOCTYPE HighWire-Bookmark-file-2>\n"

const char * bkm_File = NULL;
const char * bkm_CSS  = NULL;


/*----------------------------------------------------------------------------*/
#define open_bookmarkCss(mode) open_default (&bkm_CSS, _HIGHWIRE_BKM_CSS_, mode)
#define open_bookmarks(mode)   open_default (&bkm_File,_HIGHWIRE_BOOKMARK_,mode)

/*----------------------------------------------------------------------------*/
static char *
wr_grp (char * buff, long id_class, const char * title)
{
	sprintf (buff, "<DT CLASS='GRP' ID='G:%08lX'><B>%s</B></DT>\n"
	               "<DL CLASS='G:%08lX'>", 
	               id_class, title,   id_class);
	return buff;
}

/*----------------------------------------------------------------------------*/
static char *
wr_lnk (char * buff, long id, long visit, const char * url, const char * title)
{
	sprintf (buff, "<DT CLASS='LNK' ID='L:%08lX'><A TARGET='_hw_top'"
	               " LAST_VISIT='%08lX' HREF='%s'>%s</A></DT>",
	               id, visit, url, title);
	return buff;
}


/******************************************************************************/

typedef struct s_bkm_line * BKM_LINE;
struct s_bkm_line {
	BKM_LINE Next, Prev;
	char   * Id;
	size_t   Id_ln;
	char   * Class;
	size_t   Class_ln;
	char   * Text;
	size_t   Text_ln;
	char     Type;      /* '\0' unchecked, '*' someting else, 'B'ookmark, `!`, */
	char     Buff[2];                             /* 'G'roup & 'b'egin & 'e'nd */
};
static BKM_LINE bkm_list_beg = NULL;
static BKM_LINE bkm_list_end = NULL;
static time_t   bkm_list_tm  = 0;

/*----------------------------------------------------------------------------*/
#if 0
static void
bkm_print (BKM_LINE line)
{
	BOOL ci = FALSE;
	int len = line->Text_ln;
	while (line->Text[len-1] == '\n') len--;
	printf ("%c", (line->Type ? line->Type : '?'));
	if (line->Class_ln&&line->Class)
		ci |= printf (" cl='%.*s'", (int)line->Class_ln, line->Class);
	if (line->Id_ln&&line->Id)
		ci |= printf (" id='%.*s'", (int)line->Id_ln, line->Id);
	printf ("%s  '%.*s'\n", (ci ? "\n " : ""), len, line->Text);
}

/*----------------------------------------------------------------------------*/
#if 0
static void
bkm_dump_ (const char * func)
{
	BKM_LINE line = bkm_list_beg;
	puts ("________________________________________"
	      "________________________________________");
	if (func) {
		puts(func);
		printf ("%.*s\n", (int)strlen(func)+1, "-------------------------------");
	}
	while (line) {
		bkm_print (line);
		line = line->Next;
	}
}
# define bkm_dump(func) bkm_dump_(func)
#endif
#endif /* bkm_print */

#ifndef bkm_dump
# define bkm_dump(func)
#endif

/*----------------------------------------------------------------------------*/
static void
bkm_insert (BKM_LINE line, BKM_LINE prev)
{
	if (prev) {
		line->Prev   = prev;
		line->Next   = prev->Next;
		prev->Next   = line;
	} else {
		line->Prev   = NULL;
		line->Next   = bkm_list_beg;
		bkm_list_beg = line;
	}
	if (line->Next) {
		line->Next->Prev = line;
	} else {
		bkm_list_end     = line;
	}
}

/*----------------------------------------------------------------------------*/
static void
bkm_remove (BKM_LINE line)
{
	if (line->Prev) line->Prev->Next = line->Next;
	else            bkm_list_beg     = line->Next;
	if (line->Next) line->Next->Prev = line->Prev;
	else            bkm_list_end     = line->Prev;
}

/*----------------------------------------------------------------------------*/
static BKM_LINE
bkm_create (BKM_LINE prev, const char * text)
{
	size_t   len  = (text ? strlen (text) : 0);
	BKM_LINE line;
	while (len && isspace (text[len-1])) {
		len--;
	}
	if ((line = malloc (sizeof (struct s_bkm_line) + len)) != NULL) {
		if (len) {
			memcpy (line->Buff, text, len);
			line->Buff[len++] = '\n';
			line->Buff[len]   = '\0';
			line->Text = line->Buff;
			while (isspace (*line->Text)) {
				line->Text++;
				len--;
			}
			line->Text_ln = len;
			line->Type    = (*line->Text == '<' ? '\0' : '*');
		} else {
			line->Buff[0] = '\n';
			line->Buff[1] = '\0';
			line->Text    = line->Buff +1;
			line->Text_ln = len;
			line->Type    = '*';
		}
		line->Id       = NULL;
		line->Id_ln    = 0;
		line->Class    = NULL;
		line->Class_ln = 0;
		bkm_insert (line, prev);
	}
	return line;
}

/*----------------------------------------------------------------------------*/
static void
bkm_delete (BKM_LINE line)
{
	if (line) {
		bkm_remove (line);
		free (line);
	} 
}

/*----------------------------------------------------------------------------*/
static void
bkm_clear (void)
{
	 while (bkm_list_beg) {
		BKM_LINE next = bkm_list_beg->Next;
		free (bkm_list_beg);
		bkm_list_beg = next;
	}
	bkm_list_end = NULL;
}

/*----------------------------------------------------------------------------*/
static void
bkm_flush (void)
{
	FILE * file = (bkm_list_beg ? open_bookmarks ("wb") : NULL);
	if (file) {
		struct stat st;
		union { const char * c; char * v; } u;
		BKM_LINE line = bkm_list_beg;
		while (line) {
			fputs (line->Buff, file);
			line = line->Next;
		}
		fclose (file);
		u.c = bkm_File;
		stat (u.v, &st);
		bkm_list_tm = st.st_mtime;
	}
}

/*----------------------------------------------------------------------------*/
static char
bkm_check (BKM_LINE line)
{
	char * p = line->Text;
	
	if (strncmp (p, "</DL>\n", 6) == 0) {
		line->Type  = 'e';
		return line->Type;
	}
	
	if (*(p++) != '<' || *(p++) != 'D' || (*p != 'T' && *p != 'L')
	                  || strncmp (++p, " CLASS='", 8) != 0) {
		line->Type  = '*';
		return line->Type;
	}
	
	line->Class = p += 8;
	while (*p && *p != '\'') {
		p++;
	}
	if ((line->Class_ln = p - line->Class) == 0) {
		line->Class = NULL;
	}
	if (line->Class && line->Text[2] == 'L') {
		line->Type  = 'b';
		return line->Type;
	}
	if (!line->Class || line->Text[2] != 'T'
	                 || strncmp (p, "' ID='", 6) != 0) {
	
		line->Type  = '*';
		return line->Type;
	}
	
	line->Id = p += 6;
	while (*p && *p != '\'') {
		p++;
	}
	if ((line->Id_ln = p - line->Id) == 0) {
		line->Id   = NULL;
		line->Type = '*';
		return line->Type;
	}
	
	if (strncmp (line->Class, "LNK'", 4) == 0) {
	    if ((p = strchr (p +1, '>'))   != NULL &&
	        (p = strstr (p +1, "<A ")) != NULL &&
	        (p = strchr (p +3, '>'))   != NULL) {
	    	char * q = strchr (p += 1, '<');
	    	if (q > p) {
	    		line->Text    = p;
	    		line->Text_ln = q - p;
	    		line->Type    = 'B';
	    		return line->Type;
		    }
	    }
	
	} else if (strncmp (line->Class, "GRP'", 4) == 0) {
	    if ((p = strchr (p +1, '>'))   != NULL &&
	        (p = strstr (p +1, "<B>")) != NULL) {
	    	char * q = strchr (p += 3, '<');
	    	if (q > p) {
	    		line->Text    = p;
	    		line->Text_ln = q - p;
	    		line->Type    = 'G';
	    		return line->Type;
		    }
	    }
	}
	
	return (line->Type = '*');
}

/*----------------------------------------------------------------------------*/
static BKM_LINE
bkm_search (BKM_LINE line, const char * id, BOOL dnNup)
{
	size_t len = strlen (id ? id : (id = "</DL>"));
	
	if (bkm_File && !line && (bkm_list_beg || !bkm_list_tm)) {
		struct stat st;
		union { const char * c; char * v; } u;
		u.c = bkm_File;
		stat  (u.v, &st);
		if (bkm_list_tm != st.st_mtime) {
			bkm_list_tm = st.st_mtime;
			bkm_clear();
		}
	}
	if (!bkm_list_beg) {
		FILE * file = open_bookmarks ("rb");
/*puts("!bkm_list_beg");*/
		if (file) {
			char buff[1024];
			BKM_LINE temp = NULL;
			while (fgets (buff, (int)sizeof(buff), file)) {
				if ((temp = bkm_create (temp, buff)) == NULL) {
					bkm_clear();
					puts ("bkm_search(): memory exhausted!");
					break;
				}
			}
			fclose (file);
			if ((temp = bkm_list_beg) != NULL) do {
				if (!temp->Type && strncmp (temp->Text, "<DL", 3) == 0) {
					temp->Type = '!';
					break;
				}
			} while ((temp = temp->Next) != NULL);
			if ((temp = bkm_list_end) != NULL) do {
				if (!temp->Type && strncmp (temp->Text, "</DL>", 5) == 0) {
					temp->Type = '!';
					break;
				}
			} while ((temp = temp->Prev) != NULL);
		}
		line = NULL;
	}
	
	if (strcmp (id, "*") == 0) { /*............ just get the next/prev element */
		line = (dnNup ? line ? line->Next : bkm_list_beg
		              : line ? line->Prev : bkm_list_end);
		if (line && !line->Type) {
			bkm_check (line);
		}
		return line;
		
	} else if (strcmp (id, "?") == 0) { /*.............. search for any 'type' */
		line = (dnNup ? line ? line->Next : bkm_list_beg
		              : line ? line->Prev : bkm_list_end);
		while (line) {
			if (isalpha (line->Type ? line->Type : bkm_check (line))) {
				return line;
			}
			line = (dnNup ? line->Next : line->Prev);
		}
		
	} else if (id[0] && !id[1]) { /*.................. search for exact 'type' */
		if (!line) {
			line = (dnNup ? bkm_list_beg : bkm_list_end);
		}
		while (line) {
			if ((line->Type ? line->Type : bkm_check (line)) == id[0]) {
				return line;
			}
			line = (dnNup ? line->Next : line->Prev);
		}
		
	} else { /*.................................. normal case, search for 'id' */
		if (!line) {
			line = (dnNup ? bkm_list_beg : bkm_list_end);
		}
		while (line) {
			if (!line->Type) {
				bkm_check (line);
			}
			if (line->Id && line->Id_ln == len
			             && strncmp (line->Id, id, len) == 0) {
				return line;
			}
			line = (dnNup ? line->Next : line->Prev);
		}
	}
	return NULL;
}


/******************************************************************************/


static const char tmpl_head[] =
	BKM_MAGIC "\n"
	"<HTML><HEAD>\n"
	"  <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;"
	         " charset=atarist\">\n"
	"  <TITLE>Bookmarks</TITLE>\n"
	"  <LINK REL=\"stylesheet\" HREF=\"bookmark.css\" TYPE=\"text/css\""
	         "  MEDIA=\"screen\">\n"
	"</HEAD>\n";
static const char tmpl_tail[] =
	 "</DL>\n"
	 "</BODY></HTML>\n";

static const char tmpl_css[] =
	"BODY   { background-color: white; }\n"
	"DL DL  { border-width: 0px 1px 1px; border-color: A0A0A0;"
	        " border-style: solid;\n"
	"         margin-bottom: 1px; margin-left: 1em; padding: 0px 3px; }\n"
	"DT.GRP { white-space:nowrap; background-color: E0E0E0;"
	        " padding: 0px 3px 1px 3px;\n"
	"         border-width: 1px; border-style: solid; }\n"
	"DT.LNK { text-indent: -2em }\n"
	"A      { text-decoration: none; }\n"
	"\n";

/*============================================================================*/
BOOL
read_bookmarks (void) {
	FILE * file = NULL;
	if ((file = open_bookmarks ("r")) != NULL) { 
		size_t len = strchr (tmpl_head, '>') - tmpl_head +1;
		char   buff[1024];
		if (!fgets (buff, (int)sizeof(buff), file)) {
			buff[0] = '\n';
		}
		fclose (file);
		if (strncmp (buff, tmpl_head, len) != 0) {
			char * bak_file = (bkm_File ? strdup (bkm_File) : NULL);
			if (bak_file) {
				const char * bak_name = "book-old.htm";
				char       * ptr      = strrchr (bak_file, '\\');
				if (ptr) {
					strcpy (++ptr, bak_name);
					unlink (bak_file);
					if (rename (bkm_File, bak_file) == E_OK) {
						union { const char * c; char * v; } u;
						u.c = bkm_File;
						free (u.v);
						bkm_File = NULL;
						if ((file = open_bookmarkCss ("r")) != NULL) {
							fclose (file);
						}
						if (bkm_CSS) {
							unlink (bkm_CSS);
							u.c = bkm_CSS;
							free (u.v);
							bkm_CSS = NULL;
						}
						hwUi_info (NULL, "Outdated '%s'\nsaved as '%s'",
						           _HIGHWIRE_BOOKMARK_, bak_name);
					}
				}
				free (bak_file);
			}
			file = NULL;
		}
/*		bkm_search (NULL, "", TRUE);*/
		bkm_dump ("bkm_search");
	}
	if (!file && (file = open_bookmarks ("wb")) != NULL) {
		long now = time (NULL) -5;
		char buff[1024];
		fputs (tmpl_head, file);
		fprintf (file, "<BODY ID='%08lX'>\n", now -1);
		fputs ("<DL>\n", file);
		fputs (strcat (wr_grp (buff, now++, "HighWire Project"), "\n"), file);
		fputs (strcat (wr_lnk (buff, now++, 0L,
		       "http://highwire.atari-users.net", "HighWire Homepage"),
		       "\n"), file);
		fputs (strcat (wr_lnk (buff, now++, 0L,
		       "http://www.atariforums.com/index.php?20", "HighWire Forum"),
		       "\n"), file);
		fputs (strcat (wr_lnk (buff, now++, 0L,
		       "http://www.atari-users.net/mailman/listinfo/highwire",
		       "Developers Mailing lists"), "\n"), file);
		fputs (strcat (wr_lnk (buff, now++, 0L,
		       "http://www.atari-users.net/mailman/listinfo/highwire-users",
		       "Users Mailing lists"), "\n"), file);
		fputs (strcat (wr_lnk (buff, now++, 0L,
		       "http://highwire.atari-users.net/mantis/", "Bugtracker"),
		       "\n"), file);
		fputs ("</DL>\n", file);
		fputs (tmpl_tail, file);
		fclose(file);
		
		if ((file = open_bookmarkCss ("r")) != NULL) {
			/* Bookmarks CSS exists, read or parse or load? */
			fclose(file);
		
		} else if ((file = open_bookmarkCss ("w")) != NULL) {
			/* The idea is that we can put a display none in to hide DL sets */
			fputs (tmpl_css, file);
			fclose(file);
		}
	}
	return TRUE;
}

/*============================================================================*/
BOOL
save_bookmarks (void)
{
	bkm_flush();
	bkm_clear();
	return TRUE;
}


/*============================================================================*/
BOOL
add_bookmark (const char * url, const char * title)
{
	BOOL       done = FALSE;
	BKM_LINE   line = bkm_search (NULL, "?", FALSE);
	if (!line) line = bkm_search (NULL, "!", TRUE); /* empty file? */
	if (line) {
		long now = time (NULL);
		char buff[1024];
		if (bkm_create (line, wr_lnk (buff, now, now, url, title))) {
			bkm_flush();
			done = TRUE;
		}
	}
	return done;
}

/*============================================================================*/
BOOL
del_bookmark (const char * lnk)
{
	BOOL     done = FALSE;
	BKM_LINE line = bkm_search (NULL, lnk, TRUE);
	if (line && line->Type == 'B') {
		bkm_delete (line);
		bkm_flush();
		done = TRUE;
	}
	return done;
}

/*============================================================================*/
BOOL
pos_bookmark (const char * id,  BOOL dnNup)
{
	BOOL     done = FALSE;
	BKM_LINE line = bkm_search (NULL, id, !dnNup);
	if (line && line->Type == 'B') {
		BKM_LINE othr = bkm_search (line, "?", dnNup);
		if (othr->Type) switch (othr->Type) {
			case 'G':
				if (!dnNup) othr = othr->Prev;
				else        othr = bkm_search (othr, "b", TRUE);
				break;
			case 'b':
				if (!dnNup && (othr = bkm_search (othr, "G", FALSE)) == NULL) break;
			case 'e':
			case 'B':
				if (!dnNup) othr = othr->Prev;
				break;
			default:
				othr = NULL;
		}
		if (othr) {
			bkm_remove (line);
			bkm_insert (line, othr);
			bkm_flush();
			done = TRUE;
		}
	}
	return done;
}

/*============================================================================*/
BOOL
txt_bookmark (const char * id, char * rw_buf, size_t lenNwr)
{
	BOOL     done = FALSE;
	BKM_LINE line = bkm_search (NULL, id, TRUE);
	if (line) {
		if (lenNwr) {
			size_t len = min (line->Text_ln, lenNwr -1);
			memcpy (rw_buf, line->Text, len);
			rw_buf[len] = '\0';
			done = TRUE;
		} else {
			size_t len = strlen (rw_buf);
			if (line->Text_ln == len) {
				memcpy (line->Text, rw_buf, len);
				bkm_flush();
				done = TRUE;
			} else {
				char   buff[1024];
				size_t n = line->Text - line->Buff;
				memcpy (buff,           line->Buff, n);
				memcpy (buff + n,       rw_buf,     len);
				strcpy (buff + n + len, line->Text + line->Text_ln);
				if (bkm_create (line, buff)) {
					bkm_delete (line);
					bkm_flush();
					done = TRUE;
				}
			}
		}
	}
	return done;
}


/*============================================================================*/
BOOL
add_bookmark_group (const char * lnk, const char *title)
{
	BOOL     done = FALSE;
	BOOL     ins  = (lnk && *lnk);
	BKM_LINE line = (ins ? bkm_search (NULL, lnk,  TRUE)
	                     : bkm_search (NULL, "!", FALSE));
	if (line) {
		long now = time (NULL);
		char buff[1024], * p;
		BKM_LINE group;
		char     id[12];
		if (!title) {
			sprintf (id, "G:%08lX", now);
			title = id;
		}
		if ((p = strchr (wr_grp (buff, now, title), '\n')) != NULL) {
			*(p++) = '\0';
		}
		if ((group = bkm_create (line->Prev, buff)) != NULL) {
			BKM_LINE beg = (p ? bkm_create (line->Prev, p) : group);
			if (bkm_create ((ins ? line : beg), "</DL>")) {
				bkm_flush();
				done = TRUE;
			} else {
				if (beg != group) {
					bkm_delete (beg);
				}
				bkm_delete (group);
			}
		}
		bkm_dump ("add_bookmark_group");
	}
	return done;
}

/*============================================================================*/
BOOL
del_bookmark_group (const char * grp)
{
	BOOL     done = FALSE;
	size_t   len  = (grp ? strlen (grp) : 0l);
	BKM_LINE line = (len ? bkm_search (NULL, grp, TRUE) : NULL);
	if (line && line->Type == 'G') {
		BKM_LINE beg = line->Next;
		BKM_LINE end = (beg ? bkm_search (beg, "e", TRUE) : NULL);
/*printf ("%p %p  %li|%.*s| %li|%.*s| %li|%.*s|%c\n", beg,end, len,(int)len,grp,
        line->Id_ln, (int)line->Id_ln, line->Id,
        beg->Class_ln, (int)beg->Class_ln, beg->Class, beg->Type);*/
/*printf ("   %li|%.*s|\n", beg->Text_ln, (int)beg->Text_ln-1, beg->Text);*/
		if (end && beg->Class_ln == len && strncmp (beg->Class, grp, len) == 0) {
			bkm_delete (line);
			bkm_delete (beg);
			bkm_delete (end);
			set_bookmark_group (grp, TRUE);
			bkm_flush();
			done = TRUE;
		}
		bkm_dump ("del_bookmark_group");
	}
	return done;
}

/*----------------------------------------------------------------------------*/
typedef struct s_fline * FLINE;
struct s_fline {
	FLINE Next;
	char  Text[1];
};
/* - - - - - - - - - - - - - - - - - - - - - */
static FLINE
add_line (FLINE ** base, char * buff)
{
	size_t len  = strlen (buff);
	FLINE  line = malloc (sizeof (struct s_fline) + len);
	if (line) {
		memcpy (line->Text, buff, len +1);
		line->Next = **base;
		**base     = line;
		*base      = &line->Next;
	}
	return line;
}

/*==============================================================================
 * collaps/expand a bookmark group
*/
BOOL
set_bookmark_group (const char * grp, BOOL openNclose)
{
	BOOL   done = FALSE;
	FILE * file = open_bookmarkCss ("rb+");
	if (file) {
		char mark[1024];
		int   m_ln = sprintf (mark, "DL.%.*s", (int)sizeof(mark) -4, grp);
		FLINE list = NULL, * pptr = &list;
		char buff[1024], * p;
		while ((p = fgets (buff, (int)sizeof(buff), file)) != NULL) {
			size_t len = strlen (p);
			while (len && isspace (p[len-1])) p[--len] = '\0';
			while (isspace(*p)) p++;
			if (strncmp (p, mark, m_ln) == 0 && isspace (p[m_ln])) {
				/* skip this */
				done = TRUE;
			} else if (!add_line (&pptr, buff)) {
				fclose (file);
				file = NULL;
				done = FALSE; /* error case */
			}
		}
		if (done && file) {   /* rewrite the file */
			fclose (file);
			if ((file = open_bookmarkCss ("wb")) != NULL) {
				FLINE line = list;
				while (line) {
					fprintf (file, "%s\n", line->Text);
					line = line->Next;
				}
			} else {
				done = FALSE; /* error case */
			}
		}
		while (list) {
			FLINE next = list->Next;
			free (next);
			list = next;
		}
		if (file) {	
			if (!openNclose) {	
				fputs (mark, file);
				fputs ("\t{ display: none; }\n", file);
				done = TRUE;
			}
			fclose (file);
		}
	}
	return done;
}
