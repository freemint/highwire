/*
 * Config.C
 *
 */
#include <stdlib.h> /* for atol etc */
#include <string.h>
#include <ctype.h>

#include "global.h"
#include "hwWind.h"
#include "cache.h"

#define HTMLTAG int
#define HTMLKEY int
#include "scanner.h"


/*----------------------------------------------------------------------------*/
#define FA(f,b,i)   (((long)f <<16) | (b ? 0x10 : 0x00) | (i ? 0x01 : 0x00))
static void
cfg_font (char * param, long arg)
{
	long n = atol (param);
	if (n >= 1 && n <= 0xFFFF) {
		fonts[(short)(arg >>16)][arg & 0x10 ? 1 : 0][arg & 0x01] = n;
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_fntsize (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n >= 8 && n <= 24) {
		font_size = n;
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_minsize (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n >= 1 && n <= 24) {
		font_minsize = n;
	}
}


/*----------------------------------------------------------------------------*/
static void
cfg_backgnd (char * param, long arg)
{
	(void)arg;
	if (!ignore_colours) {
	
		if (isalpha(*param)) { /* named colour */
			long n = scan_color (param, strlen(param));
			if (n >= 0) {
				background_colour = remap_color (n);
			}
		
		} else if (param[0] != '0' || !param[1]) { /* VDI colour index */
			char * tail = param;
			long   n    = strtoul (param, &tail, 10);
			if (tail > param && n <= 0xFF) {
				background_colour = n;
			}
		
		} else if (param[1] == 'x') { /* 0xRRGGBB direct colour value */
			char * tail = param;
			long   n    = strtoul (param, &tail, 16);
			if (tail > param && n <= 0xFFFFFFL) {
				background_colour = remap_color (n);
			}
		}
	}
}


/*----------------------------------------------------------------------------*/
static void
cfg_infobar (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	hwWind_setup (HWWS_INFOBAR, n);
}


/*----------------------------------------------------------------------------*/
static void
cfg_timeout_connect (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	conn_timeout= n;
}

/*----------------------------------------------------------------------------*/
static void
cfg_timeout_hdr (char * param, long arg)
{
	long n = strtoul (param, &param, 10);
	long m = strtoul (param, &param, 10);
	(void)arg;
	if (n > 0 || m > 0) {
		hdr_tout_gfx=1000*m;
		hdr_tout_doc=1000*n;
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_retry (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	conn_retry = n;
}

/*----------------------------------------------------------------------------*/
static void
cfg_localweb (char * param, long arg)
{
	(void)arg;
	local_web = strdup (param);
}


/*----------------------------------------------------------------------------*/
static void
cfg_func (char * param, long arg)
{
	long n = atol (param);
	if (n) {
		typedef void (*gen_func)(void);
		(*(gen_func)arg)();
	}
}


/*----------------------------------------------------------------------------*/
static void
cfg_cachedir (char * param, long arg)
{
	(void)arg;
	cache_setup (param, 0, 0, 0);
}

/*----------------------------------------------------------------------------*/
static void
cfg_cachemem (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n > 0) {
		cache_setup (NULL, (ULONG)n * 1024, 0, 0);
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_cachedsk (char * param, long arg)
{
	long n = strtoul (param, &param, 10);
	long m = strtoul (param, &param, 10);
	(void)arg;
	if (n > 0 || m > 0) {
		cache_setup (NULL, 0, (ULONG)n * 1024, m);
	}
}


/******************************************************************************/
WORD
read_config(char *fn)
{
	char l[HW_PATH_MAX], * p, * d;
	FILE * fp = NULL;
	
	if ((p = getenv ("HOME")) != NULL && *p) {
		d = (p[1] == ':' ? "\\" : "/");
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
		
		if ((d = strchr (p, '#')) == NULL) {
			d = strchr (p, '\0');
		}
		while (d > p && isspace(d[-1])) {
			d--;
		}
		*d = '\0';
		if (!*p) continue;
		
		while (isspace(*p)) p++;
		do if (*p == '_') {
			buf[b] = *(p++);
		} else if (isalpha(*p)) {
			buf[b] = toupper(*(p++));
		} else {
			break;
		} while (++b < sizeof(buf) -1);
		buf[b] = '\0';
		
		while (isspace(*p)) p++;
		if (buf[0] && *(p++) == '=' && *p) {
			struct {
				const char * key;
				void       (*func)(char*, long);
				long         arg;
			} cfg[] = {
				{ "BOLD_HEADER",          cfg_font,    FA(header_font, 1, 0) },
				{ "BOLD_ITALIC_HEADER",   cfg_font,    FA(header_font, 1, 1) },
				{ "BOLD_ITALIC_NORMAL",   cfg_font,    FA(normal_font, 1, 1) },
				{ "BOLD_ITALIC_TELETYPE", cfg_font,    FA(pre_font,    1, 1) },
				{ "BOLD_NORMAL",          cfg_font,    FA(normal_font, 1, 0) },
				{ "BOLD_TELETYPE",        cfg_font,    FA(pre_font,    1, 0) },
				{ "CACHEDIR",             cfg_cachedir,0 },
				{ "CACHEDSK",             cfg_cachedsk,0 },
				{ "CACHEMEM",             cfg_cachemem,0 },
				{ "DFLT_BACKGND",         cfg_backgnd, 0 },
				{ "FONT_MINSIZE",         cfg_minsize, 0 },
				{ "FONT_SIZE",            cfg_fntsize, 0 },
				{ "FORCE_FRAMECTRL",      cfg_func,    (long)menu_frm_ctrl   },
				{ "HEADER",               cfg_font,    FA(header_font, 0, 0) },
				{ "INFOBAR",              cfg_infobar, 0 },
				{ "ITALIC_HEADER",        cfg_font,    FA(header_font, 0, 1) },
				{ "ITALIC_NORMAL",        cfg_font,    FA(normal_font, 0, 1) },
				{ "ITALIC_TELETYPE",      cfg_font,    FA(pre_font,    0, 1) },
				{ "LOCAL_WEB",            cfg_localweb,0 },
				{ "LOGGING",              cfg_func,    (long)menu_logging    },
				{ "NORMAL",               cfg_font,    FA(normal_font, 0, 0) },
				{ "NO_IMAGE",             cfg_func,    (long)menu_alt_text   },
				{ "RETRY_HEADER",	cfg_retry, 0 },
				{ "TELETYPE",             cfg_font,    FA(pre_font,    0, 0) },
				{ "TIMEOUT_CONNECT",            cfg_timeout_connect, 0 },
				{ "TIMEOUT_HEADER",            cfg_timeout_hdr, 0 }
			};
			short beg = 0;
			short end = (short)numberof(cfg) - 1;
			do {
				short i = (end + beg) / 2;
				int   k = strcmp (buf, cfg[i].key);
				if (k > 0) {
					beg = i + 1;
				} else if (k) {
					end = i - 1;
				} else {
					while (isspace(*p)) p++;
					(*cfg[i].func) (p, cfg[i].arg);
					break;
				}
			} while (beg <= end);
		}
	}
	fclose (fp);

	return (1);
}
