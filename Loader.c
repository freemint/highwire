/* @(#)highwire/Loader.c
 *
 * Currently has ended up a junk file of initializations
 * loading routine and some assorted other routines
 * for file handling.
*/
#ifdef __PUREC__
#include <tos.h>
#include <ext.h>
#endif

#ifdef LATTICE
#include <dos.h>
#include <mintbind.h>

/* I'm certain this is in a .H somewhere, just couldn't find it - Baldrick*/
#define O_RDONLY    0x00
#define DTA struct FILEINFO
#define d_length size
#define E_OK		0x00
#endif

#ifdef __GNUC__
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <mintbind.h>
#endif

#include <gemx.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "global.h"
#include "av_comm.h"
#include "schedule.h"
#include "Containr.h"
#include "Location.h"
#include "token.h"
#include "Loader.h"
#include "parser.h"
#include "http.h"
#include "inet.h"
#include "cache.h"
#include "hwWind.h"


static char *load_file (const LOCATION);


/*******************************************************************************
 * the following function should placed either in parse.c or render.c but
 * at the moment I have no clue where to do it best - AltF4 Feb. 4, 2002
 */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * job function for parsing a file and attach it to the passed frame
 */
static BOOL
parser_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	PARSER parser;
	
	if (invalidated) {
		delete_loader (&loader);
		
		return FALSE;
	}
	
	parser = new_parser (loader);
	
	switch (loader->MimeType)
	{
		case MIME_IMAGE:
			parse_image (parser, 0);
			break;
		
		case MIME_TXT_HTML:
			if (loader->Data) {
				parse_html (parser, 0);
			
			} else if (PROTO_isLocal(loader->Location->Proto)) {
				parse_dir (parser, 0);
			
			} else { /* PROT_ABOUT -- also catch internal errors */
				parse_about (parser, 0);
			}
			break;
		
		case MIME_TEXT: {
			char * p = loader->Data;
			while (isspace(*p)) p++;
		#ifdef MOD_TROFF
			if (p[0] == '.' &&
			    (p[1] == '"' || (p[1] == '\\' && p[2] == '"') || isalpha(p[1]))) {
				extern BOOL parse_troff (void *, long);
				parse_troff (parser, 0);
				break;
			}
		#endif
			if (strncmp(p, "<!--", 4) == 0) {
				long i = min(500, (long)(loader->Data - p) + loader->DataSize - 15);
				while (--i > 0 && *(p++) != '>');
				while (--i > 0 && isspace(*p)) p++;
			}
			if (strnicmp (p, "<html>",          6) == 0 ||
			    strnicmp (p, "<!DOCTYPE HTML", 14) == 0) {
				parse_html (parser, 0);
				break;
			}
		}
		default: /* pretend it's text */
			parse_plain (parser, 0);
	}
	
	return FALSE;
}

/******************************************************************************/


/*============================================================================*/
const struct {
	const char * Ext;
	const MIMETYPE Type;
	const char * Appl;
}
mime_list[] = {
	{ "au",   MIME_AUDIO,      "GEMJing"  },
	{ "avr",  MIME_AUDIO,      "GEMJing"  },
	{ "dvs",  MIME_AUDIO,      "GEMJing"  },
	{ "gif",  MIME_IMG_GIF,    NULL       },
	{ "hsn",  MIME_AUDIO,      "GEMJing"  },
	{ "htm",  MIME_TXT_HTML,   NULL       },
	{ "html", MIME_TXT_HTML,   NULL       },
	{ "hyp",  MIME_APPL,       "ST-Guide" },
	{ "img",  MIME_IMG_X_XIMG, ""         },
	{ "jpeg", MIME_IMG_JPEG,   ""         },
	{ "jpg",  MIME_IMG_JPEG,   ""         },
	{ "mpg",  MIME_VID_MPEG,   "ANIPLAY"  },
	{ "pdf",  MIME_APP_PDF,    "MyPdf"    },
	{ "png",  MIME_IMG_PNG,    ""         },
	{ "snd",  MIME_AUDIO,      "GEMJing"  },
	{ "txt",  MIME_TXT_PLAIN,  NULL       },
	{ "wav",  MIME_AUDIO,      "GEMJing"  }
};
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
LOADER
new_loader (LOCATION loc)
{
	const char * ext;
	LOADER loader = malloc (sizeof (struct s_loader));
	loader->Location = location_share (loc);
	loader->Target   = NULL;
	loader->MimeType = MIME_TEXT;
	loader->notified = FALSE;
	loader->Socket   = -1;
	loader->isChunked = 0;
	loader->DataSize  = 0;
	loader->DataFill  = 0;
	loader->Data      = NULL;
	
	if ((loc->Proto == PROT_FILE || PROTO_isRemote (loc->Proto)) &&
	    (ext = strrchr (loc->File, '.')) != NULL && *(++ext)) {
		/*
		 * resolve the MIME type depending on the file name extension
		 */
		size_t i = 0;
		do if (stricmp (ext, mime_list[i].Ext) == 0) {
			loader->MimeType = mime_list[i].Type;
			if (mime_list[i].Appl) {
				loader->Data  = strdup (mime_list[i].Appl);
			}
			break;
		} while (++i < numberof(mime_list));
	}
	return loader;
}


/*============================================================================*/
void
delete_loader (LOADER * p_loader)
{
	LOADER loader = *p_loader;
	if (loader) {
		if (loader->notified) {
			containr_notify (loader->Target, HW_PageFinished, NULL);
		}
#ifdef USE_INET
		inet_close (loader->Socket);
#endif /* USE_INET */
		free_location (&loader->Location);
		if (loader->Data) free (loader->Data);
		free (loader);
		*p_loader = NULL;
	}
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef USE_INET
static BOOL
receive_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	long   left = loader->DataSize - (loader->DataFill - loader->isChunked);
	char * p    = loader->Data     +  loader->DataFill;
	long   n    = -1;
	
	if (invalidated) {
		delete_loader (&loader);
		
		return FALSE;
	}
	
/*	printf ("recive '%s' (%li) %li/%li+%i \n",
	        loader->Location->FullName, loader->DataFill,
	        left, loader->DataSize, loader->isChunked);
*/	
	while (left > 0 && (n = inet_recv (loader->Socket, p, left)) > 0) {
		p    += n;
		left -= n;
	}
	loader->DataFill = p - loader->Data;
	*p = '\0';
	
	if (!n && left) {
		return TRUE; /* re-schedule */
	}
	
	if (loader->isChunked && loader->DataFill > loader->DataSize) {
		char * ln_brk;
		char * chunk = loader->Data + loader->DataSize;
		*(chunk++) = '\0';
		if (*chunk == '\n') chunk++;
		
		loader->DataFill = loader->DataSize;
		
		if ((ln_brk = strchr (chunk, '\n')) != NULL) {
			char * tail = chunk;
			long   size = strtoul (chunk, &tail, 16);
			if (size > 0 && tail > chunk && tail <= ln_brk) {
				char * mem;
				loader->DataSize += size;
				if ((mem = malloc (loader->DataSize
				                   + loader->isChunked +1)) != NULL) {
					memcpy (mem, loader->Data, loader->DataFill);
					while (*(++ln_brk)) {
						mem[loader->DataFill++] = *ln_brk;
					}
					free (loader->Data);
					loader->Data = mem;
					
					return TRUE; /* re-schedule */
				}
			}
		}
	}
	inet_close (loader->Socket);
	loader->Socket   = -1;
	
	sched_insert (parser_job, loader, (long)loader->Target);
	
	return FALSE;
}
#endif /* USE_INET */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
loader_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	
	if (invalidated) {
		delete_loader (&loader);
		
		return FALSE;
	}
	
	if (!loader->notified) {
		loader->notified = containr_notify (loader->Target, HW_PageStarted,
		                                    loader->Location->FullName);
	}
	
	if (loader->Location->Proto == PROT_HTTP) {
#ifdef USE_INET
		HTTP_HDR hdr;
		short    sock  = -1;
		short    reply;
		
		{ /*  */
			char buf[300];
			sprintf (buf, "Connecting: %.*s",
			         (int)(sizeof(buf) -13), location_Host (loader->Location));
			containr_notify (loader->Target, HW_SetInfo, buf);
		}
		
		reply = http_header (loader->Location, &hdr, &sock);
		
		if (hdr.MimeType) {
			loader->MimeType = hdr.MimeType;
			if (hdr.Encoding) {
				loader->Encoding = hdr.Encoding;
			}
		}
		if ((reply == 301 || reply == 302) && hdr.Rdir) {
			LOCATION loc = new_location (hdr.Rdir, loader->Location);
			free_location (&loader->Location);
			loader->Location = loc;
			inet_close (sock);
			containr_notify (loader->Target, HW_PageFinished, NULL);
			loader->notified = FALSE;
			
			return TRUE; /* re-schedule with the new location */
			
		} else if (reply == 200 && MIME_Major(loader->MimeType) == MIME_TEXT
		           && hdr.Size > 0) {
			loader->isChunked = (hdr.Chunked ? 10 : 0);
			loader->DataSize  = hdr.Size;
			loader->DataFill  = strlen (hdr.Tail);
			
			if (sock < 0) {
				loader->Data = malloc (loader->DataFill +1);
				memcpy (loader->Data, hdr.Tail, loader->DataFill +1);
				inet_close (sock);
			
			} else {
				loader->Socket = sock;
				loader->Data   = malloc (loader->DataSize + loader->isChunked +1);
				memcpy (loader->Data, hdr.Tail, loader->DataFill);
				
				sched_insert (receive_job, loader, (long)loader->Target);
				return FALSE;
			}
		
		} else {
			loader->MimeType = MIME_TEXT;
			loader->Data     = strdup (hdr.Head);
		}
#endif /* USE_INET */
	} else {
		loader->Data = load_file (loader->Location);
	}
	if (!loader->Data) {
		loader->Data     = strdup ("<html><head><title>Error</title></head>"
		                           "<body><h1>Page not found!</h1></body></html>");
		loader->MimeType = MIME_TXT_HTML;
	}
	/* registers a parser job with the scheduler */
	sched_insert (parser_job, loader, (long)loader->Target);

	return FALSE;
}


/*----------------------------------------------------------------------------*/
static short
find_application (const char * name)
{
	short id = -1;

	if (name == NULL && ((name = getenv ("AVSERVER")) == NULL || !*name)) {
		static const char * list[] = {
			"AVServer", "Jinnee", "Thing", "MagxDesk", "GEMINI",
			"StrngSrv", "DIRECT", "EASY",  "KAOS",
			NULL };
		const char ** desktop = list;
		while ((id = find_application (*desktop)) < 0 && *(++desktop));

	} else if (name && *name) {
		char buf[] = "        ", * p = buf;
		while (*name && *p) *(p++) = toupper (*(name++));
		id = appl_find (buf);
	}
	return id;
}


/*==============================================================================
 * small routine to call external viewer to view the SRC to
 * a file so that realtime comparisons can be made
 * baldrick August 9, 2002
*/
void
launch_viewer(const char *name)
{
	short id = find_application ("Viewer");

	if (id >= 0 || (id = find_application (NULL)) >= 0) {
		short msg[8];
		msg[0] = (VA_START);
		msg[1] = gl_apid;
		msg[2] = 0;
		*(char**)(msg +3) = strcpy (va_helpbuf, name);
		msg[5] = msg[6] = msg[7] = 0;
		appl_write (id, 16, msg);
	}
}

/*==============================================================================
 * registers a loader job with the scheduler.
 *
 * The parameters 'address', 'base' or 'target' can be NULL.
 * address == NULL: reread the file in base with default_encoding
 */
void
new_loader_job (const char *address, LOCATION base, CONTAINR target,
                ENCODING encoding, short margin_w, short margin_h)
{
	LOCATION loc    = new_location (address, base);
	LOADER   loader = new_loader (loc);
	
	loader->Target   = target;
	loader->Encoding = encoding;
	loader->MarginW  = margin_w;
	loader->MarginH  = margin_h;

	if (loc->Proto == PROT_DIR) {
		loader->MimeType = MIME_TXT_HTML;
		sched_insert (parse_dir, new_parser (loader), (long)loader->Target);
		
	} else if (loc->Proto == PROT_ABOUT) {
		loader->MimeType = MIME_TXT_HTML;
		sched_insert (parse_about, new_parser (loader), (long)loader->Target);

	} else if (loc->Proto == PROT_MAILTO) {
		short id = id = find_application (NULL);
		if (id >= 0) {
			short msg[8] = { VA_START, };
			msg[1] = gl_apid;
			msg[2] = msg[5] = msg[6] = msg[7] = 0;
			*(char**)(msg +3) = strcat (strcpy (va_helpbuf, "mailto:"),
			                            loc->File);
			appl_write (id, 16, msg);
		}
		delete_loader (&loader);
	
	} else if (loader->Data) {
		short           id = find_application (loader->Data);
		if (id >= 0 || (id = find_application (NULL)) >= 0) {
			short msg[8];
			msg[0] = (loader->MimeType == MIME_APP_PDF
			          ? PDF_AV_OPEN_FILE : VA_START);
			msg[1] = gl_apid;
			msg[2] = 0;
			*(char**)(msg +3) = strcpy (va_helpbuf, loc->FullName);
			msg[5] = msg[6] = msg[7] = 0;
			appl_write (id, 16, msg);

			if (target && !target->Mode) {
				free (loader->Data);
				loader->Data     = strdup ("\n\tLoading application...\n");
				loader->MimeType = MIME_TXT_PLAIN;

				sched_insert (parser_job, loader, (long)loader->Target);

			} else {
				delete_loader (&loader);
			}
		}
		
#ifdef USE_INET
	} else if (loc->Proto == PROT_HTTP) {
		loader->notified = containr_notify (loader->Target, HW_PageStarted,
		                                    loader->Location->FullName);
		sched_insert (loader_job, loader, (long)loader->Target);
#endif /* USE_INET */
	
	} else if (loc->Proto) {
		loader->Data     = strdup ("<html><head><title>Error</title></head>"
		                           "<body><h1>Protocol not supported!</h1></body>"
		                           "</html>");
		loader->MimeType = MIME_TXT_HTML;
		sched_insert (parser_job, loader, (long)loader->Target);
	
	} else if (MIME_Major(loader->MimeType) == MIME_IMAGE) {
		loader->MimeType = MIME_IMAGE;
		sched_insert (parse_image, new_parser (loader), (long)loader->Target);
		
	} else {
		loader->notified = containr_notify (loader->Target, HW_PageStarted,
		                                    loader->Location->FullName);
		sched_insert (loader_job, loader, (long)loader->Target);
	}
	
	free_location (&loc);
}


/******************************************************************************/
static char fsel_path[HW_PATH_MAX];
char help_file[HW_PATH_MAX];


/*==============================================================================
 * load_file()
 *
 * I modified the hell out of this to do local searching
 * It is probably not the prettiest.  - baldrick July 10, 2001
*/
static char *
load_file (const LOCATION loc)
{
	const char *filename = loc->FullName;
	long   size = 0;
	char * file = NULL;
	
#ifdef DEBUG
	fprintf (stderr, "load_file: %s\n", filename);
#endif

#ifndef LATTICE
	struct stat file_info;

	if (stat (filename, &file_info) == 0) {
		size = file_info.st_size;
	}
	
#else
	/* for Lattice */
	struct xattr file_info;
	long r = Fxattr(0, filename, &file_info);
	
	if (r == E_OK) {  /* Fxattr() exists */
		size = file_info.st_size;
	
	} else if (r == EINVFN) {  /* here for TOS filenames */
		DTA  new, * old = Fgetdta();
		Fsetdta(&new);

		if (Fsfirst(filename, 0) == E_OK) {
			size = new.d_length;
		}
		Fsetdta(old);
	}
#endif

	if (size) {
		int fh = open (filename, O_RDONLY);

		file = malloc (size + 3);
		if (fh > 0 && file) {
			read (fh, file, size);
			close (fh);

			file[size +0] = '\0';
			file[size +1] = '\0';
			file[size +2] = '\0';
		}
	}
	
	return file;
}


/*==============================================================================
 * page_load()
 *
 * calls the fileselector and loads the selected file
 *
 * added 10-04-01 mj.
 *
 * simplified AltF4 December 26, 2001
 *
 * return modified Rainer Seitel May 2002
*/
BOOL
page_load(void)
{
	char fsel_file[HW_PATH_MAX] = "";
	WORD r, butt;  /* file selector exit button */

	if ((gl_ap_version >= 0x140 && gl_ap_version < 0x200)
	    || gl_ap_version >= 0x300 /* || getcookie(FSEL) */) {
		r = fsel_exinput (fsel_path, fsel_file, &butt,
		                  "HighWire: Open HTML or text");
	} else {
		r = fsel_input(fsel_path, fsel_file, &butt);
	}
	if (r && butt != FSEL_CANCEL) {
		char * slash = strrchr (fsel_path, '\\');
		if (slash) {
			char   file[HW_PATH_MAX];
			size_t len = slash - fsel_path +1;
			memcpy (file, fsel_path, len);
			strcpy (file + len, fsel_file);
			new_loader_job (file, NULL,
			                hwWind_Top->Pane, ENCODING_WINDOWS1252, -1,-1);
		} else {
			butt = FALSE;
		}
	}
	return butt;
}


/*==============================================================================
 * init_paths()
 *
 * Handles the initialization of the file paths
 *
 *  Currently just sets last_location to something in the directory
 * from which HighWire was launched, so that people on certain
 * systems can get the default values
 *
 * this could be useful for config files, RSC files etc.
 *
 * baldrick (August 14, 2001)
*/
void
init_paths(void)
{
	char temp_location[HW_PATH_MAX];
	char config_file[HW_PATH_MAX];

	temp_location[0] = Dgetdrv();
	temp_location[0] += (temp_location[0] < 26 ? 'A' : -26 + '1');
	temp_location[1] = ':';
	Dgetpath (temp_location + 2, 0);

	if (temp_location[strlen(temp_location) - 1] != '\\') {
		strcat(temp_location,"\\");
	}
	strcpy(config_file, temp_location);  /* save the start path */
	strcat(config_file, _HIGHWIRE_CFG_);
	read_config(config_file);

	strcpy(fsel_path, temp_location);  /* used by the first file selector call */
	strcat(fsel_path, "html\\*.HTM*");  /* uppercase is for the TOS file selector */

	strcpy(help_file, temp_location);  /* used for keyboard F1 or Help */
	strcat(help_file, "html\\hwdoc.htm#Use");
}


/* This identifies what AES you are running under.
 */

WORD
identify_AES(void)
{
	long	search_id;
	long	*search_p = (long *)Setexc(0x5A0/4, (void (*)())-1);
	WORD	retv = AES_single;

	if (search_p != NULL) {
		while ((search_id = *search_p) != 0) {
			if (search_id == 0x4D674D63L/*MgMc*/ || search_id == MagX_cookie)
				retv = AES_MagiC;
			if (search_id == 0x6E414553L/*nAES*/)
				retv = AES_nAES;
			if (search_id == 0x476E7661L/*Gnva*/)
				retv = AES_Geneva;

			search_p += 2;
		}
	}

	return retv;
}	/* Ends:	WORD identify_AES(void) */


/* Looks for MiNT or MagiC Cookies.  If found TRUE returned,
 * which indicates we can call extended Mxalloc() with GLOBAL flag.
 */

BOOL
can_extended_mxalloc(void)
{
	long	search_id;
	long	*search_p = (long *)Setexc(0x5A0/4, (void (*)())-1);
	BOOL	retv = FALSE;

	if (search_p != NULL) {
		while ((search_id = *search_p) != 0) {
			if (search_id == 0x4D674D63L || search_id == MagX_cookie
			   || search_id == 0x4D694E54L) {
				retv = TRUE;
				break;
			}
			search_p += 2;
		}
	}

	return retv;
}
