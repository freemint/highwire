/*
 * Config.C
 *
 */
#include <stdlib.h> /* for atol etc */
#include <string.h>
#include <ctype.h>

#include "global.h"
#include "hwWind.h"


WORD
read_config(char *fn)
{
	char l[HW_PATH_MAX], * p;
	FILE * fp = NULL;
	
	if ((p = getenv ("HOME")) != NULL && *p) {
		char * d = (p[1] == ':' ? "\\" : "/");
		p = strchr (strcpy (l, p), '\0');
		if (p[-1] != *d) *(p++) = *d;
		strcat (strcat (strcpy (p, "defaults"), d), _HIGHWIRE_CFG_);
		if ((fp = fopen (l, "r")) == NULL) {
			strcpy (p, _HIGHWIRE_CFG_);
			fp = fopen (l, "r");
			if ((fp = fopen (l, "r")) == NULL) {
				strcpy (p, ".highwire");
				fp = fopen (l, "r");
			}
		}
	}
	if (!fp && (fp = fopen(fn, "r")) == NULL) {
		return (-1);
	}

	while ((p = fgets (l, (int)sizeof(l), fp)) != NULL) {
		char  buf[22];
		short b = 0;
		long  n;
		
		while (isspace(*p)) p++;
		if (!*p || *p == '#') continue;
		
		do if (*p == '_') {
			buf[b] = *(p++);
		} else if (isalpha(*p)) {
			buf[b] = toupper(*(p++));
		} else {
			break;
		} while (++b < sizeof(buf));
		
		while (isspace(*p)) p++;
		if (!b || *(p++) != '=' || !*p) continue;
		
		n = atol (p);
		
		if (strncmp ("FONT_SIZE", buf, b) == 0) {
			if (n >= 8 && n <= 24) font_size = n;
		
		} else if (strncmp ("FONT_MINSIZE", buf, b) == 0) {
			if (n >= 1 && n <= 24) font_minsize = n;
		
		} else if (strncmp ("NORMAL", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[normal_font][0][0] = n;
		
		} else if (strncmp ("ITALIC_NORMAL", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[normal_font][0][1] = n;
		
		} else if (strncmp ("BOLD_NORMAL", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[normal_font][1][0] = n;
		
		} else if (strncmp ("BOLD_ITALIC_NORMAL", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[normal_font][1][1] = n;
		
		} else if (strncmp ("HEADER", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[header_font][0][0] = n;
		
		} else if (strncmp ("ITALIC_HEADER", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[header_font][0][1] = n;
		
		} else if (strncmp ("BOLD_HEADER", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[header_font][1][0] = n;
		
		} else if (strncmp ("BOLD_ITALIC_HEADER", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[header_font][1][1] = n;
		
		} else if (strncmp ("TELETYPE", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[pre_font][0][0] = n;
		
		} else if (strncmp ("ITALIC_TELETYPE", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[pre_font][0][1] = n;
		
		} else if (strncmp ("BOLD_TELETYPE", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[pre_font][1][0] = n;
		
		} else if (strncmp ("BOLD_ITALIC_TELETYPE", buf, b) == 0) {
			if (n >= 1 && n <= 0xFFFF) fonts[pre_font][1][1] = n;
		
		} else if (strncmp ("LOCAL_WEB", buf, b) == 0) {
			while (isspace(*p)) p++;
			if (*p) local_web = strdup (p);
		
		} else if (strncmp ("LOGGING", buf, b) == 0) {
			if (n) menu_logging();
		
		} else if (strncmp ("NO_IMAGE", buf, b) == 0) {
			if (n) menu_alt_text();
		
		} else if (strncmp ("FORCE_FRAMECTRL", buf, b) == 0) {
			if (n) menu_frm_ctrl();
		
		} else if (strncmp ("INFOBAR", buf, b) == 0) {
			hwWind_setup (HWWS_INFOBAR, n);
		}
	}
	fclose (fp);

	return (1);
}
