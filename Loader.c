/* @(#)highwire/Loader.c
 *
 * Currently has ended up a junk file of initializations
 * loading routine and some assorted other routines
 * for file handling.
*/
#if defined (__PUREC__)
#include <tos.h>
#include <ext.h>

#elif defined (LATTICE)
#include <dos.h>
#include <mintbind.h>
#define DTA      struct FILEINFO
#define d_length size
/* I'm certain this is in a .H somewhere, just couldn't find it - Baldrick*/
#define O_RDONLY 0x00

#elif defined (__GNUC__)
#include <mintbind.h>
# define DTA      _DTA
# define d_length dta_size
# include <fcntl.h>
# include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <gem.h>

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


char fsel_path[HW_PATH_MAX];
char help_file[HW_PATH_MAX];


static char *load_file (const LOCATION, size_t * expected, size_t * loaded);


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


typedef struct s_ldr_chunk * LDRCHUNK;
struct s_ldr_chunk {
	LDRCHUNK next;
	size_t   size;
	char     data[1];
};


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
	loader->Encoding = ENCODING_WINDOWS1252;
	loader->MimeType = MIME_TEXT;
	loader->MarginW  = -1;
	loader->MarginH  = -1;
	/* */
	loader->DataSize = 0;
	loader->DataFill = 0;
	loader->Data     = NULL;
	loader->notified = FALSE;
	/* */
	loader->rdChunked = FALSE;
	loader->rdSocket  = -1;
	loader->rdLeft    = 0;
	loader->rdDest    = NULL;
	loader->rdTlen    = 0;
	loader->rdList    = loader->rdCurr = NULL;
	
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
		if (loader->rdSocket >= 0) {
			inet_close (loader->rdSocket);
		}
#endif /* USE_INET */
		if (loader->rdList) {
			LDRCHUNK chunk = loader->rdList, next;
			do {
				next =  chunk->next;
				free (chunk);
			} while ((chunk = next) != NULL);
		}
		if (loader->Data) {
			free (loader->Data);
		}
		free_location (&loader->Location);
		free (loader);
		*p_loader = NULL;
	}
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef USE_INET
static BOOL
chunked_job (void * arg, long invalidated)
{
	LOADER   loader = arg;
	BOOL     s_recv = (loader->rdDest == NULL);
	
	if (invalidated) {
		delete_loader (&loader);
		return FALSE;
	}
	
	loader->rdDest = NULL;
	loader->rdLeft = 0;
	
	while (1) {
		char * data = loader->rdTemp;
		size_t size = 0;
		
		if (!loader->rdChunked) {
			if (loader->rdSocket >= 0) {
				size = 8192;
			}
			if (size < loader->rdTlen) {
				 size = loader->rdTlen;
			}
			
		} else {
			long n = loader->rdTlen -2;
			data   = (n > 0 ? memchr (data +2, '\n', n) : NULL);
			if (!data) {
				n = inet_recv (loader->rdSocket, loader->rdTemp + loader->rdTlen,
				               sizeof (loader->rdTemp) - loader->rdTlen);
				if (n > 0) {
					loader->rdTlen += n;
					if (loader->rdTlen > 2) {
						data = memchr (loader->rdTemp +2, '\n', loader->rdTlen -2);
					}
				}
			}
			if (data) {
				*(data++) = '\0';
				if ((long)(size = strtoul (loader->rdTemp, NULL, 16)) <= 0) {
					loader->rdTlen = 0; /* end of chunks */
				} else {
					loader->rdTlen -= data - loader->rdTemp;
				}
			
			} else if (loader->rdTlen == sizeof(loader->rdTemp)) {
				printf ("rotten chunk header\n");
				loader->rdDest = NULL;
				loader->rdLeft = 0;
				break;
			
			} else {
				if (!s_recv) {
					sched_insert (chunked_job, loader, (long)loader->Target);
				}
				return TRUE;
			}
		}
		
		if (size) {
			LDRCHUNK chunk = malloc (sizeof (struct s_ldr_chunk) + size -1);
			if (!loader->rdList) loader->rdList                   = chunk;
			else                 ((LDRCHUNK)loader->rdCurr)->next = chunk;
			loader->rdCurr = chunk;
			loader->rdDest = chunk->data;
			loader->rdLeft = size;
			chunk->next = NULL;
			chunk->size = size;
			
			if (loader->rdTlen) {
				if (size > loader->rdTlen) {
					 size = loader->rdTlen;
				}
				memcpy (loader->rdDest, data, size);
				loader->rdDest   += size;
				loader->rdLeft   -= size;
				loader->DataFill += size;
				
				loader->rdTlen -= size;
				if (loader->rdTlen) { /* there are still data in the temp buffer */
					memmove (loader->rdTemp, data + size, loader->rdTlen);
				}
				if (!loader->rdLeft) {     /* chunk was completely filled from */
					loader->rdDest = NULL;  /* the temp buffer                  */
					continue;
				}
			}
		}
		break;
	}
	
	if (!loader->rdDest) {
		LDRCHUNK next = loader->rdList, chunk;
		long     left = loader->DataFill;
		char   * p    = loader->Data = malloc (left +3);
		
		while ((chunk = next) != NULL) {
			if (left > 0) {
				memcpy (p, chunk->data, (left < chunk->size ? left : chunk->size));
				p += chunk->size;
			}
			left -= chunk->size;
			next =  chunk->next;
			free (chunk);
		}
		loader->rdList   = loader->rdCurr = NULL;
		loader->DataSize = loader->DataFill;
		if (loader->rdChunked && left < 0) {
			loader->DataSize -= left;
		}
	}
	if (s_recv) {
		static BOOL receive_job (void *, long);

		sched_insert (receive_job, loader, (long)loader->Target);
	}
	
	return FALSE;
}
#endif /* USE_INET */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef USE_INET
static BOOL
receive_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	
	if (invalidated) {
		delete_loader (&loader);
		return FALSE;
	}
	
	if (loader->rdLeft) {
		long n = inet_recv (loader->rdSocket, loader->rdDest, loader->rdLeft);
		
		if (n < 0) { /* no more data available */
			inet_close (loader->rdSocket);
			loader->rdSocket = -1;
			loader->rdLeft   = 0;
		
		} else if (n) {
			loader->rdDest   += n;
			loader->rdLeft   -= n;
			loader->DataFill += n;
		}
		if (loader->rdLeft) {
			return TRUE;  /* re-schedule for next block of data */
		}
		
		if (!loader->DataSize) {
			if (chunked_job (arg, 0)) {
				return FALSE;  /* chunk head need to be completed */
			
			} else if (loader->rdLeft) {
				return TRUE;  /* re-schedule for start of next chunk */
			}
		}
	}
	/* else download finished */
	
	if (loader->rdSocket >= 0) {
		inet_close (loader->rdSocket);
		loader->rdSocket   = -1;
	}
	if (loader->Data) {
		char * p = loader->Data + loader->DataFill;
		*(p++) = '\0';
		*(p++) = '\0';
		*(p)   = '\0';
	}
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
		char buf[1024];
		location_FullName (loader->Location, buf, sizeof(buf));
		loader->notified = containr_notify (loader->Target, HW_PageStarted, buf);
	}
	
	if (loader->Location->Proto == PROT_HTTP) {
#ifdef USE_INET
		const char * host = location_Host (loader->Location);
		HTTP_HDR     hdr;
		short        sock = -1;
		short        reply;
		UWORD        retry = 0;
		
		if (host) {
			char buf[300];
			sprintf (buf, "Connecting: %.*s", (int)(sizeof(buf) -13), host);
			containr_notify (loader->Target, HW_SetInfo, buf);
		}
		do {
			reply = http_header (loader->Location,
			                     &hdr, &sock, sizeof (loader->rdTemp) -2);
		} while (reply == -ECONNRESET && retry++ < 1);
		
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
			
		} else if (reply == 200 && MIME_Major(loader->MimeType) == MIME_TEXT) {
			char buf[300];
			sprintf (buf, "Receiving from %.*s", (int)(sizeof(buf) -16), host);
			containr_notify (loader->Target, HW_SetInfo, buf);
			
			if (hdr.Size >= 0 && !hdr.Chunked) {
				loader->Data     = loader->rdDest = malloc (hdr.Size +3);
				loader->DataSize = loader->rdLeft = hdr.Size;
				loader->DataFill = hdr.Tlen;
				if (loader->DataFill) {
					if (loader->DataFill > loader->rdLeft) {
						 loader->DataFill = loader->rdLeft;
					}
					memcpy (loader->rdDest, hdr.Tail, loader->DataFill);
					loader->rdDest += loader->DataFill;
					loader->rdLeft -= loader->DataFill;
				}
				if (sock < 0 || !loader->rdLeft) {
					loader->rdDest[0] = '\0';
					loader->rdDest[1] = '\0';
					loader->rdDest[2] = '\0';
					inet_close (sock);
				} else {
					loader->rdSocket = sock;
					sched_insert (receive_job, loader, (long)loader->Target);
					return FALSE;
				}
			
			} else {
				if ((loader->rdChunked = hdr.Chunked) == TRUE) {
					loader->rdTemp[0] = '\r';
					loader->rdTemp[1] = '\n';
					loader->rdTlen = 2;
				}
				if (hdr.Tlen) {
					memcpy (loader->rdTemp + loader->rdTlen, hdr.Tail, hdr.Tlen);
					loader->rdTlen += hdr.Tlen;
				}
				loader->rdSocket = sock;
				sched_insert (chunked_job, loader, (long)loader->Target);
				return FALSE;
			}
		
		} else {
			loader->MimeType = MIME_TEXT;
			loader->Data     = strdup (hdr.Head);
		}
#endif /* USE_INET */
	} else {
		loader->Data = load_file (loader->Location,
		                          &loader->DataSize, &loader->DataFill);
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
LOADER
new_loader_job (const char *address, LOCATION base, CONTAINR target)
{
	LOCATION loc    = new_location (address, base);
	LOADER   loader = new_loader (loc);
	
	loader->Target   = target;

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
		char buf[1024];
		location_FullName (loader->Location, buf, sizeof(buf));
		loader->notified = containr_notify (loader->Target, HW_PageStarted, buf);
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
		char buf[1024];
		location_FullName (loader->Location, buf, sizeof(buf));
		loader->notified = containr_notify (loader->Target, HW_PageStarted, buf);
		sched_insert (loader_job, loader, (long)loader->Target);
	}
	
	free_location (&loc);
	
	return loader;
}

/*============================================================================*/
void
loader_setParams (LOADER loader,
                  ENCODING encoding, short margin_w, short margin_h)
{
	if (loader) {
		if (encoding > ENCODING_Unknown) loader->Encoding = encoding;
		if (margin_w >= 0)               loader->MarginW  = margin_w;
		if (margin_h >= 0)               loader->MarginH  = margin_h;
	}
}


/******************************************************************************/


/*==============================================================================
 * load_file()
 *
 * I modified the hell out of this to do local searching
 * It is probably not the prettiest.  - baldrick July 10, 2001
*/
static char *
load_file (const LOCATION loc, size_t * expected, size_t * loaded)
{
	const char *filename = loc->FullName;
	long         size = 0;
	char       * file = NULL;
	struct xattr file_info;
	long         xret = Fxattr(0, filename, &file_info);
	
	if (xret == E_OK) {  /* Fxattr() exists */
		size = file_info.st_size;
	
	} else if (xret == -EINVFN) {  /* here for TOS filenames */
		DTA  new, * old = Fgetdta();
		Fsetdta(&new);

		if (Fsfirst(filename, 0) == E_OK) {
			size = new.d_length;
		}
		Fsetdta(old);
	
	} else {
		*expected = *loaded = 0;
		return NULL;
	}
	
	*expected = size;
	
	if ((file = malloc (size + 3)) == NULL) {
		size = 0;
	
	} else if (size) {
		int fh = open (filename, O_RDONLY);
		if (fh < 0) {
			size = 0;
		} else {
			size = read (fh, file, size);
			close (fh);
		}
		file[size +0] = '\0';
		file[size +1] = '\0';
		file[size +2] = '\0';
	}
	*loaded = size;
	
	return file;
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


/*==============================================================================
 * This identifies what AES you are running under.
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


/*==============================================================================
 * Looks for MiNT or MagiC Cookies.  If found TRUE returned,
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
