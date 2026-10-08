/*
 * Config.C
 *
 */
#include <stdlib.h> /* for atol etc */
#include <string.h>
#include <ctype.h>

#include "version.h"
#include "file_sys.h"
#include "global.h"
#include "Logging.h"
#include "Location.h"
#include "mime.h"
#include "http.h"
#include "hwWind.h"
#include "fontbase.h"
#include "cache.h"

#include "scanner.h"
#include "mbedTLS.h"


WORD         cfg_UptoDate     = -1;
const char * cfg_File         = NULL;
const char * cfg_StartPage    = "html\\index.htm";
const char * cfg_Viewer       = NULL;
BOOL         cfg_AllowCookies = FALSE;
BOOL         cfg_DropImages   = FALSE;
BOOL         cfg_ViewImages   = TRUE;
BOOL         cfg_FixedCmap    = FALSE;
BOOL         cfg_FastImages   = FALSE;
BOOL         cfg_GreyPalette  = TRUE;
BOOL         cfg_ImgAspect    = TRUE;
WORD         cfg_LineSpacing  = -1;
BOOL         cfg_UseCSS       = TRUE;
BOOL         cfg_HotlistOpen  = FALSE;
BOOL         cfg_AVWindow     = FALSE;
BOOL         cfg_GlobalCycle  = FALSE;
WORD         cfg_ConnTout     = 1;
WORD         cfg_ConnRetry    = 3;
ULONG        cfg_MaxDocument  = 128uL * 1024uL;
UWORD        cfg_MaxImages    = 0;
UWORD        cfg_SecProtMin   = SECURE_PROTOCOL_TLSv1_2;
UWORD        cfg_SecProtMax   = SECURE_PROTOCOL_TLSv1_3;
UWORD        cfg_SrvCertVer   = 0;

static const char * cfg_magic = _HIGHWIRE_VERSION_ " [" __DATE__ "]";


/*----------------------------------------------------------------------------*/
/* Fill buf with 'X:\launch\path\' and return the append point for a name. */
static char *
prog_dir (char * buf)
{
	char * p;
	buf[0] = Dgetdrv();
	buf[0] += (buf[0] < 26 ? 'A' : -26 + '1');
	buf[1] = ':';
	Dgetpath (buf + 2, 0);
	p = strchr (buf, '\0');
	if (p[-1] != '\\') *(p++) = '\\';
	*p = '\0';
	return p;
}

/*============================================================================*/
FILE *
open_default (const char ** path, const char * name, const char * mode)
{
	FILE * file = NULL;
	
	if (*path) {
		file = fopen (*path, mode);
		
	} else {
		char buff[1024], * p;
		if ((p = getenv ("HOME")) != NULL && *p) {
			if (p[1] == ':') {
				buff[0] = toupper (p[0]);
				p = strchr (strcpy (buff +1, p +1), '\0');
			} else {
				char * q = buff;
				*(q++) = 'U';
				*(q++) = ':';
				while (*p) {
					if (*p == '/') *(q++) = '\\';
					else           *(q++) = *p;
					p++;
				}
				p = q;
			}
			if (p[-1] != '\\') *(p++) = '\\';
			strcpy (p, "defaults");
			p[8] = '\\';
			strcpy (p +9, name);
			file = fopen (buff, mode);
			
			if (!file) {
				strcpy (p, name);
				file = fopen (buff, mode);
			}
		}
		if (!file) {
			strcpy (prog_dir (buff), name);
			file = fopen (buff, mode);
		}
		if (file) {
			*path = strdup (buff);
		}
	}
	
	return file;
}

/*----------------------------------------------------------------------------*/
#define open_cfg(mode)   open_default (&cfg_File, _HIGHWIRE_CFG_, mode)

/*----------------------------------------------------------------------------*/
/* If no CACHEDIR was configured (or it failed), default to a folder 'CACHE'
 * in the program's directory, created if it doesn't exist yet.
 */
static void
dflt_cachedir (void)
{
	if (!cache_DirInfo()) {
		char buf[HW_PATH_MAX];
		strcpy (prog_dir (buf), "CACHE");
		(void)Dcreate (buf); /* may exist already, cache_build() sorts it out */
		cache_setup (buf, 0, 0, 0);
	}
}


/*----------------------------------------------------------------------------*/
/* A fixed document cap suits exactly one size of machine.  Take it from the
 * memory actually there instead: measured, a 4MB ST parses 128k of markup and
 * runs out on 192k, and Malloc() reports about 3MB free at this point on that
 * machine -- so a twenty-fourth of what is free is what the DOM built from it
 * can stand.  Checked against 2, 4, 8 and 14MB: the 4MB case lands on 129k,
 * which is the size measured to work there, and none of them fall over.
 *
 * The ratio is what matters rather than the machine: it leaves a floppy-only
 * STE something it can survive and gives a TT or Falcon one worth having.
 *
 * Capped at 512k because past that the limit stops being memory and becomes
 * time -- parsing is worse than linear -- and nobody should meet a five minute
 * page by default.  MAX_DOCUMENT is read afterwards and overrides all of this,
 * including back to 0 for no cap at all.
*/
static void
dflt_maxdocument (void)
{
	long avail;

	/* A TT or Falcon keeps most of its memory in TT ram, so ask for that
	 * where the system knows what it is -- the same test main() uses. */
	if (Sysconf (-1) != -EINVFN) {
		avail = (long)Mxalloc (-1L, 3 /* prefer TT ram */);
	} else {
		avail = (long)Malloc (-1L);
	}
	if (avail > 0) {
		avail /= 24;
		if      (avail <   8L *1024) avail =   8L *1024;
		else if (avail > 512L *1024) avail = 512L *1024;
		cfg_MaxDocument = (ULONG)avail;
	}
}


/*============================================================================*/
BOOL
save_config (const char * key, const char * arg)
{
	struct cfg_line {
		struct cfg_line * Next;
		char              Text[1];
	}    * list = NULL, * match = NULL, * nmtch = NULL;
	size_t klen = (key ? strlen (key) : 0);
	BOOL   ok   = TRUE;
	BOOL  fresh = (cfg_File == NULL);
	FILE * file;
	
	if ((file = open_cfg ("r")) != NULL) {
		struct cfg_line ** pptr = &list;
		char buff[1024], * beg, * end;
		while ((beg = fgets (buff, (int)sizeof(buff), file)) != NULL) {
			struct cfg_line * line;
			while (isspace (*beg)) beg++;
			if (strnicmp (beg, "HighWire", 8) == 0) {
				continue;
			}
			end = strchr (beg, '\0');
			while (end > beg && isspace (end[-1])) {
				*(--end) = '\0';
			}
			if ((line = malloc (sizeof(struct cfg_line) + (end - beg))) == NULL) {
				ok = FALSE;
				break;
			}
			memcpy (line->Text, beg, end - beg +1);
			line->Next = NULL;
			
			if (klen) {
				if (*beg != '#') {
					if (strnicmp (beg, key, klen) == 0) {
						if (!match) {
							match = line;
						} else { /* duplicate line, delete */
							free (line);
							line = NULL;
						}
					}
				} else if (!nmtch) {
					while (isspace (*(++beg)));
					if (strnicmp (beg, key, klen) == 0) {
						nmtch = line;
					}
				}
			}
			if (line) {
				*pptr = line;
				pptr  = &line->Next;
			}
		}
		fclose (file);
		
		if (match) nmtch = NULL;
		else       match = nmtch;
	}
	
	if (ok && (file = open_cfg ("w")) != NULL) {
		struct cfg_line * line = list;
		fprintf (file, "HighWire = %s\n", cfg_magic);
		while (line) {
			if (line == match) {
				char * p = strchr (line->Text +1, '#');
				size_t spc;
				if (!p) {
					p   = "";
					spc = 0;
				} else {
					spc = klen + 3 + strlen (arg);
					if (spc >= p - line->Text) {
						spc = 0;
					} else {
						spc = (p - line->Text) - spc + strlen (p);
					}
				}
				fprintf (file, "%s = %s%*s\n", key, arg, (int)spc, p);
				
			} else {
				fputs (line->Text, file);
				fputs ("\n", file);
			}
			line = line->Next;
		}
		if (!match && klen) { /* nothing found to replace, so append */
			fprintf (file, "%s = %s\n", key, arg);
		}
		fclose (file);
		
		if (fresh) {
			hwUi_info (NULL, "New config file created at\n%.100s", cfg_File);
		}
	}
	
	while (list) {
		struct cfg_line * line = list;
		list = line->Next;
		free (line);
	}
	return ok;
}


/*******************************************************************************
 *
 * callback functions for config file parsing
*/

/*------------------------------------------------------------------------------
 * generic function:  'arg' points to a BOOL varaiable which will be set to
 * TRUE or FALSE according to the numeric value in 'param'.
*/
static void
cfg_BOOL (char * param, long arg)
{
	*((BOOL*)arg) = (atol (param) > 0);
}

/*------------------------------------------------------------------------------
 * generic function:  'arg' is an external function of the type  void f (int)
 * will be called with the argument TRUE or FALSE according to the numeric value
 * in 'param'.
*/
static void
cfg_Func (char * param, long arg)
{
	long n = atol (param);
	typedef void (*gen_func)(int);
	(*(gen_func)arg)(n > 0);
}


/*----------------------------------------------------------------------------*/
static void
cfg_up2date (char * param, long arg)
{
	(void)arg;
	if (cfg_UptoDate < 0) {
		cfg_UptoDate = (strcmp (param, cfg_magic) == 0);
	}
}


/*----------------------------------------------------------------------------*/
static void
cfg_startpage (char * param, long arg)
{
	(void)arg;
	if (*param == '!') { /* for debugging only */
		param++;
		cfg_UptoDate = TRUE;
	}
	if (cfg_UptoDate > 0) {
		cfg_StartPage = strdup (param);
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_bookm_geo (char * param, long arg)
{
	(void)arg;
	hwWind_setup (HWWS_BOOKMGEO, (long)param);
}

/*----------------------------------------------------------------------------*/
static void
cfg_brwsr_geo (char * param, long arg)
{
	(void)arg;
	hwWind_setup (HWWS_GEOMETRY, (long)param);
}


/*----------------------------------------------------------------------------*/
static void
cfg_viewer (char * param, long arg)
{
	(void)arg;
	cfg_Viewer = strdup (param);
}


/*----------------------------------------------------------------------------*/
#define FA(f,b,i)   (((long)f <<16) | (b ? 0x10 : 0x00) | (i ? 0x01 : 0x00))
static void
cfg_font (char * param, long arg)
{
	long id = atol (param);
	if (id >= 1 && id <= 0xFFFF) {
		font_setup ((UWORD)id, (WORD)(arg >>16), (arg & 0x10) != 0, (arg & 0x01) != 0);
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_fntsize (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n >= 8 && n <= 24) {
		font_size = (WORD)n;
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_minsize (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n >= 1 && n <= 24) {
		font_minsize = (WORD)n;
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
				*(long*)arg = n;
			}
		
		} else if (param[0] != '0' || !param[1]) { /* VDI colour index */
			char * tail = param;
			long   n    = strtoul (param, &tail, 10);
			if (tail > param && n <= 0xFF) {
				background_colour = (WORD)n;
			}
		
		} else if (param[1] == 'x') { /* 0xRRGGBB direct colour value */
			char * tail = param;
			long   n    = strtoul (param, &tail, 16);
			if (tail > param && n <= 0xFFFFFFL) {
				*(long*)arg = n;
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
cfg_avwin (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n > 0) {
		cfg_AVWindow  = TRUE;
		if (n > 1) {
			cfg_GlobalCycle  = TRUE;
		}
	}
}

/*----------------------------------------------------------------------------*/
static void
cfg_tout_conn (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n > 0) cfg_ConnTout = (WORD)n;
}

/*----------------------------------------------------------------------------*/
static void
cfg_tout_hdr (char * param, long arg)
{
	long n = strtoul (param, &param, 10);
	long m = strtoul (param, &param, 10);
	(void)arg;
	if (n > 0) hdr_tout_doc = n *1000;
	if (m > 0) hdr_tout_gfx = m *1000;
}

/*----------------------------------------------------------------------------*/
static void
cfg_retry (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n > 0) cfg_ConnRetry = (WORD)n;
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
cfg_linespc (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n >= -1 && n <= 16) {
		cfg_LineSpacing = (WORD)n;
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


/*----------------------------------------------------------------------------*/
static void
cfg_restrict (char * param, long arg)
{
	ULONG flags;
	(void)arg;
	if (*param == '*') {
		flags = 0x7FFFFFFFuL;
		param++;
	} else {
		flags = 0uL;
		while (isalpha(*param)) {
			flags |= (1uL << (toupper(*(param++)) - 'A'));
		}
	}
	if (!isspace(*param)) {
		flags = 0uL;
	} else {
		while (isspace(*(++param)));
	}
	if (flags) {
		if (isalnum(*param))    location_DBhost   (param, 0, &flags);
		else if (*param == '.') location_DBdomain (param, 0, &flags);
		else if (*param == '/') location_DBpath   (param, 0, &flags);
	}
}


/*----------------------------------------------------------------------------*/
static void
cfg_http_proxy (char * param, long arg)
{
	char * p = param;
	(void)arg;
	while (*p && *p != ':' && !isspace (*p)) p++;
	if (isspace (*p)) {
		*p = '\0';
		while (isspace (*(++p)));
	}
	if (*p == ':') {
		*(p++) = '\0';
	}
	if (p > param) {
		long port = atol (p);
		hhtp_proxy (param, (port > 0  && port <= 0xFFFF ? port : 0));
	}
}

/*----------------------------------------------------------------------------*/
static void cfg_protocol (char * param, long arg)
{
	long n = atol (param);
  
  if (arg == 0)
  {
    if ((n >= SECURE_PROTOCOL_MIN) && (n <= SECURE_PROTOCOL_MAX)) { cfg_SecProtMin = (UWORD)n; }
  }
  else if (arg == 1)
  {
    if ((n >= SECURE_PROTOCOL_MIN) && (n <= SECURE_PROTOCOL_MAX)) { cfg_SecProtMax = (UWORD)n; }
  }
}

/*----------------------------------------------------------------------------*/
static void
cfg_csfile (char * param, long arg)
{
	(void)arg;
	ldg_mbedtls_set_csfile(param);
}

/*----------------------------------------------------------------------------*/
static void
cfg_trusted (char * param, long arg)
{
	(void)arg;
	ldg_mbedtls_set_trusted_domains(param);
}


/*----------------------------------------------------------------------------*/
static void
cfg_logfile (char * param, long arg)
{
	(void)arg;
	log_setfile (param);
}


/*----------------------------------------------------------------------------*/
static void
cfg_max_doc (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n >= 0) cfg_MaxDocument = (ULONG)n * 1024uL;  /* 0 disables the cap */
}


/*----------------------------------------------------------------------------*/
static void
cfg_max_img (char * param, long arg)
{
	long n = atol (param);
	(void)arg;
	if (n >= 0 && n <= 0xFFFF) cfg_MaxImages = (UWORD)n;  /* 0 disables it */
}


/*----------------------------------------------------------------------------*/
static void
cfg_urlhist (char * param, long arg)
{
	(void)arg;
	hwWind_urlhist (NULL, param);
}


/*----------------------------------------------------------------------------*/
static void
cfg_data_dir (char * param, long arg)
{
	size_t n = strlen (param);
	(void)arg;
	if (n > 0 && n < sizeof(fsel_path) -2) {
		char sep = (*param == '/' ? '/' : '\\');
		memcpy (fsel_path, param, n);
		if (fsel_path[n-1] != sep) {
			fsel_path[n++] = sep;
		}
		fsel_path[n] = '\0';
	}
}


/*----------------------------------------------------------------------------*/
typedef struct s_devl_flag * DEVL_FLAG;
struct s_devl_flag {
	DEVL_FLAG Next;
	char    * Value;
	char      Name[1];
};
static DEVL_FLAG _flag_list = NULL;
/*- - - - - - - - - - - - - - - - - - - -*/
static void
cfg_devl_flags (char * param, long arg)
{
	(void)arg;
	while (isalnum (*param)) {
		char * f_beg = param, * f_end = param;
		char * v_beg = NULL,  * v_end = NULL;
		while (isalnum (*(++f_end)));
		if (*f_end != ':') {
			param = f_end;
		} else {
			v_beg = v_end = f_end +1;
			while (isalnum (*(v_end))) v_end++;
			param = v_end;
		}
		if (f_beg < f_end) {
			size_t   f_len = f_end - f_beg;
			size_t   v_len = v_end - v_beg;
			size_t    size = sizeof (struct s_devl_flag)
			               + f_len + (v_len ? v_len +1 : 0);
			DEVL_FLAG flag = malloc (size);
			if (flag) {
				((char*)memcpy (flag->Name, f_beg, f_len))[f_len] = '\0';
				flag->Value = flag->Name + f_len;
				if (v_len) {
					((char*)memcpy (++flag->Value, v_beg, v_len))[v_len] = '\0';
				}
				flag->Next = _flag_list;
				_flag_list = flag;
			}
		}
		while (isspace(*param)) param++;
	}
}

/*============================================================================*/
const char *
devl_flag (const char * name)
{
	DEVL_FLAG flag = _flag_list;
	while (flag && strcmp (flag->Name, name) != 0) {
		flag = flag->Next;
	}
	return (flag ? flag->Value : NULL);
}


/*============================================================================*/
BOOL
read_config(void)
{
	static long backgnd = -1;
	
	char l[HW_PATH_MAX], * p, * d;
	FILE   * fp;

	dflt_maxdocument(); /* before the file is read, so MAX_DOCUMENT wins */
	fp = open_cfg ("r");

	if (!fp) {
		save_config (NULL, NULL);
		dflt_cachedir();
		cfg_UptoDate = FALSE;
		return FALSE;
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
		} else if (isalnum(*p)) {
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
				{ "AV_WINDOW",            cfg_avwin,    0 },		
				{ "BOLD_HEADER",          cfg_font,      FA(header_font, 1, 0)  },
				{ "BOLD_ITALIC_HEADER",   cfg_font,      FA(header_font, 1, 1)  },
				{ "BOLD_ITALIC_NORMAL",   cfg_font,      FA(normal_font, 1, 1)  },
				{ "BOLD_ITALIC_TELETYPE", cfg_font,      FA(pre_font,    1, 1)  },
				{ "BOLD_NORMAL",          cfg_font,      FA(normal_font, 1, 0)  },
				{ "BOLD_TELETYPE",        cfg_font,      FA(pre_font,    1, 0)  },
				{ "BOOKM_GEO",            cfg_bookm_geo, 0 },
				{ "BRWSR_GEO",            cfg_brwsr_geo, 0 },
				{ "CACHEDIR",             cfg_cachedir,  0 },
				{ "CACHEDSK",             cfg_cachedsk,  0 },
				{ "CACHEMEM",             cfg_cachemem,  0 },
				{ "COOKIES",              cfg_Func,      (long)menu_cookies     },
				{ "DATA_DIR",             cfg_data_dir,  0 },
				{ "DEVL_FLAGS",           cfg_devl_flags,0 },
				{ "DFLT_BACKGND",         cfg_backgnd,   (long)&backgnd },
				{ "FAST_IMAGES",          cfg_BOOL,      (long)&cfg_FastImages  },
				{ "FIXED_CMAP",           cfg_BOOL,      (long)&cfg_FixedCmap   },
				{ "FONT_MINSIZE",         cfg_minsize,   0 },
				{ "FONT_SIZE",            cfg_fntsize,   0 },
				{ "FORCE_FRAMECTRL",      cfg_Func,      (long)menu_frm_ctrl    },
				{ "GLOBAL_WINCYCLE",      cfg_BOOL,      (long)&cfg_GlobalCycle },
				{ "GREY_PALETTE",         cfg_BOOL,      (long)&cfg_GreyPalette },
				{ "HEADER",               cfg_font,      FA(header_font, 0, 0)  },
				{ "HIGHWIRE",             cfg_up2date,   0 },
				{ "HOTLIST_OPEN",         cfg_BOOL,      (long)&cfg_HotlistOpen },
				{ "HTTP_PROXY",           cfg_http_proxy,0 },
				{ "IMG_ASPECT",           cfg_BOOL,      (long)&cfg_ImgAspect   },
				{ "INFOBAR",              cfg_infobar,   0 },
				{ "ITALIC_HEADER",        cfg_font,      FA(header_font, 0, 1)  },
				{ "ITALIC_NORMAL",        cfg_font,      FA(normal_font, 0, 1)  },
				{ "ITALIC_TELETYPE",      cfg_font,      FA(pre_font,    0, 1)  },
				{ "LINE_SPACING",         cfg_linespc,   0 },
				{ "LOCAL_WEB",            cfg_localweb,  0 },
				{ "LOGGING",              cfg_Func,      (long)menu_logging     },
				{ "LOG_FILE",             cfg_logfile,   0 },
				{ "MAX_DOCUMENT",         cfg_max_doc,   0 },
				{ "MAX_IMAGES",           cfg_max_img,   0 },
				{ "NORMAL",               cfg_font,      FA(normal_font, 0, 0)  },
				{ "NO_IMAGE",             cfg_BOOL,      (long)&cfg_DropImages  },
				{ "RESTRICT_HOST",        cfg_restrict,  0 },
				{ "RETRY_HEADER",         cfg_retry,     0 },
				{ "SECURE_PROTOCOL_MAX",  cfg_protocol,  1 },
				{ "SECURE_PROTOCOL_MIN",  cfg_protocol,  0 },
				{ "SERVER_CERT_VERIFY",   cfg_Func,      (long)ldg_mbedtls_verify_certs },
				{ "START_PAGE",           cfg_startpage, 0 },
				{ "TELETYPE",             cfg_font,      FA(pre_font,    0, 0)  },
				{ "TIMEOUT_CONNECT",      cfg_tout_conn, 0 },
				{ "TIMEOUT_HEADER",       cfg_tout_hdr,  0 },
				{ "TRUSTED_DOMAINS",      cfg_trusted,   0 },
				{ "URL_01",               cfg_urlhist,   0 },
				{ "URL_02",               cfg_urlhist,   1 },
				{ "URL_03",               cfg_urlhist,   2 },
				{ "URL_04",               cfg_urlhist,   3 },
				{ "URL_05",               cfg_urlhist,   4 },
				{ "URL_06",               cfg_urlhist,   5 },
				{ "URL_07",               cfg_urlhist,   6 },
				{ "URL_08",               cfg_urlhist,   7 },
				{ "URL_09",               cfg_urlhist,   8 },
				{ "URL_10",               cfg_urlhist,   9 },
				{ "USE_CSS",              cfg_Func,      (long)menu_use_css     },
				{ "VIEWER",               cfg_viewer,    0 },
				{ "VIEW_IMAGES",          cfg_Func,      (long)menu_images      },
				{ "WANTED_CIPHERSUITE",   cfg_csfile,    0 }
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
			if (cfg_UptoDate < 0) {
				cfg_UptoDate = FALSE;
			}
		}
	}
	fclose (fp);
	
	if (backgnd >= 0) { /* must be set after cfg_FixedCmap */
		save_colors();
		background_colour = remap_color (backgnd);
	}
	dflt_cachedir();
	
	if (cfg_UptoDate <= 0) {
		save_config (NULL, NULL);
	}
	
	return TRUE;
}
