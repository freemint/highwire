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

#include <errno.h>
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT 305
#endif

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


static short  start_application (const char * appl, LOCATION loc);


/*______________return_values_of_scheduled_jobs,_controlling_their_priority___*/
#define JOB_KEEP 1  /* restart again later and doesn't change priority   */
#define JOB_AGED -1 /* restart, but decrease the prio a bit because the job is
                     * running already a bit longer now   */
#define JOB_NOOP -2 /* restart also, but decrease the priority much more because
                     * this job didn't got anything to do (was stalled)   */
#define JOB_DONE 0  /* job is done, remove from list and don't start it again */

/*____________________________________________priorities_for_following_jobs___*/
#define PRIO_INTERN   200   /* internal produced pages like error pages, about,
                      * dir listings etc, get the highest priority in general */
#define PRIO_USERACT  100   /* start of loading something initiated by a user
                      * action, normally a link clicked.  this should have be
                      * the next highest value   */
#define PRIO_AUTOMATIC 10   /* start loading some part of a page, usually a
                      * frame */
#define PRIO_TRIVIAL    1   /* start loading some object, usually an image   */
#define PRIO_RECIVE    20   /* begin receiving data from remote   */
#define PRIO_SUCCESS   91   /* decode a file, usually an image    */
#define PRIO_LOADFILE  95   /* load a local file into memory      */
#define PRIO_FINISH    99   /* decode from memory, usually html or text   */
#define PRIO_KEEP       0   /* generic, when nothing from above matches   */

static int loader_job  (void * arg, long invalidated);
static int header_job  (void * arg, long invalidated);
static int receive_job (void * arg, long invalidated);
static int generic_job (void * arg, long invalidated);


/*----------------------------------------------------------------------------*/
static BOOL
start_parser (LOADER loader)
{
	SCHED_FUNC func = parse_plain;
	PARSER     parser;
	
	if (!loader->MimeType) {
		loader->MimeType = MIME_TEXT;
	}
	parser = new_parser (loader);
	
	switch (loader->MimeType)
	{
		case MIME_IMAGE:
			func = parse_image;
			break;
		
		case MIME_TXT_HTML:
			if (loader->Data) {
				func = parse_html;
			
			} else if (PROTO_isLocal(loader->Location->Proto)) {
				func = parse_dir;
			
			} else { /* PROT_ABOUT -- also catch internal errors */
				func = parse_about;
			}
			break;
		
		case MIME_TEXT: {
			char * p = loader->Data;
			while (isspace(*p)) p++;
		#ifdef MOD_TROFF
			if (p[0] == '.' &&
			    (p[1] == '"' || (p[1] == '\\' && p[2] == '"') || isalpha(p[1]))) {
				extern int parse_troff (void *, long);
				func = parse_troff;
				break;
			}
		#endif
			while (strncmp(p, "<!--", 4) == 0) {
				long i = min(500, (long)(loader->Data - p) + loader->DataSize - 15);
				while (--i > 0 && *(p++) != '>');
				while (--i > 0 && isspace(*p)) p++;
			}
			if (strnicmp (p, "<html>",          6) == 0 ||
			    strnicmp (p, "<!DOCTYPE HTML", 14) == 0) {
				func = parse_html;
				break;
			}
		}
		default: /* pretend it's plain text */ ;
	}
	
	return sched_insert (func, parser, (long)parser->Target, PRIO_FINISH);
}

/******************************************************************************/


typedef struct s_ldr_chunk * LDRCHUNK;
struct s_ldr_chunk {
	LDRCHUNK next;
	size_t   size;
	char     data[1];
};


/*============================================================================*/
LOADER
new_loader (LOCATION loc, CONTAINR target)
{
	const char * appl = NULL;
	LOADER loader = malloc (sizeof (struct s_loader));
	loader->Location   = location_share (loc);
	loader->Target     = target;
	loader->Encoding   = ENCODING_WINDOWS1252;
	loader->MimeType   = MIME_Unknown;
	loader->FileExt[0] = '\0';
	loader->MarginW    = -1;
	loader->MarginH    = -1;
	loader->ScrollV    = 0;
	loader->ScrollH    = 0;
	/* */
	loader->Cached   = NULL;
	loader->Date     = 0;
	loader->Expires  = 0;
	loader->DataSize = 0;
	loader->DataFill = 0;
	loader->Data     = NULL;
	loader->notified = FALSE;
	loader->Error    = E_OK;
	/* */
	loader->SuccJob = NULL;
	loader->FreeArg = NULL;
	/* */
	loader->rdChunked = FALSE;
	loader->rdSocket  = -1;
	loader->rdLeft    = 0;
	loader->rdDest    = NULL;
	loader->rdTlen    = 0;
	loader->rdList    = loader->rdCurr = NULL;
	
	if (loc->Proto == PROT_FILE || PROTO_isRemote (loc->Proto)) {
		loader->MimeType = mime_byExtension (loc->File, &appl, loader->FileExt);
	}
	
	if (loc->Proto == PROT_HTTP) {
		CACHED cached = cache_lookup (loc, 0, NULL);
		if (cached) {
			union { CACHED c; LOCATION l; } u;
			u.c            = cache_bound (cached, &loader->Location);
			loader->Cached = u.l;
			if (!loader->MimeType) {
				loader->MimeType = mime_byExtension (loader->Cached->File,
				                                     &appl, NULL);
			}
		}
	}
	
	loader->ExtAppl = (appl ? strdup (appl) : NULL);
	
	containr_notify (loader->Target, HW_ActivityBeg, NULL);
	
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
		containr_notify (loader->Target, HW_ActivityEnd, NULL);

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
		if (loader->ExtAppl) {
			free (loader->ExtAppl);
		}
		if (loader->Data) {
			free (loader->Data);
		}
		if (loader->Cached) {
			cache_release ((CACHED*)&loader->Cached, FALSE);
		}
		free_location (&loader->Location);
		free (loader);
		*p_loader = NULL;
	}
}


/*============================================================================*/
LOADER
start_page_load (CONTAINR target, const char * url, LOCATION base, BOOL u_act)
{
	LOCATION loc    = (url ? new_location (url, base) : location_share (base));
	LOADER   loader = NULL;
	
	if (loc->Proto == PROT_MAILTO || loc->Proto == PROT_FTP) {
		start_application (NULL, loc);
	
	} else {
		loader = start_cont_load (target, NULL, loc, u_act);
	}
	free_location (&loc);
	
	return loader;
}

/*============================================================================*/
LOADER
start_cont_load (CONTAINR target, const char * url, LOCATION base, BOOL u_act)
{
	LOCATION loc    = (url ? new_location (url, base) : location_share (base));
	LOADER   loader = new_loader (loc, target);
	
	free_location (&loc);
	loc = (loader->Cached ? loader->Cached : loader->Location);
	
	if (!loader->notified) {
		char buf[1024];
		location_FullName (loader->Location, buf, sizeof(buf));
		loader->notified = containr_notify (loader->Target, HW_PageStarted, buf);
	}
	
	if (loc->Proto == PROT_DIR) {
		loader->MimeType = MIME_TXT_HTML;
		sched_insert (parse_dir, new_parser (loader), (long)target, PRIO_INTERN);
		
	} else if (loc->Proto == PROT_ABOUT) {
		loader->MimeType = MIME_TXT_HTML;
		sched_insert (parse_about, new_parser (loader),(long)target, PRIO_INTERN);
		
	} else if (MIME_Major(loader->MimeType) == MIME_IMAGE) {
		loader->MimeType = MIME_IMAGE;
		sched_insert (parse_image, new_parser (loader),(long)target, PRIO_INTERN);
		
#ifdef USE_INET
	} else if (loc->Proto == PROT_HTTP) {
		if (loader->ExtAppl) {
			loader->SuccJob = generic_job;
			containr_notify (loader->Target, HW_PageFinished, NULL);
			loader->notified = FALSE;
		}
		sched_insert (header_job, loader, (long)target,
		              (u_act ? PRIO_USERACT : PRIO_AUTOMATIC));
#endif /* USE_INET */
	
	} else if (loc->Proto) {
		const char txt[] = "<html><head><title>Error</title></head><body>"
		                   "<h1>Protocol #%i not supported!</h1></body></html>";
		char buf [sizeof(txt) +10];
		sprintf (buf, txt, loc->Proto);
		loader->Error    = -EPROTONOSUPPORT;
		loader->Data     = strdup (buf);
		loader->MimeType = MIME_TXT_HTML;
		sched_insert (parse_html, new_parser (loader), (long)target, PRIO_INTERN);
	
	} else if (loader->ExtAppl) {
		start_application (loader->ExtAppl, loc);
		delete_loader (&loader);
		
	} else {
		sched_insert (loader_job, loader, (long)target, PRIO_LOADFILE);
	}
	
	return loader;
}

/*============================================================================*/
LOADER
start_objc_load (CONTAINR target, const char * url, LOCATION base,
                 int (*successor)(void*, long), void * objc)
{
	LOCATION loc  = new_location (url, base);
	LOADER loader = new_loader (loc, target);
	
	free_location (&loc);
	loc = (loader->Cached ? loader->Cached : loader->Location);
	
	if (successor) {
		loader->SuccJob = successor;
	
	} else if (loader->ExtAppl) {
		loader->SuccJob = generic_job;
	
	} else {
		printf ("start_objc_load(%s) dropped.\n", loc->FullName);
		delete_loader (&loader);
		return NULL;
	}
	loader->FreeArg = objc;
	
	if (PROTO_isLocal (loc->Proto)) {
		sched_insert (loader->SuccJob, loader, (long)target, PRIO_SUCCESS);
	
#ifdef USE_INET
	} else if (loc->Proto == PROT_HTTP) {
		sched_insert (header_job, loader, (long)target, PRIO_TRIVIAL);
#endif
	
	} else {
		printf ("start_objc_load() invalid protocol %i.\n", loc->Proto);
		loader->Error = -EPROTONOSUPPORT;
		(*loader->SuccJob)(loader, (long)target);
		loader = NULL;
	}
	
	return loader;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef USE_INET
static int
chunked_job (void * arg, long invalidated)
{
	LOADER   loader = arg;
	BOOL     s_recv = (loader->rdDest == NULL);
	
	if (invalidated) {
		return receive_job (arg, invalidated);
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
					sched_insert (chunked_job, loader, (long)loader->Target,
					              PRIO_KEEP);
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
		sched_insert (receive_job, loader, (long)loader->Target, PRIO_KEEP);
	}
	
	return JOB_DONE;
}
#endif /* USE_INET */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef USE_INET
static int
receive_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	
	if (invalidated) {
		cache_abort (loader->Location);
		if (loader->SuccJob) {
			(*loader->SuccJob)(arg, invalidated);
		} else {
			delete_loader (&loader);
		}
		return JOB_DONE;
	}
	
	if (loader->rdLeft) {
		long n = inet_recv (loader->rdSocket, loader->rdDest, loader->rdLeft);
		int  r = JOB_AGED;
		
		if (n < 0) { /* no more data available */
			inet_close (loader->rdSocket);
			loader->rdSocket = -1;
			loader->rdLeft   = 0;
		
		} else if (n) {
			loader->rdDest   += n;
			loader->rdLeft   -= n;
			loader->DataFill += n;
		
		} else {
			r = JOB_NOOP;
		}
		if (loader->rdLeft) {
			return r;  /* re-schedule for next block of data */
		}
		
		if (!loader->DataSize) {
			if (chunked_job (arg, 0)) {
				return JOB_DONE;  /* chunk head need to be completed */
			
			} else if (loader->rdLeft) {
				return r;  /* re-schedule for start of next chunk */
			}
		}
	}
	/* else download finished */
	
	if (loader->rdSocket >= 0) {
		inet_close (loader->rdSocket);
		loader->rdSocket   = -1;
	}
	if (loader->Data) {
		const char * ext = mime_toExtension (loader->MimeType);
		char * p = loader->Data + loader->DataFill;
		*(p++) = '\0';
		*(p++) = '\0';
		*(p)   = '\0';
		
		loader->Cached = cache_assign (loader->Location, loader->Data,
		                               loader->DataSize,
		                               (ext && *ext ? ext : loader->FileExt),
		                               loader->Date, loader->Expires);
		if (loader->Cached) {
			cache_bound (loader->Location, NULL);
		}
	}
	if (loader->SuccJob) {
		sched_insert (loader->SuccJob, loader,(long)loader->Target, PRIO_SUCCESS);
	} else {
		start_parser (loader);
	}
	return JOB_DONE;
}	
#endif /* USE_INET */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef USE_INET
static int
header_job (void * arg, long invalidated)
{
	LOADER   loader = arg;
	LOCATION loc    = loader->Location;
	
	const char * host;
	HTTP_HDR     hdr;
	short        sock = -1;
	short        reply;
	UWORD        retry = 0;
	
	if (invalidated) {
		if (loader->SuccJob) {
			(*loader->SuccJob)(arg, invalidated);
		} else {
			delete_loader (&loader);
		}
		return JOB_DONE;
	}
	
	switch (CResultDsk (cache_exclusive (loc))) {
		
		case CR_BUSY:
		/*	printf ("header_job(%s): cache busy\n", loc->FullName);*/
			return JOB_KEEP;
		
		case CR_LOCAL: {
			CACHED cached = cache_lookup (loc, 0, NULL);
			if (cached) {
				union { CACHED c; LOCATION l; } u;
				u.c            = cache_bound (cached, &loader->Location);
				loader->Cached = u.l;
				if (!loader->MimeType) {
					loader->MimeType = mime_byExtension (loader->Cached->File,
					                                     NULL, NULL);
				}
			/*	printf ("header_job(%s): cache hit\n", loc->FullName);*/
				if (loader->SuccJob) {
					sched_insert (loader->SuccJob,
					              loader, (long)loader->Target, PRIO_SUCCESS);
				} else {
					sched_insert (loader_job,
					              loader, (long)loader->Target, PRIO_LOADFILE);
				}
				return JOB_DONE;
			
			} else { /* cache inconsistance error */
				printf ("header_job(%s): not in cache!\n", loc->FullName);
				return header_job (arg, (long)loader->Target); /* invalidate */
			}
		} /*break;*/
	}	
	
	/* Connect to host
	*/
	if ((host = location_Host (loc)) != NULL) {
		char buf[300];
		sprintf (buf, "Connecting: %.*s", (int)(sizeof(buf) -13), host);
		containr_notify (loader->Target, HW_SetInfo, buf);
	}
	do {
		reply = http_header (loc, &hdr, sizeof (loader->rdTemp) -2, &sock,
		                     (loader->SuccJob ? 2000l : 5000l));
	} while (reply == -ECONNRESET && retry++ < 1);
	
	/* Check for HTTP header redirect
	*/
	if ((reply == 301 || reply == 302) && hdr.Rdir) {
		LOCATION redir  = new_location (hdr.Rdir, loader->Location);
		CACHED   cached = cache_lookup (redir, 0, NULL);
		inet_close  (sock);
		cache_abort (loc);
		
		if (!loader->MimeType) {
			loader->MimeType = mime_byExtension (redir->File, NULL, NULL);
		}
		if (cached) {
			union { CACHED c; LOCATION l; } u;
	 		if (loader->Cached) {
 				cache_release ((CACHED*)&loader->Cached, FALSE);
 			}
			u.c = cache_bound (cached, &loader->Location);
			loader->Cached = loc = u.l;
			free_location (&redir);
			if (loader->SuccJob) {
				sched_insert (loader->SuccJob,
				              loader, (long)loader->Target, PRIO_SUCCESS);
			} else {
				sched_insert (loader_job,
				              loader, (long)loader->Target, PRIO_LOADFILE);
			}
			return JOB_DONE;
		
		} else {
			free_location (&loader->Location);
			loader->Location = loc = redir;
			return JOB_KEEP; /* re-schedule with the new location */
		}
	}
	
	/* start loading
	*/
	if (hdr.MimeType) {
		if (MIME_Major (hdr.MimeType) && MIME_Minor (hdr.MimeType)
		    && (MIME_Major (loader->MimeType) != MIME_Major (hdr.MimeType)
			     || !MIME_Minor (loader->MimeType))) {
			loader->MimeType = hdr.MimeType;
		}
		if (hdr.Encoding) {
			loader->Encoding = hdr.Encoding;
		}
	}
	if (reply == 200 /*&& MIME_Major(loader->MimeType) == MIME_TEXT*/) {
		char buf[300];
		sprintf (buf, "Receiving from %.*s", (int)(sizeof(buf) -16), host);
		containr_notify (loader->Target, HW_SetInfo, buf);
		
		if (hdr.Modified > 0) {
			loader->Date = hdr.Modified;
		} else if (hdr.SrvrDate > 0) {
			loader->Date = hdr.SrvrDate;
		}
		if (loader->Date && hdr.Expires > 0) {
			loader->Expires = hdr.Expires;
		}
		
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
				/* nothing more to do, falls through */
			} else {
				loader->rdSocket = sock;
				sched_insert (receive_job,
				              loader, (long)loader->Target, PRIO_RECIVE);
				return JOB_DONE;
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
			sched_insert (chunked_job, loader, (long)loader->Target, PRIO_RECIVE);
			return JOB_DONE;
		}
	
	} else { /* something went wrong */
		loader->Error = reply;
	}
	
	/* else either a very short file or an error case occured
	*/
	inet_close  (sock);
	cache_abort (loc);
	
	if (!loader->Data) {
		loader->MimeType = MIME_TEXT;
		loader->Data     = strdup (hdr.Head);
	}
	if (loader->SuccJob) {
		(*loader->SuccJob)(loader, (long)loader->Target);
	} else {
		start_parser (loader);
	}

	return JOB_DONE;
}
#endif /* USE_INET */


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
loader_job (void * arg, long invalidated)
{
	LOADER   loader = arg;
	LOCATION loc    = (loader->Cached ? loader->Cached : loader->Location);
	
	if (invalidated) {
		delete_loader (&loader);
		return JOB_DONE;
	}
	
	if (!loader->notified) {
		char buf[1024];
		location_FullName (loader->Location, buf, sizeof(buf));
		loader->notified = containr_notify (loader->Target, HW_PageStarted, buf);
	}
	
	loader->Data = load_file (loc, &loader->DataSize, &loader->DataFill);
	if (!loader->Data) {
		loader->Error    = -ENOENT;
		loader->Data     = strdup ("<html><head><title>Error</title></head>"
		                           "<body><h1>Page not found!</h1></body></html>");
		loader->MimeType = MIME_TXT_HTML;
	}
	/* registers a parser job with the scheduler */
	start_parser (loader);

	return JOB_DONE;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
generic_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	
	if (!invalidated) {
		LOCATION loc = (loader->Cached ? loader->Cached : loader->Location);
		
		if (!loader->ExtAppl) {
			printf("generic_job(): no appl found!\n");
		
		} else if (PROTO_isRemote (loc->Proto)) {
			printf("generic_job(): not in cache!\n");
		
		} else {
			start_application (loader->ExtAppl, loc);
		}
	}
	delete_loader (&loader);
	
	return JOB_DONE;
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

/*----------------------------------------------------------------------------*/
static short
start_application (const char * appl, LOCATION loc)
{
	short id   = (appl ? find_application (appl) : -1);
	if (id >= 0 || (id = find_application (NULL)) >= 0) {
		short msg[8] = { VA_START, };
		msg[1] = gl_apid;
		msg[2] = msg[5] = msg[6] = msg[7] = 0;
		*(char**)(msg +3) = va_helpbuf;
		location_FullName (loc, va_helpbuf, HW_PATH_MAX *2);
		appl_write (id, 16, msg);
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


/******************************************************************************/


/*============================================================================*/
char *
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
