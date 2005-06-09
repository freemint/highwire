/* @(#)highwire/Loader.c
 *
 * Currently has ended up a junk file of initializations
 * loading routine and some assorted other routines
 * for file handling.
*/
#ifdef __PUREC__
# include <tos.h>
# include <ext.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <gem.h>

#include "file_sys.h"
#include "global.h"
#include "hwWind.h"
#include "av_comm.h"
#include "schedule.h"
#include "Containr.h"
#include "Location.h"
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
	BOOL       chk0 = FALSE;
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
				chk0 = TRUE;
			
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
			while (strncmp(p, "<!--", 4) == 0 || strncmp(p, "<?xml", 5) == 0) {
				long i = min(500, (long)(loader->Data - p) + loader->DataSize - 15);
				while (--i > 0 && *(p++) != '>');
				while (--i > 0 && isspace(*p)) p++;
			}
			if (strnicmp (p, "<html>",          6) == 0 ||
			    strnicmp (p, "<!DOCTYPE HTML", 14) == 0) {
				func = parse_html;
				chk0 = TRUE;
				break;
			}
		}
		default: /* pretend it's plain text */ ;
	}
	
	if (chk0) { /* check for invalid nul characters in html */
		char * p = loader->Data, * q;
		size_t n = loader->DataSize;
		while (n > 0l && (q = memchr (p, '\0', n)) != NULL) {
			*q = ' ';
			n -= q - p;
			p  = q;
		}
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
new_loader (LOCATION loc, CONTAINR target, BOOL lookup)
{
	const char * appl = NULL;
	LOADER loader = malloc (sizeof (struct s_loader));
	loader->Location   = location_share (loc);
	loader->Target     = target;
	loader->Encoding   = ENCODING_WINDOWS1252;
	loader->MimeType   = MIME_Unknown;
	loader->Referer    = NULL;
	loader->AuthRealm  = NULL;
	loader->AuthBasic  = NULL;
	loader->PostBuf    = NULL;
	loader->FileExt[0] = '\0';
	loader->MarginW    = -1;
	loader->MarginH    = -1;
	loader->ScrollV    = 0;
	loader->ScrollH    = 0;
	/* */
	loader->Cached   = NULL;
	loader->Tdiff    = 0;
	loader->Date     = 0;
	loader->Expires  = 0;
	loader->DataSize = 0;
	loader->DataFill = 0;
	loader->Data     = NULL;
	loader->notified = FALSE;
	loader->Retry    = cfg_ConnRetry;
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
	
	if (loc->Proto == PROT_HTTP && lookup) {
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
		if (loader->PostBuf) {
			free (loader->PostBuf);
		}
		if (loader->AuthRealm) {
			free (loader->AuthRealm);
		}
		if (loader->AuthBasic) {
			free (loader->AuthBasic);
		}
		free_location (&loader->Referer);
		free_location (&loader->Location);
		free (loader);
		*p_loader = NULL;
	}
}


/*============================================================================*/
LOADER
start_page_load (CONTAINR target, const char * url, LOCATION base,
                 BOOL u_act, char * post_buff)
{
	LOCATION loc    = (url ? new_location (url, base) : location_share (base));
	LOADER   loader = NULL;
	
	if (loc->Proto == PROT_MAILTO || loc->Proto == PROT_FTP) {
		start_application (NULL, loc);
	
	} else {
		loader = start_cont_load (target, NULL, loc, u_act, (post_buff == NULL));
		if (loader) {
			loader->PostBuf = post_buff;
		} else {
			free (post_buff);
		}
	}
	free_location (&loc);
	
	return loader;
}

/*============================================================================*/
LOADER
start_cont_load (CONTAINR target, const char * url, LOCATION base,
                 BOOL u_act, BOOL use_cache)
{
	LOCATION loc    = (url ? new_location (url, base) : location_share (base));
	LOADER   loader = new_loader (loc, target, use_cache);
	
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
# ifdef MOD_MBOX
	} else if (loc->Proto == PROT_POP) {
		int parse_mbox (void*, long);
		sched_insert (parse_mbox, new_parser (loader), (long)target, PRIO_INTERN);
	
# endif
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
	BOOL hdr_only = (!target && successor);
	LOADER loader = new_loader (loc, target, !hdr_only);
	
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
		sched_insert (header_job, loader, (long)target,
		              (hdr_only ? PRIO_USERACT : PRIO_TRIVIAL));
#endif
	
	} else {
		printf ("start_objc_load() invalid protocol %i.\n", loc->Proto);
		loader->Error = -EPROTONOSUPPORT;
		(*loader->SuccJob)(loader, 0);
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
		if (!loader->PostBuf) {
			cache_abort (loader->Location);
		}
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
		char * p = loader->Data + loader->DataFill;
		*(p++) = '\0';
		*(p++) = '\0';
		*(p)   = '\0';
		
		if (!loader->PostBuf || MIME_Major (loader->MimeType) != MIME_TEXT) {
			const char * ext = mime_toExtension (loader->MimeType);
			if (loader->PostBuf) {
				loader->Expires = -1; /* mark to get deleted at program end */
			}
			loader->Cached = cache_assign (loader->Location, loader->Data,
			                               loader->DataSize,
			                               (ext && *ext ? ext : loader->FileExt),
			                               loader->Date, loader->Expires);
			if (loader->Cached) {
				cache_bound (loader->Location, NULL);
			}
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
	
	const char * host = NULL;
	HTTP_HDR     hdr;
	short        sock = -1;
	short        reply;
	char       * auth = NULL;
	
	BOOL hdr_only = (!loader->Target && loader->SuccJob);
	BOOL cache_lk = (!loader->PostBuf && !hdr_only);
#define CACHE_ABORT(loc)   { if (cache_lk) cache_abort (loc); }
	
	if (invalidated) {
		if (loader->SuccJob) {
			(*loader->SuccJob)(arg, invalidated);
		} else {
			delete_loader (&loader);
		}
		return JOB_DONE;
	}
	
	if (cache_lk) switch (CResultDsk (cache_exclusive (loc))) {
		
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
	if (loader->Target) {
		if ((host = location_Host (loc, NULL)) != NULL && !*host) host = NULL;
	}
	if (host) {
		char buf[300];
		sprintf (buf, "Connecting: %.*s", (int)(sizeof(buf) -13), host);
		containr_notify (loader->Target, HW_SetInfo, buf);
	}
	while(1) { /* authorization loop */
		do {
			long tout = (loader->SuccJob ? hdr_tout_gfx : hdr_tout_doc);
			reply = http_header (loc, &hdr, sizeof (loader->rdTemp) -2,
			                     &sock, tout, loader->Referer, auth, loader->PostBuf);
		} while (reply == 100);
		
		if (reply == -ECONNRESET || reply == -ETIMEDOUT) {
			if (loader->Retry) {
				CACHE_ABORT(loc);
				loader->Retry--;
				return JOB_KEEP; /* try connecting later */
			}
		}
		if (reply == -35/*EMFILE*/) {
			CACHE_ABORT(loc);
			return JOB_KEEP; /* too many open connections yet, try again later */
		}
		
		if (reply == 401) {
			if (!auth && hdr.Realm && loader->AuthBasic && loader->AuthRealm
			          && strcmp (loader->AuthRealm, hdr.Realm) == 0) {
				inet_close (sock);
				sock = -1;
				auth = loader->AuthBasic;
				continue;
			}
			if (loader->AuthRealm) {
				free (loader->AuthRealm);
				loader->AuthRealm = NULL;
			}
			if (loader->AuthBasic) {
				free (loader->AuthBasic);
				loader->AuthBasic = NULL;
			}
			if (hdr.Realm && !loader->SuccJob && !hdr_only) {
				const char form[] =
					"<html><head><title>%.*s</title></head>"
					"<body bgcolor=\"white\">"
					"<h1>Authorization Required</h1>&nbsp;<br>"
					"Enter user name and password for <q><b>%s</b></q> at <b>%s</b>:"
					"<form method=\"AUTH\">"
					"<table border=\"0\">"
					"<tr><td align=right>User:&nbsp;"
					    "<td><input type=\"text\" name=\"USR\">"
					"<tr><td align=right>Password:&nbsp;"
					    "<td><input type=\"password\" name=\"PWD\">"
					"<tr><td align=right>"
					      "<input type=\"submit\" value=\"Login\">&nbsp;"
					"</table></form>&nbsp;<p><hr size=\"1\">"
					"\n<small><pre>%s</pre></small>\n"
					"</bod></html>";
				const char * titl = hdr.Head, * text;
				size_t       titl_s, size;
				while (*titl && !isspace(*titl)) titl++;
				while (isspace(*titl))           titl++;
				text = titl;
				while (*text && *text != '\r' && *text != '\n') text++;
				titl_s = text - titl;
				while (isspace(*text)) text++;
				size = sizeof(form)
				     + titl_s + strlen(hdr.Realm) + strlen(host) + strlen (text);
				if ((loader->AuthRealm = strdup (hdr.Realm)) != NULL &&
				    (loader->Data = malloc (size)) != NULL) {
					inet_close (sock);
					sock = -1;
					CACHE_ABORT(loc);
					size = sprintf (loader->Data, form,
					                (int)titl_s, titl, hdr.Realm, host, text);
					loader->Data[++size] = '\0';
					loader->Data[++size] = '\0';
					loader->MimeType = MIME_TXT_HTML;
					start_parser (loader);
					return JOB_DONE;
				}
			}
		}
		break;
	}
	/* Check for HTTP header redirect
	*/
	if ((reply == 301 || reply == 302 || reply == 303) && hdr.Rdir) {
		LOCATION redir  = new_location (hdr.Rdir, loader->Location);
		CACHED   cached = (hdr_only ? NULL : cache_lookup (redir, 0, NULL));
		inet_close  (sock);
		CACHE_ABORT(loc);
		
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
	if (reply == 200) {
		if (host) {
			char buf[300];
			sprintf (buf, "Receiving from %.*s", (int)(sizeof(buf) -16), host);
			containr_notify (loader->Target, HW_SetInfo, buf);
		}
		loader->Date  = (hdr.Modified > 0 ? hdr.Modified : hdr.SrvrDate);
		loader->Tdiff = hdr.LoclDate - hdr.SrvrDate;
		if (hdr.Expires > 0) {
			loader->Expires = hdr.Expires + loader->Tdiff;
		} else if (hdr.Modified <= 0) {
			/* none valid date given at all, so we assume a dynamic page
			 * (eg. from php) that needs to be revisited at the next session */
			loader->Expires = -1; /* mark to get deleted at program end */
		}
		
		if (hdr_only) {
			loader->DataSize = hdr.Size;
			if (hdr.Tlen) {
				memcpy (loader->rdTemp, hdr.Tail, hdr.Tlen);
				loader->rdTlen = hdr.Tlen;
			}
			loader->rdSocket = sock;
			sched_insert (loader->SuccJob, loader, (long)0, PRIO_RECIVE);
			return JOB_DONE;
		
		} else if (hdr.Size >= 0 && !hdr.Chunked) {
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
	
	inet_close (sock);
	
	/* if it is a short file and already finished, end proceedings
	*/
	if (loader->Data) {
		return receive_job (loader, 0);
	}
	
	/*  an error case occured
	*/
	CACHE_ABORT(loc);
	
	loader->MimeType = MIME_TEXT;
	loader->Data     = strdup (hdr.Head);
	
	if (loader->SuccJob) {
		(*loader->SuccJob)(loader, 0);
	} else {
		start_parser (loader);
	}
	
#undef CACHE_ABORT(loc)
	
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
	
	if (!invalidated && !loader->Error) {
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

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int
saveas_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	char fsel_file[HW_PATH_MAX] = "";
	WORD r, butt;  /* file selector exit button */
	int    fh1, fh2;
	long fsize, bsize, rsize, csize = 0, nsize, ssize;
	char *buffer;
	char *p;
	HwWIND wind = hwWind_byContainr (loader->Target);
				
	if (!invalidated && !loader->Error) {
		LOCATION loc = (loader->Cached ? loader->Cached : loader->Location);
		LOCATION remote = loader->Location;

		if (PROTO_isRemote (loc->Proto)) {
			printf("saveas_job(): not in cache!\n");
		
		} else {
			/* get cache file size */
			fsize =	file_size(loc);

			if (fsize <= 0)
			{
				printf("saveas_job(): file empty skipping\r\n");
				goto saveas_bottom;
			}

			/* remote->file is the remote filename, with the possibility
			 * of extra characters
			 * ssize is the start size of the remote file
			 * nsize is the size of everything after a ?
			 */
	
			p = strrchr (remote->File, '?');

			nsize = (p ? strlen(p) : 0);
			ssize = strlen(remote->File);
			ssize -= nsize;
			if (ssize >= sizeof (fsel_file)) {
				ssize = sizeof (fsel_file) -1;
			}
			memcpy (fsel_file, remote->File, ssize);
			fsel_file[ssize] = '\0';
			
			/* get our new filename */
			
			if ((gl_ap_version >= 0x140 && gl_ap_version < 0x200)
			    || gl_ap_version >= 0x300 /* || getcookie(FSEL) */) {
				r = fsel_exinput (fsel_path, fsel_file, &butt,
			                  "HighWire: Save File as...");
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

					if ((fh2 = open (file, O_RDWR|O_CREAT|O_TRUNC, 0666)) >= 0) {
						/* get our cache file name */
						location_FullName (loc, va_helpbuf, HW_PATH_MAX *2);

						/* attempt to open cache file */
						fh1 = open (va_helpbuf, O_RDONLY);
						
						if (fh1 < 0) {
								printf("saveas_job(): file not found in cache\r\n");
								close (fh2);
								goto saveas_bottom;
							} 

						/* max 32k buffer to read/write */
						if (fsize < 32000)
							bsize = fsize;
						else
							bsize = 32000;
											
						buffer = malloc(bsize);

						if (!buffer) {
							close (fh1);
							close (fh2);
							printf("saveas_job(): memory malloc error\r\n");
							goto saveas_bottom;
						}
															
						while (csize < fsize)
						{
							rsize = read (fh1, buffer, bsize);
							write (fh2, buffer, rsize);
							csize += rsize;
						}
		
						/* done so close our files */
						close (fh1);
						close (fh2);

						/* and release our buffer */
						free(buffer);
						
					} else
						printf("File creation error\r\n");
						
				}
				/* We don't worry about the else as all is done above */
			}
		}
	}

saveas_bottom:
	
	delete_loader (&loader);

	/* We should close the window here	 */
	if (wind) {
		delete_hwWind (wind);
	}

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
/* Get the size of a file from the cache, predominately */

long
file_size (const LOCATION loc)
{
	const char *filename = loc->FullName;
	long         size = 0;
	struct xattr file_info;
	long         xret = F_xattr(0, filename, &file_info);
	
	if (xret == E_OK) {  /* Fxattr() exists */
		size = file_info.size;
	
	} else if (xret == -EINVFN) {  /* here for TOS filenames */
		DTA  new, * old = Fgetdta();
		Fsetdta(&new);

		if (Fsfirst(filename, 0) == E_OK) {
			size = new.d_length;
		}
		Fsetdta(old);
	
	} else {
		size = -1;
	}
	
	return(size);
}


/*============================================================================*/
char *
load_file (const LOCATION loc, size_t * expected, size_t * loaded)
{
	long   size = file_size (loc);
	char * file = NULL;
	
	*expected = size;
	
	if ((file = malloc (size + 3)) == NULL) {
		size = 0;
	
	} else if (size >= 0) {
		const char * filename = loc->FullName;
		int          fh       = open (filename, O_RDONLY);
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

	temp_location[0] = Dgetdrv();
	temp_location[0] += (temp_location[0] < 26 ? 'A' : -26 + '1');
	temp_location[1] = ':';
	Dgetpath (temp_location + 2, 0);

	if (temp_location[strlen(temp_location) - 1] != '\\') {
		strcat(temp_location,"\\");
	}
	strcpy(fsel_path, temp_location);  /* used by the first file selector call */
	strcat(fsel_path, "html\\*.HTM*");  /* uppercase is for the TOS file selector */

	strcpy(help_file, temp_location);  /* used for keyboard F1 or Help */
	strcat(help_file, "html\\help\\index.htm");
}
