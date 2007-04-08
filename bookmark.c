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
# ifdef __GNUC__
#  include <unistd.h>
# endif
#endif

#include "global.h"
#include "bookmark.h"


#define BKM_MAGIC "<!DOCTYPE HighWire-Bookmark-File-3>"

const char * bkm_File = NULL;
const char * bkm_CSS  = NULL;


/*----------------------------------------------------------------------------*/
#define open_bookmarkCss(mode) open_default (&bkm_CSS, _HIGHWIRE_BKM_CSS_, mode)
#define open_bookmarks(mode)   open_default (&bkm_File,_HIGHWIRE_BOOKMARK_,mode)


/*============================================================================*/
long
bookmark_id2date (const char * id, char * buff, size_t b_len)
{
	time_t dt = (id && (*id == 'L' || *id == 'G') && id[1] == ':'
	             ? (strtoul (id +2, NULL, 16) >>2) | 0x40000000ul : 0ul);
	if (buff && b_len) {
		char   buf[80];
		size_t len;
		if (dt) {
			struct tm * tm  = localtime (&dt);
			len = sprintf (buf, "%04i-%02i-%02i %02i:%02i:%02i",
			               tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
			               tm->tm_hour, tm->tm_min, tm->tm_sec);
		} else {
			len = strlen (strcpy (buf, "<invalid>"));
		}
		if (len >= b_len) len = b_len -1;
		memcpy (buff, buf, len);
		buff[len] = '\0';
	}
	return dt;
}

/*----------------------------------------------------------------------------*/
static ULONG
get_id (void)
{
	static ULONG _id = 0uL;
	ULONG        id  = time(NULL) <<2;
	return (_id = (id > _id ? id : _id +1));
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
#if 01
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
bkm_create (BKM_LINE prev, const char * text, size_t extra)
{
	size_t   len  = strlen (text ? text : (text = ""));
	BKM_LINE line;
	while (len && isspace (text[len-1])) {
		len--;
	}
	if ((line = malloc (sizeof (struct s_bkm_line) + len + extra)) != NULL) {
		line->Text = memcpy (line->Buff, text, len);
		if (!extra) {
			line->Buff[len +0] = '\n';
			line->Buff[len +1] = '\0';
		} else {
			line->Buff[len +0] = '\0';
		}
		while (isspace (*line->Text)) {
			line->Text++;
			len--;
		}
		line->Text_ln  = len;
		line->Type     = (extra || *line->Text == '<' ? '\0' : '*');
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
static BKM_LINE
bkm_create_grp (BKM_LINE prev, const char * title)
{
	ULONG    id_class = get_id();
	char     b1[100];
	size_t   l1   = sprintf (b1, "<DT CLASS='GRP' ID='G:%08lX'><B>", id_class);
	size_t   l2   = strlen (title && *title ? title : (title = "?¨?¨?"));
	char     b3[] = "</B></DT>\n";
	BKM_LINE line = bkm_create (prev, NULL, l1 + l2 + sizeof(b3)-1);
	if (line) {
		char * p = line->Buff;
		memcpy (p,       b1,    l1);
		memcpy (p += l1, title, l2);
		strcpy (p += l2, b3);
		sprintf (b1, "<DL CLASS='G:%08lX'>", id_class);
		if ((line = bkm_create ((prev = line), b1, 0)) == NULL) {
			bkm_delete (prev);
		}
	}
	return line;
}

/*----------------------------------------------------------------------------*/
static BKM_LINE
bkm_create_lnk (BKM_LINE prev, const char * title, const char * url, long visit)
{
	ULONG    id = get_id();
	char     b1[100];
	size_t   l1   = sprintf (b1, "<DT CLASS='LNK' ID='L:%08lX'>"
	                             "<A TARGET='_hw_top' LAST_VISIT='%08lX'"
	                             " HREF='", id, visit);
	size_t   l2   = strlen (url && *url ? url : (url = "#"));
	char     b3[] = "'>";
	size_t   l3   = sizeof(b3) -1;
	size_t   l4   = strlen (title && *title ? title : (title = "?¨?¨?"));
	char     b5[] = "</A></DT>\n";
	BKM_LINE line = bkm_create (prev, NULL, l1 + l2 + l3 + l4 + sizeof(b5)-1);
	if (line) {
		char * p = line->Buff;
		memcpy (p,       b1,    l1);
		memcpy (p += l1, url,   l2);
		memcpy (p += l2, b3,    l3);
		memcpy (p += l3, title, l4);
		strcpy (p += l4, b5);
	}
	return line;
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
	size_t len = (id ? strlen (id) : 0);
	
	if (bkm_File && !line && (bkm_list_beg || !bkm_list_tm)) {
		struct stat st;
		union { const char * c; char * v; } u;
		u.c = bkm_File;
		if (stat (u.v, &st) == E_OK && bkm_list_tm != st.st_mtime) {
			bkm_list_tm = st.st_mtime;
			bkm_clear();
		}
	}
	if (!bkm_list_beg) {
		char * buff = NULL;
		FILE * file = NULL;
		struct stat st;
		union { const char * c; char * v; } u;
		u.c = bkm_File;
		if (stat (u.v, &st) == E_OK && st.st_size > 0
		    && (buff = malloc (st.st_size +1)) != NULL
		    && (file = open_bookmarks ("rb"))  != NULL
		    && fread  (buff, 1, st.st_size, file)) {
			char * beg = buff;
			buff[st.st_size] = '\0';
			line = NULL;
			while (*beg) {
				char    * end = strchr (beg, '\n');
				if (!end) end = strchr (beg, '\0');
				*end = '\0';
				if ((line = bkm_create (line, beg, 0)) == NULL) {
					bkm_clear();
					puts ("bkm_search(): memory exhausted!");
					break;
				}
				beg = end +1;
			}
		}
		if (file) fclose (file);
		if (buff) free   (buff);
		if ((line = bkm_list_beg) != NULL) do {
			if (!line->Type && strncmp (line->Text, "<DL", 3) == 0) {
				line->Type = '!';
				break;
			}
		} while ((line = line->Next) != NULL);
		if ((line = bkm_list_end) != NULL) do {
			if (!line->Type && strncmp (line->Text, "</DL>", 5) == 0) {
				line->Type = '!';
				break;
			}
		} while ((line = line->Prev) != NULL);
		line = NULL;
	}
	
	if (!len) { /*.............................................................*/
		return NULL;
		
	} else if (strcmp (id, "*") == 0) { /*..... just get the next/prev element */
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
read_bookmarks (char ** old_file)
{
	BOOL   ok   = TRUE;
	FILE * file = NULL;
	if (old_file) {
		*old_file = NULL;
	}
	if ((file = open_bookmarks ("r")) != NULL) { 
		size_t len = strchr (tmpl_head, '>') - tmpl_head +1;
		char   buff[1024];
		if (!fgets (buff, (int)sizeof(buff), file)) {
			buff[0] = '\0';
		}
		fclose (file);
		if (strncmp (buff, tmpl_head, len) == 0) {
			ok = TRUE; /* version matches */
		
		} else {
			char * bak_file = (bkm_File ? strdup (bkm_File) : NULL);
			if (bak_file) {
				const char * bak_name = "book-old.htm";
				char       * ptr      = strrchr (bak_file, '\\');
				if (!ptr)    ptr      = strrchr (bak_file, '/');
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
					}
				}
				if (old_file) {
					*old_file = bak_file;
				} else {
					free (bak_file);
				}
			}
			file = NULL;
		}
	}
	if (!file) {
		char buff[30];
		BKM_LINE line = NULL;
		sprintf (buff, "<BODY ID='%08lX'>", get_id());
		line = bkm_create     (line, tmpl_head, 0uL);
		line = bkm_create     (line, buff,      0uL);
		line = bkm_create     (line, "<DL>",    0uL);
		line = bkm_create_grp (line, "HighWire Project");
		line = bkm_create_lnk (line, "HighWire Homepage",
		        "http://highwire.atari-users.net", 0L);
		line = bkm_create_lnk (line, "HighWire Forum",
		        "http://www.atariforums.com/index.php?20", 0L);
		line = bkm_create_lnk (line, "Developers Mailing lists",
		        "http://www.atari-users.net/mailman/listinfo/highwire", 0L);
		line = bkm_create_lnk (line, "Users Mailing lists",
		        "http://www.atari-users.net/mailman/listinfo/highwire-users", 0L);
		line = bkm_create_lnk (line, "Bugtracker",
		        "http://highwire.atari-users.net/mantis/", 0L);
		line = bkm_create     (line, "</DL>",   0uL);
		line = bkm_create     (line, tmpl_tail, 0uL);
		bkm_flush();
		bkm_clear();
		ok = TRUE; /* version matches now */
		
		if ((file = open_bookmarkCss ("r")) != NULL) {
			/* Bookmarks CSS exists, read or parse or load? */
			fclose(file);
		
		} else if ((file = open_bookmarkCss ("w")) != NULL) {
			/* The idea is that we can put a display none in to hide DL sets */
			fputs (tmpl_css, file);
			fclose(file);
		}
	}
	return ok;
}

/*============================================================================*/
BOOL
save_bookmarks (void)
{
	bkm_flush();
	bkm_clear();
	return TRUE;
}

/*==============================================================================
 * extract useable links from the file 'name' and add them to the bookmark file
*/
BOOL
pick_bookmarks (const char * name,
                void (*progress)(ULONG, ULONG, const char *, const char *))
{
	BOOL     done = FALSE;
	BKM_LINE line = NULL;
	ULONG    size = 0ul;
	char   * fbuf = NULL;
	char   * p    = NULL;
	char   * pend = NULL;
	
	if (name && *name) {
		struct stat st;
		union { const char * c; char * v; } u;
		u.c = name;
		st.st_size = 0;
		if (stat (u.v, &st) == E_OK && (size = st.st_size) > 0
		    && (fbuf = malloc (max (size, 100) +1)) != NULL) {
			FILE * file;
			sprintf (fbuf, "Import: '%.90s'",
			         (p = strrchr (name, '\\')) != NULL ? p +1 :
			         (p = strrchr (name, '/') ) != NULL ? p +1 : name);
			if (progress) (*progress)(size, 0, NULL, fbuf);
			if ((file = fopen (name, "rb")) != NULL) {
				fread  (fbuf, 1, size, file);
				fclose (file);
				*(pend = (p = fbuf) + size) = '\0';
				do if (*p < ' ') {
					*p = ' ';
				} while (++p < pend);
				p = fbuf;
				if (progress) (*progress)(size, 1, NULL, NULL);
			}
		}
	}
	while (p < pend) {
		char * url = NULL;
		char * txt = NULL;
		char * tag = memchr (p, '<', pend - p);
		if (!tag) {
			if (progress) (*progress)(size, size, NULL, NULL);
			p = pend;
			break;
		}
		if (strnicmp (++tag, "A ", 2) == 0) {
			if (progress) (*progress)(size, p - fbuf, NULL, NULL);
			p = tag +2;
			if ((tag = memchr (p, '>', pend - p)) == NULL) tag  = pend -1;
			else                                           *tag = '\0';
			while (*p && strnicmp (p, "HREF=", 5) != 0) p++;
			if (*p && *(p += 5)) {
				char * end = (*p == '"'  ? strchr (p +1, '"') :
				              *p == '\'' ? strchr (p +1, '\'') : NULL);
				if (end) {
					p++;
				} else if (*p =='/' || isalnum (*p)) { 
					end = p;
					while (*end && !isspace (*(++end)));
				}
				if (end) {
					*end = '\0';
					url  = p;
					if (progress) (*progress)(size, url - fbuf, "", url);
				}
			}
		}
		if (url && (p = strstr (tag +1, "</")) != NULL) {
			while (strnicmp (p +2, "A>", 2) != 0
			       && (p = strstr (p +4, "</")) != NULL);
			if (p) {
				*p  = '\0';
				txt = tag;
				tag = p +4;
				while (isspace(*(++txt)));
				while (p > txt && isspace(*(--p))) *p = '\0';
				if (progress) (*progress)(size, txt - fbuf, txt, NULL);
			}
		}
		if (url) {
			char buff[1024];
			if (!line) {
				sprintf (buff, "Import: '%.90s'",
				         (p = strrchr (name, '\\')) != NULL ? p +1 :
				         (p = strrchr (name, '/') ) != NULL ? p +1 : name);
				if (add_bookmark_group (NULL, buff)) {
					line = bkm_search (NULL, "b", FALSE);
					bkm_flush();
					done = TRUE;
				} else {
					break; /* memory exhausted or file format error */
				}
			}
			line = bkm_create_lnk (line, (txt && *txt ? txt : url), url, 0L);
			if (line) {
				bkm_flush();
				done = TRUE;
			} else {
				break; /* memory exhausted */
			}
		}
		p = tag +1;
		if (progress) (*progress)(size, p - fbuf, NULL, NULL);
	}
	if (fbuf) free (fbuf);
	if (progress) (*progress)(0, 0, NULL, NULL);
	
	return done;
}


/*============================================================================*/
BOOL
add_bookmark (const char * url, const char * title)
{
	BOOL       done = FALSE;
	BKM_LINE   line = bkm_search (NULL, "?", FALSE);
	if (!line) line = bkm_search (NULL, "!", TRUE); /* empty file? */
	if (line) {
		if (bkm_create_lnk (line, title, url, 0L)) {
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
pos_bookmark (const char * lnk,  BOOL dnNup)
{
	BOOL     done = FALSE;
	BKM_LINE line = bkm_search (NULL, lnk, !dnNup);
	if (line && line->Type == 'B') {
		BKM_LINE othr = bkm_search (line, "?", dnNup);
		if (othr) switch (othr->Type) {
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
txt_bookmark_entry (const char * id, char * rw_buf, size_t lenNwr)
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
				char * p1 = line->Buff;
				size_t l1 = line->Text - line->Buff;
				char * p3 = line->Text + line->Text_ln;
				size_t l3 = strlen (p3);
				if ((line = bkm_create (line, NULL, l1 + len + l3)) != NULL) {
					char * p = line->Buff;
					memcpy (p,        p1,     l1);
					memcpy (p += l1,  rw_buf, len);
					strcpy (p += len, p3);
					bkm_delete (line->Prev);
					bkm_flush();
					done = TRUE;
				}
			}
		}
		bkm_dump ("txt_bookmark");
	}
	return done;
}

/*============================================================================*/
BOOL
pos_bookmark_entry (const char * id, const char * other)
{
	BOOL     done = FALSE;
	BKM_LINE line = bkm_search (NULL, id, TRUE);
	if (line) {
		BKM_LINE prev = bkm_search (NULL, (other ? other : "!"), TRUE);
		if (prev) {
			if (prev == line || prev == line->Prev) {
				prev = NULL;
			} else if (prev->Type == 'G') {
				prev = bkm_search (prev, (line->Type == 'G' ? "e" : "b"), TRUE);
			} else if (line->Type == 'G') {
				BKM_LINE temp = line->Prev;
				while (temp) { /* check for other is inside id */
					if (temp->Type == 'b' || temp->Type == 'G') {
						prev = NULL;
						break;
					} else if (temp->Type == 'e' || temp->Type == '!') {
						break;
					}
					temp = temp->Prev;
				}
				temp = prev;
				while (temp) { /* check for other is inside another group */
					if (temp->Type == 'b' || temp->Type == 'G') {
						prev = bkm_search (prev, "e", TRUE);
						break;
					} else if (temp->Type == 'e' || temp->Type == '!') {
						break;
					}
					temp = temp->Prev;
				}
			}
		}
		if (prev) {
			bkm_remove (line);
			if (line->Type == 'G') {
				BKM_LINE temp = line->Next;
				while (temp) {
					bkm_remove (temp);
					if (temp->Type == 'e') {
						temp->Next = NULL;
						break;
					}
					temp = temp->Next;
				}
			} else {
				line->Next = NULL;
			}
			do {
				BKM_LINE temp = line->Next;
				bkm_insert (line, prev);
				prev = line;
				line = temp;
			} while (line);
			bkm_flush();
			done = TRUE;
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
		BKM_LINE group;
		char     id[12];
		if (!title) {
			sprintf (id, "G:%08lX", get_id() -1);
			title = id;
		}
		if ((group = bkm_create_grp (line->Prev, title)) != NULL) {
			if (bkm_create ((ins ? line : group), "</DL>", 0)) {
				bkm_flush();
				done = TRUE;
			} else {
				bkm_delete (group->Prev);
				bkm_delete (group);
			}
		}
		bkm_dump ("add_bookmark_group");
	}
	return done;
}

/*----------------------------------------------------------------------------*/
static BOOL cl_cmp (BKM_LINE grp, const char * cl)
{
	size_t len  = (cl && grp ? strlen (cl) : 0l);
	return (len && grp->Class_ln == len && strncmp (grp->Class, cl, len) == 0);
}

/*============================================================================*/
BOOL
del_bookmark_group (const char * grp)
{
	BOOL     done = FALSE;
	BKM_LINE line = bkm_search (NULL, grp, TRUE);
	if (line && line->Type == 'G') {
		BKM_LINE beg = bkm_search (line, "b", TRUE);
		BKM_LINE end = (cl_cmp (beg, grp) ? bkm_search (beg, "e", TRUE) : NULL);
		if (end) {
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

/*============================================================================*/
BOOL
pos_bookmark_group (const char * grp,  BOOL dnNup)
{
	BOOL     done = FALSE;
	BKM_LINE line = bkm_search (NULL, grp, !dnNup);
	if (line && line->Type == 'G') {
		BKM_LINE beg  = bkm_search (line, "b", TRUE);
		BKM_LINE end  = (cl_cmp (beg, grp) ? bkm_search (beg, "e", TRUE) : NULL);
		BKM_LINE othr = (end ? bkm_search ((dnNup ? end : line), "?", dnNup)
		                     : NULL);
		if (othr) switch (othr->Type) {
			case 'G':
				if (!dnNup) othr = othr->Prev;
				else        othr = bkm_search (othr, "e", TRUE);
				break;
			case 'e':
				if (dnNup || (othr = bkm_search (othr, "G", FALSE)) == NULL) break;
			case 'B':
				if (!dnNup) othr = othr->Prev;
				break;
			default:
				othr = NULL;
		}
		if (othr) {
			bkm_remove (line);
			line->Next = beg;
			do {
				bkm_remove (beg);
			} while ((beg = beg->Next) != end);
			bkm_remove (end);
			end->Next = NULL;
			do {
				beg = line->Next;
				bkm_insert (line, othr);
				othr = line;
			} while ((line = beg) != NULL);
			bkm_flush();
			done = TRUE;
		}
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
