/*
 * Bookmark.C
 *
 */
#include <stdlib.h> /* for atol etc */
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "version.h"
#include "file_sys.h"
#include "global.h"
#include "Location.h"
#include "mime.h"
#include "http.h"
#include "hwWind.h"
#include "fontbase.h"
#include "cache.h"

#include "scanner.h"

const char * bkm_File         = NULL;

/****************************
	I'm implementing a file only method of storing the hotlist
however, I am also trying to spec out an internal storage of the hostlist
in memory.  So if some things don't make sense at the moment, like
the LAST_VISIT= timestamp, it's because I'd like to fix it in the future
******************************/


/*----------------------------------------------------------------------------*/
FILE *
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

	if ((file = open_bookmarks ("r")) != NULL) {
		/* Bookmarks exists, read or parse or load? */

		fclose(file);
	} else if ((file = open_bookmarks ("w")) != NULL) {
		fputs ("<html>\n", file);
		fputs("<!DOCTYPE HighWire-Bookmark-file-1>\n", file);
		fputs("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=Atari\">\n",file);
		fputs("<TITLE>Bookmarks</TITLE>\n",file);
		fputs("<style>\n", file);
		fputs("dt { white-space:nowrap }\n", file);
		fputs("dt.open { background-color:#1E90FF }\n", file); 
		fputs("</style>\n",file);
		fprintf (file, "<H1 LAST_MODIFIED=\"%ld\">Bookmarks</H1>\n",now);
		fputs ("<HR>\n", file);
		fputs ("<DL>\n", file);
		fprintf (file, "<DT>&#9658; <a href=\"bookmark.htm\" ADD_DATE=\"%ld\" ID=\"INTPRJ\">HighWire Project</a>\n", now);
		fputs ("<DL>\n", file);
		fprintf (file, "<DT><OBJECT DATA=\"data:\"></OBJECT><a target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://highwire.atari-users.net\">HighWire Homepage</a>\n", now, 0L);
		fprintf (file, "<DT><OBJECT DATA=\"data:\"></OBJECT><a target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://www.atariforums.com/index.php?f=20\">HighWire Forum</a>\n", now, 0L);
		fprintf (file, "<DT><OBJECT DATA=\"data:\"></OBJECT><a target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://www.atari-users.net/mailman/listinfo/highwire\">Developers Mailing lists</a>\n", now, 0L);
		fprintf (file, "<DT><OBJECT DATA=\"data:\"></OBJECT><a target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://www.atari-users.net/mailman/listinfo/highwire-users\">Users Mailing lists</a>\n", now, 0L);
		fprintf (file, "<DT><OBJECT DATA=\"data:\"></OBJECT><a target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"http://highwire.atari-users.net/mantis/\">Bugtracker</a>\n", now, 0L);
		fputs ("</DL>\n", file);
		fputs ("<HR>\n", file);
		fputs ("</DL>\n", file);
		fputs ("</html>\n", file);
		fclose(file);
	} 

	return TRUE;
}

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
		fseek (file, (flen - 14), SEEK_SET);
		fprintf (file, "<DT>&#9658;<A href=\"bookmark.htm\" ADD_DATE=\"%ld\" ID=\"USR_%s\">%s</A>\n", now,group,group);
		fputs ("<DL>\n", file);

		fputs ("</DL>\n", file);
		fputs ("<HR>\n", file);

		fputs ("</DL>\n", file);
		fputs ("</html>\n", file);
		fclose(file);
		return TRUE;
	} else {
		printf("Bookmark file lost?!? \r\n");
		return FALSE;
	}
}

/*============================================================================*/
BOOL
add_bookmark (const char * bookmark_url, const char *bookmark_title)
{
	FILE *  file = NULL;
	long    now = time (NULL);
	long	flen;

	if ((file = open_bookmarks ("r+")) != NULL) {
		fseek (file, 0, SEEK_END);
		flen = ftell(file);
		fseek (file, (flen - 14), SEEK_SET);

		fprintf (file, "<DT><OBJECT DATA=\"data:\"></OBJECT><a target=\"_blank\" ADD_DATE=\"%ld\" LAST_VISIT=\"%ld\" href=\"%s\">%s</a>\n", now, now, bookmark_url, bookmark_title);

		fputs ("</DL>\n", file);
		fputs ("</html>\n", file);
		fclose(file);

		return TRUE;
	} else {
		printf("Bookmark file lost?!? \r\n");
		return FALSE;
	}
}


