/*
 ** 
 **
 ** Changes
 ** Author         Date           Desription
 ** P Slegg        29-May-2010    Moved from hwWind.c
 **
 */
 
 /*----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <cflib.h>

#include "global.h"
#include "clipbrd.h"

FILE *
open_scrap (BOOL rdNwr)
{
	char path[HW_PATH_MAX] = "";
	const char * mode = (rdNwr ? "rb" : "wb");
	FILE * file;
	
	if (scrp_read (path) <= 0 || !path[0])
	{
		file = NULL;
	}
	else
	{
		char * scrp = strchr (path, '\0');
		if (scrp[-1] != '\\') *(scrp++) = '\\';
		if (path[0]  >= 'a')  path[0]  -= ('a' - 'A');

		strcpy (scrp, "scrap.txt");
		if ((file = fopen (path, mode)) == NULL && rdNwr)
		{
			strcpy (scrp, "SCRAP.TXT");
			file = fopen (path, mode);
		}
	}
	return file;
}

/*============================================================================*/

char *
read_scrap(void)
{
    FILE * file = open_scrap (TRUE);
		if (file)
		{
			long filelength;
			char * paste;

			/* find the size of the file */
			fseek (file, 0, SEEK_END);
			filelength = ftell (file);
			fseek (file, 0, SEEK_SET);

			/* allocate enough memory to hold the file */
			paste = malloc (filelength+1);

			if (paste != NULL)
			{
				/* read the entire file */
				long n = fread (paste, 1, filelength, file);
				paste[n] = '\0';
			}

			fclose (file);
			return paste;
		}
		else
			return NULL;
}
