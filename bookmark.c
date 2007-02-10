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

int Num_Bookmarks;

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

/*============================================================================*/
BOOL
read_bookmarks (void) {
	FILE * file = NULL;
	long   now = time (NULL);
	Num_Bookmarks = 0;

	/* Bookmarks exists, read or parse or load? */
	if ((file = open_bookmarks ("r")) != NULL) { 
		char        buff[1024];

		while (fgets (buff, (int)sizeof(buff), file)) {
			if (strnicmp(buff, "<DT><A",6) == 0 ) {
				Num_Bookmarks += 1;
			} else if (strnicmp(buff, "</HTML>", 7) == 0 ) {
				break;
			}
		}
		
		fclose(file);
	} else if ((file = open_bookmarks ("w")) != NULL) {
		fputs ("<html>\n", file);
		fputs("<!DOCTYPE HighWire-Bookmark-file-1>\n", file);
		fputs("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=Atari\">\n",file);
		fputs("<TITLE>Bookmarks</TITLE>\n",file);
		fputs("<link rel=\"stylesheet\" href=\"bookmark.css\" type=\"text/css\" media=\"screen\">\n",file);
		fprintf (file, "<H1 LAST_MODIFIED=\"%ld\">Bookmarks</H1>\n",now);
		fputs ("<HR>\n", file);
		fputs ("<DL>\n", file);
		fprintf (file, "<DT>&#9658; <a href=\"bookmark.htm\" ADD_DATE=\"%ld\" ID=\"INTPRJ\">HighWire Project</a>\n", now);
		fputs ("<DL CLASS=\"INTPRJ\">\n", file);
		fprintf (file, "<DT><A ID=\"00000001\" CLASS=\"INTPRJ\" target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://highwire.atari-users.net\">HighWire Homepage</a>\n", now, 0L);
		fprintf (file, "<DT><A ID=\"00000002\" CLASS=\"INTPRJ\" target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://www.atariforums.com/index.php?f=20\">HighWire Forum</a>\n", now, 0L);
		fprintf (file, "<DT><A ID=\"00000003\" CLASS=\"INTPRJ\" target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://www.atari-users.net/mailman/listinfo/highwire\">Developers Mailing lists</a>\n", now, 0L);
		fprintf (file, "<DT><A ID=\"00000004\" CLASS=\"INTPRJ\" target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://www.atari-users.net/mailman/listinfo/highwire-users\">Users Mailing lists</a>\n", now, 0L);
		fprintf (file, "<DT><A ID=\"00000005\" CLASS=\"INTPRJ\" target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://highwire.atari-users.net/mantis/\">Bugtracker</a>\n", now, 0L);
		fputs ("</DL>\n", file);
		fputs ("<HR>\n", file);
		fputs ("</DL>\n", file);
		fputs ("</html>", file);
		fclose(file);

		Num_Bookmarks = 5;

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

	if ((file = open_bookmarks ("r+")) != NULL) {
		fseek (file, 0, SEEK_END);
		flen = ftell(file);
		fseek (file, (flen - 16), SEEK_SET);
		fprintf (file, "<DT>&#9658;<A href=\"bookmark.htm\" ADD_DATE=\"%ld\" ID=\"USR_%s\">%s</A>\n", now,group,group);
		fprintf (file, "<DL  CLASS=\"USR_%s\">\n", group);

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
	FILE *  file = NULL;
	long    now = time (NULL);
	long	flen;
	char    buf[10];

	/* generate an ID for the URL */
	Num_Bookmarks += 1;

	sprintf(buf,"%.*d",8,Num_Bookmarks);

	if ((file = open_bookmarks ("r+")) != NULL) {
		fseek (file, 0, SEEK_END);
		flen = ftell(file);
/*
I've modified this all, but maybe we need something like the following?

#if defined (__PUREC__)
		fseek (file, (flen - 16), SEEK_SET);
#else
		fseek (file, (flen - 14), SEEK_SET);
#endif
*/
		fseek (file, (flen - 14), SEEK_SET);

		fprintf (file, "<DT><A ID=\"%s\" CLASS=\"USR_ROOT\" target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"%s\">%s</a>\n", buf, now, now, bookmark_url, bookmark_title);

		fputs ("</DL>\n", file);
		fputs ("</html>", file);
		fclose(file);

		return TRUE;
	} else {
		printf("Bookmark file lost?!? \n");
		return FALSE;
	}
}


