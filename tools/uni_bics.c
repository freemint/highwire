/* @(#)highwire/tools/uni_bics.c
 *
 * Creates a HTML file with a list of the Unicode characters useable with
 * BICS Speedo fonts by HighWire.
 * Rainer Seitel, 2002-04-09
 * 2002-05-03 added the Unicode names.
 * 2002-06-22 changed path and files.
 */


#include <stdio.h>
#include <stdlib.h>

#ifdef __PUREC__
#include <tos.h>
#endif

#ifdef LATTICE
#include <dos.h>
#include <mintbind.h>

#define DTA struct FILEINFO
#endif

#ifdef __GNUC__
#include <osbind.h>
#endif

typedef short WORD;
typedef unsigned short UWORD;
#define E_OK 0
#define EINVFN -32
#define Space_Code 561
#define numberof(arr)   (sizeof(arr) / sizeof(*arr))
						/* calculates the element number of an literally array
						 * at compile time.  doesn't work for pointers! */
#define ONLY_BICS
#include "../uni_bics.h"


int main(void)
{
	FILE *unidata_file, *file;
	long unidata_ch = 0;
	static char unidata_line[1024], unidata_name[1024];
	size_t i;
	static UWORD used_BICS[564] = {0};
	UWORD number_of_characters, ch, date = 0, year, month, day;

	{  /* read date of ../uni_bics.h as version date */
		XATTR file_info;
		long r;

		r = Fxattr(0, "..\\uni_bics.h", &file_info);
		if (r != EINVFN) {  /* Fxattr() exists */
			if (r == E_OK)
				date = file_info.mdate;
		} else {  /* here for TOS filenames */
			DTA *old, new;

			old = Fgetdta();
			Fsetdta(&new);

			if (Fsfirst("..\\uni_bics.h", 0) == E_OK)
				date = new.d_date;

			Fsetdta(old);
		}
	}
	if (date == 0) {
		printf("uni_bics: can't read file date of ../uni_bics.h!\n");
		return EXIT_FAILURE;
	}
	year = 1980 + (date >> 9);
	month = (date >> 5) & 0x0F;
	day = date & 0x1F;

	/* The character names from the Unicode database.
	 * http://www.unicode.org/Public/UNIDATA/UnicodeData.txt
	 */
	unidata_file = fopen("UnicodeData.txt", "r");
	if (unidata_file == NULL) {
		printf("uni_bics: can't open UnicodeData.txt!\n");
		return EXIT_FAILURE;
	}

	file = fopen("..\\html\\uni_bics.htm", "w");
	if (file == NULL) {
		printf("uni_bics: can't create ../html/uni_bics.htm!\n");
		return EXIT_FAILURE;
	}
	fprintf(file,
	    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
	    "<HTML LANG=\"en\">\n<HEAD>\n"
	    "<META HTTP-EQUIV=\"Content-Language\" CONTENT=\"en\">\n"
	    "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">\n"
	    "<TITLE>Unicode characters useable with Speedo fonts by HighWire</TITLE>\n"
	    "<META NAME=\"author\" CONTENT=\"Rainer Seitel\">\n"
	    "<META NAME=\"copyright\" CONTENT=\"Copyright \251 Rainer Seitel, 2002\">\n"
	    "<META NAME=\"date\" CONTENT=\"%u-%02u-%02u\">\n"
	    "<META NAME=\"description\" CONTENT=\"A list of the Unicode characters useable with BICS Speedo fonts by HighWire.\"\n"
	    "</HEAD>\n<BODY>\n\n"
	    "<H1>Unicode characters useable with Speedo fonts by HighWire</H1>\n\n"
	    "<OL START=\"192\">\n", year, month, day);

	used_BICS[Space_Code]++;
	for (i = 1; i < 95; i++) {
		used_BICS[i]++;
	}
	for (i = 0; i < numberof(windows1252_to_BICS); i++) {
		if (windows1252_to_BICS[i] < 564)
			used_BICS[windows1252_to_BICS[i]]++;
		else
			printf("uni_bics: error windows1252_to_BICS[%d] = %d = $%X >= 564\n",
			       (int)i), windows1252_to_BICS[i], windows1252_to_BICS[i];
	}

	number_of_characters = 191;

	for (i = 0; i < numberof(Unicode_to_BICS); i++) {
		number_of_characters++;
		ch = Unicode_to_BICS[i++];

		/* Read the character name from the Unicode database. */
		while (unidata_ch < ch) {
			if (!fgets(unidata_line, (int)sizeof(unidata_line), unidata_file)) {
				printf("uni_bics: unexpected end of file.\n");
				break;
			}
			if (sscanf(unidata_line, "%lx;%[^;]s", &unidata_ch, unidata_name) != 2) {
				printf("uni_bics: sscanf() returns != 2.\n");
				break;
			}
		}
		if (unidata_ch != ch || unidata_name[0] == '<')
			unidata_name[0] = '\0';

		fprintf(file, "<LI>U+%04X &amp;#%u; &#%u; %s</LI>\n", ch, ch, ch, unidata_name);
		while (Unicode_to_BICS[i]) {
			if (Unicode_to_BICS[i] < 564)
				used_BICS[Unicode_to_BICS[i]]++;
			else
				printf("uni_bics: error Unicode_to_BICS[%d] = %d = $%X >= 564\n",
				       (int)i), Unicode_to_BICS[i], Unicode_to_BICS[i];
			i++;
		}
	}

	fprintf(file, "</OL>\n\n"
	              "<P>%u Unicode characters useable with BICS Speedo fonts.</P>\n\n"
	              "<UL>\n", number_of_characters);

	number_of_characters = 0;

	for (i = 0; i < 564; i++)
		if (used_BICS[i]) {
			fprintf(file, "<LI>BICS %d %u times used</LI>\n", (int)i, used_BICS[i]);
			number_of_characters++;
		} else
			fprintf(file, "<LI><STRONG>BICS %d %u times used</STRONG></LI>\n",
			        (int)i, used_BICS[i]);

	fprintf(file,
	    "</UL>\n\n"
	    "<P>%d glyphs of 564 from the Bitstream International Character Set used.</P>\n\n"
	    "<P>Created by uni_bics, %u-%02u-%02u.</P>\n\n"
	    "</BODY>\n</HTML>\n", number_of_characters, year, month, day);
	fclose(file);

	fclose(unidata_file);

	return EXIT_SUCCESS;
}
