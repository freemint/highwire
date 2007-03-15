/*
 * Bookmark.C
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h> /* isspace() */

#include "file_sys.h"
#include "global.h"
#include "bookmark.h"

#if 0
#define USE_MEM_LISTS
static B_GRP *	group_list  = NULL;
static B_GRP *	current_grp = NULL;
static B_URL *	url_list    = NULL;
static B_URL *	current_url = NULL;

static int Num_Bookmarks = 0;
#endif

const char * bkm_File = NULL;
const char * bkm_CSS  = NULL;


static const char tmpl_head[] =
	"<!DOCTYPE HighWire-Bookmark-file-2>\n"
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
static const char m_dt_grp[] = "DT CLASS='GRP'";
static const char m_dt_lnk[] = "DT CLASS='LNK'";

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


#ifdef USE_MEM_LISTS
/*  ID & CLASS distictions
 *  Url's have ID's and CLASS's
 *  The CLASS is the ID of the Group it is a member of
 */

/*============================================================================*/
void test_bookmarks(void);

void
test_bookmarks(void)
{
	B_URL * url = url_list;
	B_GRP * group = group_list;
	
	while (url != NULL) {
		printf("URL ID %s  \r\n",url->ID);
		printf("URL Class = %s    \r\n",url->Class);
		printf("URL Target = %s    \r\n",url->Target);
		printf("URL Added %ld   \r\n",url->Time_Added);
		printf("URL Last %ld   \r\n",url->Last_Visit);
		printf("URL Address = %s    \r\n",url->Address);
		printf("URL Title = %s    \r\n",url->Title);

		url = url->Next;
	}
	while (group != NULL) {
		printf("ID = %s\r\n",group->ID);
		printf("Added = %ld\r\n",group->Time_Added);
		printf("Title = %s\r\n",group->Title);
	
		group = group->Next;
	}
}
#endif /* USE_MEM_LISTS */

/*----------------------------------------------------------------------------*/
#define open_bookmarkCss(mode) open_default (&bkm_CSS, _HIGHWIRE_BKM_CSS_, mode)
#define open_bookmarks(mode)   open_default (&bkm_File,_HIGHWIRE_BOOKMARK_,mode)


/*----------------------------------------------------------------------------*/
static void
wr_grp (FILE * file, long id_class, const char * title)
{
	/* Should have a call to the bkm_group_ctor() routine */

	fprintf (file, "<%s ID='G:%08lX'><B>%s</B></DT>\n",
	                m_dt_grp, id_class, title);
	fprintf (file, "<DL CLASS='G:%08lX'>\n", id_class);
}

/*----------------------------------------------------------------------------*/
static void
wr_lnk (FILE * file, long id, long visit, const char * url, const char * title)
{
	/* Should have a call to the bkm_url_ctor() routine */
	
	fprintf (file, "<%s ID='L:%08lX'><A TARGET='_hw_top'"
	               " LAST_VISIT='%08lX' HREF='%s'>%s </A></DT>\n",
	               m_dt_lnk, id, visit, url, title);
}

/*============================================================================*/
BOOL
read_bookmarks (void) {
	FILE * file = NULL;
#ifdef USE_MEM_LISTS
	B_GRP * group = NULL;
	B_URL * url = NULL;
	
	Num_Bookmarks = 0;

	/* Bookmarks exists, read or parse or load? */
	if ((file = open_bookmarks ("r")) != NULL) { 
		char  buff[1024];
		char  out[1024];
		char *p;
		int   offset = 0;
		int   len = 0;

		while (fgets (buff, (int)sizeof(buff), file)) {
			if (*buff == '<'
			    && strnicmp (buff +1, m_dt_lnk, sizeof(m_dt_lnk) -1) == 0 ) {
				Num_Bookmarks += 1;
				len = 0;

				p = (char *)buff;

				/* advance past all the junk */
				offset = (int)sizeof(m_dt_lnk);
				offset += 4;

				if (url_list == NULL) {
					url = bkm_url_ctor (malloc (sizeof(B_URL)), url_list);
					url_list = url;
				} else {
					url = bkm_url_ctor (malloc (sizeof(B_URL)), current_url);
				}

				offset += 4; /* advance past ID=" */

				memset (out, 0, 1024);

				strncpy (out,buff+offset,8);
				url->ID = strdup(out);

				offset += 8; /* len of ID */
				offset += 9; /* advance past CLASS=" */
				
				p += offset;
				while (*p != '"') {
					len += 1;
					p++;
				}

				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				url->Class = strdup(out);
				
				offset += len;
				offset += 10; /* advance past TARGET=" */
				p += 10;
				len = 0;

				while (*p != '"') {
					len += 1;
					p++;
				}

				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				url->Target = strdup(out);

				offset += len;
				offset += 12; /* advance past ADD_DATE=" */
				p += 12;
				len = 0;

				while (*p != '"') {
					len += 1;
					p++;
				}
				
				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				url->Time_Added = atol(out);

				offset += len;
				offset += 14; /* advance past LAST_VIST=" */
				p += 14;
				len = 0;

				while (*p != '"') {
					len += 1;
					p++;
				}
				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				url->Last_Visit = atol(out);

				offset += len;
				offset += 8; /* advance past HREF=" */
				p += 8;
				len = 0;

				while (*p != '"') {
					len += 1;
					p++;
				}
				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				url->Address = strdup(out);

				offset += len;
				offset += 2; /* advance past "> */
				p += 2;
				len = 0;

				while (*p != '<') {
					len += 1;
					p++;
				}	
				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				url->Title = strdup(out);

				/* We need some method to find the parent from the Class ?*/
				url->Parent = NULL;  /* just Null it for the moment */

				current_url->Next = url;
				current_url = url;
			} else if (*buff == '<'
			    && strnicmp (buff +1, m_dt_grp, sizeof(m_dt_grp) -1) == 0 ) {
				len = 0;

				p = (char *)buff;

				/* advance past all the junk */
				offset = (int)sizeof(m_dt_grp);
				offset += 12;

				if (group_list == NULL) {
					group = bkm_group_ctor (malloc (sizeof(B_GRP)), group_list);
					group_list = group;
				} else {
					group = bkm_group_ctor (malloc (sizeof(B_GRP)), current_grp);
				}
				offset += 4; /* advance past ID=" */

				p += offset;
				while (*p != '"') {
					len += 1;
					p++;
				}
				memset (out, 0, len+1);

				strncpy (out,buff+offset,len);
				group->ID = strdup(out);

				offset += len;
				offset += 12; /* advance past ID and " CLASS=" */
				p += 12;
				len = 0;
				
				while (*p != '"') {
					len += 1;
					p++;
				}
				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				group->Time_Added = atol(out);

				offset += len;
				len = 0;

				while (*p != '>') {
					len += 1;
					p++;
				}	
				offset += len;
				len = 0;

				offset += 1; /* advance past > */
				p += 1;

				while (*p != '<') {
					len += 1;
					p++;
				}	
				memset (out, 0, len+1);

				strncpy (out, buff+offset,len);
				group->Title = strdup(out);

				current_grp->Next = group;
				current_grp = group;
			} else if (strnicmp(buff, "</HTML>", 7) == 0 ) {
				break;
			}
		}
		fclose (file);
#else /* !USE_MEM_LISTS */
	
	/* Bookmarks exists, read or parse or load? */
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
#endif /* !USE_MEM_LISTS */
		
	}
	if (!file && (file = open_bookmarks ("wb")) != NULL) {
		long now = time (NULL) -5;
		fputs (tmpl_head, file);
		fprintf (file, "<BODY ID='%08lX'>\n", now -1);
		fputs ("<DL>\n", file);
		wr_grp (file, now++, "HighWire Project");
		wr_lnk (file, now++, 0L,
		        "http://highwire.atari-users.net", "HighWire Homepage");
		wr_lnk (file, now++, 0L,
		        "http://www.atariforums.com/index.php?20", "HighWire Forum");
		wr_lnk (file, now++, 0L,
		        "http://www.atari-users.net/mailman/listinfo/highwire",
		        "Developers Mailing lists");
		wr_lnk (file, now++, 0L,
		        "http://www.atari-users.net/mailman/listinfo/highwire-users",
		        "Users Mailing lists");
		wr_lnk (file, now++, 0L,
		        "http://highwire.atari-users.net/mantis/", "Bugtracker");
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
	
#ifdef USE_MEM_LISTS
	/* enable to dump bookmarks structs to screen */
	/* test_bookmarks(); */
#endif /* USE_MEM_LISTS */
	
	return TRUE;
}

/* <OBJECT DATA=\"data:\"></OBJECT> */

/*============================================================================*/
BOOL
save_bookmarks (const char * key)
{
	(void) key;
	
	/* If we store the URL's internally we will need to save when we close */
	return TRUE;
}


/* we need a method to add a bookmark to a group and delete a bookmark
 * from a group
 */


/*----------------------------------------------------------------------------*/
typedef struct s_fline * FLINE;
struct s_fline {
	FLINE Next;
	char  Text[1];
};

/*------------------------------------------------------------------------------
 * read normalized line from file
*/
static char *
get_line (FILE * file, char * buff, size_t b_sz)
{
	char * p = fgets (buff, (int)b_sz, file);
	if (p) {
		size_t len = strlen (p);
		while (len && isspace (p[--len])) p[len] = '\0';
		while (isspace(*p)) p++;
	}
	return p;
}

/*----------------------------------------------------------------------------*/
static FLINE
add_line (FLINE ** base, char * buff)
{
	size_t len  = strlen (buff);
	FLINE  line = malloc (sizeof (struct s_fline) + len);
	if (line) {
		memcpy (line->Text, buff, len +1);
		line->Next = NULL;
		**base     = line;
		*base      = &line->Next;
	}
	return line;
}


/*----------------------------------------------------------------------------*/
#define chk_lnk(p)   (*p=='<' && strncmp(p+1,m_dt_lnk,sizeof(m_dt_lnk)-1)==0)

/*----------------------------------------------------------------------------*/
static BOOL
cmp_id (const char * ptr, const char * id, size_t len)
{
	return (strncmp (ptr, "ID='", 4) == 0 &&
	        strncmp (ptr +4, id, len) == 0 && ptr[4 + len] == '\'');
}


/*============================================================================*/
BOOL
add_bookmark (const char * bookmark_url, const char *bookmark_title)
{
	FILE * file = open_bookmarks ("rb+");
	if (file) {
		long now = time (NULL);
		fseek (file, -(sizeof(tmpl_tail) -1), SEEK_END);
		wr_lnk (file, now, now, bookmark_url, bookmark_title);
		fputs (tmpl_tail, file);
		fclose(file);
		
		return TRUE;
		
	} else {
		printf("Bookmark file lost?!? \n");
		return FALSE;
	}
}

/*============================================================================*/
BOOL
del_bookmark (const char * lnk)
{
	BOOL   done = FALSE;
	FILE * file = open_bookmarks ("rb");
	if (file) {
		FLINE  list = NULL, * pptr = &list;
		size_t len  = strlen (lnk);
		char   buff[1024], * p;
		while ((p = get_line (file, buff, (int)sizeof(buff))) != NULL) {
			if (chk_lnk(p)) {
				char * q = p +1 + sizeof(m_dt_lnk);
				while (isspace(*q)) q++;
				if (cmp_id (q, lnk, len)) {
					done = TRUE;
					p    = NULL;
				}
			}
			if (p && !add_line (&pptr, buff)) {
				done  = FALSE; /* error case */
				break;
			}
		}
		fclose (file);
		
		if (done) {   /* rewrite the file */
			if ((file = open_bookmarks ("wb")) != NULL) {
				FLINE line = list;
				while (line) {
					fprintf (file, "%s\n", line->Text);
					line = line->Next;
				}
				fclose (file);
			
			} else {
				done = FALSE; /* error case */
			}
		}
		while (list) {
			FLINE next = list->Next;
			free (next);
			list = next;
		}
	}
	return done;
}


/*============================================================================*/
BOOL
add_bookmark_group (const char * lnk)
{
	BOOL   done = FALSE;
	FILE * file = open_bookmarks ("rb+");
	if (file) {
		FLINE list = NULL;
		
		if (!lnk || !*lnk) {
			fseek (file, -(sizeof(tmpl_tail) -1), SEEK_END);
			done  = TRUE;
	
		} else {
			FLINE * pptr = &list;
			size_t  len  = strlen (lnk);
			long    fpos = fpos = ftell (file);
			char    buff[1024], * p;
			while ((p = get_line (file, buff, (int)sizeof(buff))) != NULL) {
				if (!done && chk_lnk(p)) {
					char * q = p +1 + sizeof(m_dt_lnk);
					while (isspace(*q)) q++;
					done = cmp_id (q, lnk, len);
				}
				if (!done) {
					fpos = ftell (file);
				} else if (!add_line (&pptr, buff)) {
					done  = FALSE; /* error case */
					break;
				}
			}
			if (done) {
				fseek (file, fpos, SEEK_SET);
			}
		}
		if (done) {
			FLINE line = list;
			long  now  = time (NULL);
			char  id[12];
			sprintf (id, "G:%08lX", now);
			wr_grp (file, now, id);
			if (line) {
				fprintf (file, "%s\n", line->Text);
				line = line->Next;
			}
			fputs ("</DL>\n", file);
			if (!line) {
				fputs (tmpl_tail, file);
			} else do {
				fprintf (file, "%s\n", line->Text);
				line = line->Next;
			} while (line);
		}
		fclose(file);
		
		while (list) {
			FLINE next = list->Next;
			free (next);
			list = next;
		}
	}
	return done;
}

/*============================================================================*/
BOOL
del_bookmark_group (const char * grp)
{
	BOOL   done = FALSE;
	FILE * file = open_bookmarks ("rb");
	if (file) {
		FLINE  list  = NULL, * pptr = &list;
		size_t len   = strlen (grp);
		int    stage = 0;
		char   buff[1024], * p;
		while ((p = get_line (file, buff, (int)sizeof(buff))) != NULL) {
			switch (stage) {
				case 0: /* nothing found yet, search for the right <DT ..> */
					if (*p == '<'
					    && strncmp (p +1, m_dt_grp, sizeof(m_dt_grp) -1) == 0) {
						char * q = p +1 + sizeof(m_dt_grp);
						while (isspace(*q)) q++;
						if (cmp_id (q, grp, len)) {
							stage = 1;
							break;
						}
					}
					goto default_;
					
				case 1: /* correct <DT ..> found, now we need the <DL .> */
					if (strncmp (p, "<DL CLASS='", 11) == 0
					    && strncmp (p +11, grp, len) == 0 && p[11 + len] == '\'') {
						stage = 2;
						break;
					}
					/* else file structure damaged */
					goto case_error;
					
				case 2: /* find the closing </DL> */
					if (strcmp (p, "</DL>") == 0) {
						stage = 3;
						done  = TRUE;
						break;
					}
					goto default_;
				
				default:
				default_:
					if (add_line (&pptr, buff)) {
						break;
					}
				case_error:
					stage = -1;
					done  = FALSE;
					fseek (file, 0, SEEK_END);
			}
		}
		fclose (file);
		
		if (done) {   /* rewrite the file */
			if ((file = open_bookmarks ("wb")) != NULL) {
				FLINE line = list;
				while (line) {
					fprintf (file, "%s\n", line->Text);
					line = line->Next;
				}
				fclose (file);
			
			} else {
				done = FALSE; /* error case */
			}
		}
		while (list) {
			FLINE next = list->Next;
			free (next);
			list = next;
		}
	}
	return done;
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
		while ((p = get_line (file, buff, (int)sizeof(buff))) != NULL) {
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


#ifdef USE_MEM_LISTS

/*============================================================================*/
B_GRP *
bkm_group_ctor (B_GRP * This, B_GRP * Next)
{
	memset (This, 0, sizeof (struct bkm_group));
	
	if (This == Next) {
		puts ("bkm_group_ctor(): This and Next are equal!");
		return This;
	}
		
	return This;
}

/*============================================================================*/
B_GRP *
bkm_group_dtor (B_GRP * This)
{
/*
	if (This->Next) {
		This->Next = NULL;
	}
	
	if (This->IdName) {
		free (This->IdName);
		This->IdName = NULL;
	}
	if (This->ClName) {
		free (This->ClName);
		This->ClName = NULL;
	}
*/
	return This;	
}

/*============================================================================*/
B_URL *
bkm_url_ctor (B_URL * This, B_URL * Next)
{
	memset (This, 0, sizeof (struct bkm_url));
	
	if (This == Next) {
		puts ("bkm_url_ctor(): This and next are equal!");
		return This;
	}
	
	return This;
}

/*============================================================================*/
B_URL *
bkm_url_dtor (B_URL * This)
{
/*
	if (This->Parent) {
		This->Parent = NULL;
	}
	
	if (This->Next) {
		This->Next = NULL;
	}

	if (This->IdName) {
		free (This->IdName);
		This->IdName = NULL;
	}
	if (This->ClName) {
		free (This->ClName);
		This->ClName = NULL;
	}
*/
	
	return This;
}

#endif /* USE_MEM_LISTS */
