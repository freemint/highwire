/*
 * Bookmark.C
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "file_sys.h"
#include "defs.h"
#include "bookmark.h"

const char * bkm_File = NULL;
const char * bkm_CSS  = NULL;

static int Num_Bookmarks = 0;


static const char tmpl_head[] =
	"<!DOCTYPE HighWire-Bookmark-file-1>\n"
	"<html><head>\n"
	"  <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;"
	         " charset=atarist\">\n"
	"  <TITLE>Bookmarks</TITLE>\n"
	"  <link rel=\"stylesheet\" href=\"bookmark.css\" type=\"text/css\""
	         "  media=\"screen\">\n"
	"</head><body>\n";
static const char tmpl_tail[] =
	 "</DL>\n"
	 "</body></html>\n";
static const char m_dt_grp[] = "DT CLASS='GRP'";
static const char m_dt_lnk[] = "DT CLASS='LNK'";


/****************************
	I'm implementing a file only method of storing the hotlist
however, I am also trying to spec out an internal storage of the hostlist
in memory.  So if some things don't make sense at the moment, like
the LAST_VISIT= timestamp, it's because I'd like to fix it in the future
******************************/

/*  ID & CLASS distictions
 *  Url's have ID's and CLASS's
 *  The CLASS is the ID of the Group it is a member of
 */

/*----------------------------------------------------------------------------*/
static FILE *
open_bookmarkCss (const char * mode)
{
	FILE * file = NULL;
	
	if (bkm_CSS) {
		file = fopen (bkm_CSS, mode);
		
	} else {
		char buff[1024], * p;
		if ((p = getenv ("HOME")) != NULL && *p) {
			char slash = (p[1] == ':' ? '\\' : '/');
			p = strchr (strcpy (buff, p), '\0');
			if (p[-1] != slash) *(p++) = slash;
			strcpy (p, "defaults");
			p[8] = slash;
			strcpy (p +9, _HIGHWIRE_BKM_CSS_);
			file = fopen (buff, mode);
			
			if (!file) {
				strcpy (p, _HIGHWIRE_BKM_CSS_);
				file = fopen (buff, mode);
			}
		}
		if (!file) {
			buff[0] = Dgetdrv();
			buff[0] += (buff[0] < 26 ? 'A' : -26 + '1');
			buff[1] = ':';
			Dgetpath (buff + 2, 0);
			p = strchr (buff, '\0');
			if (p[-1] != '\\') *(p++) = '\\';
			strcpy (p, _HIGHWIRE_BKM_CSS_);
			file = fopen (buff, mode);
		}
		if (file) {
			bkm_CSS = strdup (buff);
		}
	}
	
	return file;
}
 
 
/*----------------------------------------------------------------------------*/
static FILE *
open_bookmarks (const char * mode)
{
	FILE * file = NULL;
	
	if (bkm_File) {
		file = fopen (bkm_File, mode);
		
	} else {
		char buff[1024], * p;
		if ((p = getenv ("HOME")) != NULL && *p) {
			char slash = (p[1] == ':' ? '\\' : '/');
			p = strchr (strcpy (buff, p), '\0');
			if (p[-1] != slash) *(p++) = slash;
			strcpy (p, "defaults");
			p[8] = slash;
			strcpy (p +9, _HIGHWIRE_BOOKMARK_);
			file = fopen (buff, mode);
			
			if (!file) {
				strcpy (p, _HIGHWIRE_BOOKMARK_);
				file = fopen (buff, mode);
			}
		}
		if (!file) {
			buff[0] = Dgetdrv();
			buff[0] += (buff[0] < 26 ? 'A' : -26 + '1');
			buff[1] = ':';
			Dgetpath (buff + 2, 0);
			p = strchr (buff, '\0');
			if (p[-1] != '\\') *(p++) = '\\';
			strcpy (p, _HIGHWIRE_BOOKMARK_);
			file = fopen (buff, mode);
		}
		if (file) {
			bkm_File = strdup (buff);
		}
	}
	
	return file;
}

/*----------------------------------------------------------------------------*/
static void
wr_grp (FILE * file, const char * id_class,
        long add, const char * url, const char * title)
{
	fprintf (file, "<%s>&#9658; <A ID=\"%s\" ADD_DATE=\"%ld\""
	               " HREF=\"%s\">%s</a>\n", m_dt_grp, id_class, add, url, title);
	fprintf (file, "<DL CLASS=\"%s\">\n",   id_class);
}

/*----------------------------------------------------------------------------*/
static void
wr_lnk (FILE * file, int * id, const char * class,
        long add, long visit, const char * url, const char * title)
{
	fprintf (file, "<%s><A ID=\"%.*d\" CLASS=\"%s\" TARGET=\"_hw_top\""
	               " ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" HREF=\"%s\">%s</a>\n",
	               m_dt_lnk, 8, ++(*id), class, add, visit, url, title);
}

/*============================================================================*/
BOOL
read_bookmarks (void) {
	FILE * file = NULL;
	
	Num_Bookmarks = 0;

	/* Bookmarks exists, read or parse or load? */
	if ((file = open_bookmarks ("r")) != NULL) { 
		char        buff[1024];

		while (fgets (buff, (int)sizeof(buff), file)) {
			if (*buff == '<'
			    && strnicmp (buff +1, m_dt_lnk, sizeof(m_dt_lnk) -1) == 0 ) {
				Num_Bookmarks += 1;
			} else if (strnicmp(buff, "</HTML>", 7) == 0 ) {
				break;
			}
		}
		fclose(file);
		
	} else if ((file = open_bookmarks ("wb")) != NULL) {
		long now = time (NULL);
		fputs (tmpl_head, file);
		fprintf (file, "<H1 LAST_MODIFIED=\"%ld\">Bookmarks</H1>\n",now);
		fputs ("<HR>\n", file);
		fputs ("<DL>\n", file);
		wr_grp (file, "INTPRJ", now, "bookmark.htm", "HighWire Project");
		wr_lnk (file, &Num_Bookmarks, "INTPRJ", now, 0L,
		        "http://highwire.atari-users.net", "HighWire Homepage");
		wr_lnk (file, &Num_Bookmarks, "INTPRJ", now, 0L,
		        "http://www.atariforums.com/index.php?f=20", "HighWire Forum");
		wr_lnk (file, &Num_Bookmarks, "INTPRJ", now, 0L,
		        "http://www.atari-users.net/mailman/listinfo/highwire",
		        "Developers Mailing lists");
		wr_lnk (file, &Num_Bookmarks, "INTPRJ", now, 0L,
		        "http://www.atari-users.net/mailman/listinfo/highwire-users",
		        "Users Mailing lists");
		wr_lnk (file, &Num_Bookmarks, "INTPRJ", now, 0L,
		        "http://highwire.atari-users.net/mantis/", "Bugtracker");
		fputs ("</DL>\n", file);
		fputs ("<HR>\n", file);
		fputs (tmpl_tail, file);
		fclose(file);
		
		if ((file = open_bookmarkCss ("r")) != NULL) {
			/* Bookmarks CSS exists, read or parse or load? */
			fclose(file);
		
		} else if ((file = open_bookmarkCss ("w")) != NULL) {
			/* Not exciting but want something in file */
			fputs ("A { text-decoration: none; }\n", file);
			/* The idea is that we can put a display none in to hide DL sets */
			/*fputs (".INTPRJ { display: inline; }\n", file); */
			fclose(file);
		}
	} 
	
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

/*============================================================================*/
/* 
 * THIS IS JUNK!  This really needs to be rewritten
 * This just over writes the end of the file
 */

BOOL
add_bookmark_group (const char * group)
{
	FILE *  file = NULL;
	long    now = time (NULL);
	long	flen;

	if ((file = open_bookmarks ("rb+")) != NULL) {
		char id_class[45];
		sprintf (id_class, "USR_%s", group);
		
		fseek (file, 0, SEEK_END);
		flen = ftell(file);
		fseek (file, (flen - 16), SEEK_SET);
		wr_grp (file, id_class, now, "bookmark.htm", group);

		fputs ("</DL>\n", file);
		fputs ("<HR>\n", file);

		fputs ("</DL>\n", file);
		fputs ("</html>\n", file);
		fclose(file);
		return TRUE;
	} else {
		printf("Bookmark file lost?!? \n");
		return FALSE;
	}
}

/* we need a method to add a bookmark to a group and delete a bookmark
 * from a group
 */


/*============================================================================*/
BOOL
add_bookmark (const char * bookmark_url, const char *bookmark_title)
{
	FILE * file = open_bookmarks ("rb+");
	if (file) {
		long now = time (NULL);
		fseek (file, -(sizeof(tmpl_tail) -1), SEEK_END);
		wr_lnk (file, &Num_Bookmarks, "USR_ROOT",
		              now, now, bookmark_url, bookmark_title);
		fputs (tmpl_tail, file);
		fclose(file);
		
		return TRUE;
		
	} else {
		printf("Bookmark file lost?!? \n");
		return FALSE;
	}
}


