/* @(#)highwire/tools/make_tab.c
 *
 * Takes a Unicode character mapping table and creates a mapping table as
 * C source for the HighWire.
 * Rainer Seitel, 2002-06-22
 */


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
	FILE *unidata_file, *file;
	long other_ch, uni_ch = 0;
	static char unidata_line[1024];
	bool line_ended = true;  /* neede for tables without control characters */

	if (argc != 2) {
		printf("highwire/tools/make_tab.tos\n"
		       "Usage: make_tab <Unicode character mapping table>\n");
		return EXIT_FAILURE;
	}

	/* The character mapping table from the Unicode database.
	 * http://www.unicode.org/Public/MAPPINGS/
	 */
	unidata_file = fopen(argv[1], "rb");
	if (unidata_file == NULL) {
		printf("make_tab: can't open mapping table!\n");
		return EXIT_FAILURE;
	}

	strcpy(argv[1] + strlen(argv[1]) - 3, "map");

	file = fopen(argv[1], "w");
	if (file == NULL) {
		printf("make_tab: can't create '%s'!", argv[1]);
		return EXIT_FAILURE;
	}

	fprintf(file,
	"/* Translation table for ??? characters to Unicode.\n"
	" */\n"
	"static const WCHAR ???_to_Unicode[] = {\n"
	"\t/*         .0     .1     .2     .3     .4     .5     .6     .7     .8     .9     .A     .B     .C     .D     .E     .F */\n"
	);

	/* Read the character mapping from the Unicode database. */
	do {

		/* Apple uses CR for line ends.  Our C library can't read these lines.
		if (!fgets(unidata_line, (int)sizeof(unidata_line), unidata_file)) {
			printf("make_tab: unexpected end of file.\n");
			break;
		}*/
		register size_t i;
		register int c;

		for (i = 0; i < sizeof(unidata_line); i++) {
			c = fgetc(unidata_file);
			if (c == EOF) {
				printf("make_tab: unexpected end of file.\n");
				return EXIT_FAILURE;
			}
			if (c == '\n') {
				c = '\0';
			} else if (c == '\r') {
				c = fgetc(unidata_file);
				if (c != EOF && c != '\n')
					ungetc(c, unidata_file);
				c = '\0';
			}
			unidata_line[i] = c;
			if (c == '\0')
				break;
		}

		if (unidata_line[0] == '\0' || unidata_line[0] == '#')
			continue;  /* ignore comments */
		if (sscanf(unidata_line, "%lx\t%lx\t", &other_ch, &uni_ch) != 2) {
			printf("make_tab: sscanf() returns != 2.\n");
			break;
		}
		if (other_ch > 256) {
			printf("make_tab: not 8-bit character.\n");
			break;
		}
		if ((other_ch % 16) == 0)
			if (line_ended)
				fprintf(file, "\t/* %lX. */ ", other_ch / 16);
			else
				fprintf(file, "\n\t/* %lX. */ ", other_ch / 16);
		if (other_ch != 255)
			fprintf(file, "0x%04lX,", uni_ch);
		else
			fprintf(file, "0x%04lX", uni_ch);  /* no comma after the last */
		if ((other_ch % 16) == 15) {
			fprintf(file, "\n");
			line_ended = true;
		} else
			line_ended = false;
	} while (other_ch < 255);

	fprintf(file, "};\n#define BEG_???_to_Unicode 0x??\n\n");

	fclose(file);

	fclose(unidata_file);

	return EXIT_SUCCESS;
}
