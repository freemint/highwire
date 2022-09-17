/* @(#)highwire/dragdrop.c
 */

/*
 * Drag & drop sample code.
 * Copyright 1992 Atari Corporation
 *
 * With some change for Highwire
 *
 * global variables used:
 * gl_apid: our AES application id
 *
 * BUGS/CAVEATS:
 * This code is not re-entrant (it uses a static
 * variable for the pipe name and for saving the
 * SIGPIPE signal handler).
 *
 * While doing the drag and drop, the SIGPIPE
 * signal (write on an empty pipe) is ignored
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define __TOS
#include <gem.h>
#undef  __TOS

#include <cflib.h>
#include "file_sys.h"
#include "global.h"
#include "Loader.h"
#include "hwWind.h"
#include "dragdrop.h"

#ifndef EACCDN
#define EACCDN -36
#endif

#ifndef SIGPIPE
#define SIGPIPE 13
#endif

#ifndef SIG_IGN
#define SIG_IGN 1L
#endif

#ifndef WF_OWNER
#define WF_OWNER 20
#endif

#ifndef FA_HIDDEN
#define FA_HIDDEN 0x2
#endif

static char pipename[] = "U:\\PIPE\\DRAGDROP.AA";
static long oldpipesig;

#define DEBUGGING

#ifdef DEBUGGING
#define debug_alert(x, y) form_alert(x, y)
#else
#define debug_alert(x, y)
#endif


/*------------------------------------------------------------------------------
 * parseargs (char cmdlin[]):
 *	Given a null terminated string of arguments, separated
 *	by spaces, parse it and perform whatever actions are
 *	appropriate on the arguments (usually file and folder
 *	names)
 *
 * Return:
 * - The numbers of arguments.
 * - The str contains the string separated by null
*/

static WORD
parseargs (char * str)
{
	WORD	 cnt      = 1;
	char * c        = str;
	BOOL	 in_quote = FALSE;

	while (*c) {
		switch (*c) {
			
			case ' ' :
				if (!in_quote) {
					*c = '\0';
					cnt++;
				}
				break;
			
			case '\'' :
				strcpy (c, c + 1);
				if (!in_quote ) {
					in_quote = TRUE;
				} else if (*c != '\'') {
					in_quote = FALSE;
					*c = 0;
					if (c[1]) {
						cnt++;
					}
				}
				break;
			
			default:
				break;
		}
		c++;
	}
	return cnt;
}


/* Code for originator*/


/*------------------------------------------------------------------------------
 *
 * create a pipe for doing the drag & drop,
 * and send an AES message to the recipient
 * application telling it about the drag & drop
 * operation.
 *
 * Input Parameters:
 * apid:	AES id of the window owner
 * winid:	target window (0 for background)
 * msx, msy:	mouse X and Y position
 *		(or -1, -1 if a fake drag & drop)
 * kstate:	shift key state at time of event
 *
 * Output Parameters:
 * exts:	A 32 byte buffer into which the
 *		receipient's 8 favorite
 *		extensions will be copied.
 *
 * Returns:
 * A positive file descriptor (of the opened
 * drag & drop pipe) on success.
 * -1 if the receipient doesn't respond or
 *    returns DD_NAK
 * -2 if appl_write fails
 */

static int
ddcreate (int apid, int winid, int msx, int msy, int kstate, char exts[])
{
	int  fd;
	WORD msg[8];
	long i;
	long fdmask;
	char c;

	pipename[17] = pipename[18] = 'A';
	fd = -1;
	do {
		pipename[18]++;
		if (pipename[18] > 'Z') {
			pipename[17]++;
			if (pipename[17] > 'Z') {
				break;
			}
		}
		/* FA_HIDDEN means "get EOF if nobody has pipe open for reading" */
		fd = (int) Fcreate (pipename, FA_HIDDEN);
	} while (fd == EACCDN);

	if (fd < 0) {
		debug_alert (1, "[1][Fcreate error][OK]");
		return fd;
	}

	/* construct and send the AES message */
	msg[0] = AP_DRAGDROP;
	msg[1] = gl_apid;
	msg[2] = 0;
	msg[3] = winid;
	msg[4] = msx;
	msg[5] = msy;
	msg[6] = kstate;
	msg[7] = (pipename[17] << 8) | pipename[18];
	i = appl_write (apid, 16, msg);
	if (i == 0) {
		debug_alert (1, "[1][appl_write error][OK]");
		Fclose (fd);
		return -2;
	}
	
	/* now wait for a response */
	fdmask = 1L << fd;
	i = Fselect (DD_TIMEOUT, &fdmask, 0L, 0L);
	if (!i || !fdmask) {	/* timeout happened */
		debug_alert (1, "[1][ddcreate: Fselect timeout][OK]");
		Fclose (fd);
		return -1;
	}

	/* read the 1 byte response */
	i = Fread (fd, 1L, &c);
	if (i != 1 || c != DD_OK) {
		if (i != 1) debug_alert (1, "[1][ddcreate: read error][OK]");
		else        debug_alert (1, "[1][ddcreate: DD_NAK][OK]");
		Fclose (fd);
		return -1;
	}

	/* now read the "preferred extensions" */
	i = Fread (fd, DD_EXTSIZE, exts);
	if (i != DD_EXTSIZE) {
		debug_alert (1, "[1][Error reading extensions][OK]");
		Fclose (fd);
		return -1;
	}

	oldpipesig = Psignal (SIGPIPE, (LONG)SIG_IGN);
	return fd;
}

/*------------------------------------------------------------------------------
 * see if the recipient is willing to accept a certain
 * type of data (as indicated by "ext")
 *
 * Input parameters:
 * fd		file descriptor returned from ddcreate()
 * ext		pointer to the 4 byte file type
 * name		pointer to the name of the data
 * size		number of bytes of data that will be sent
 *
 * Output parameters: none
 *
 * Returns:
 * DD_OK	if the receiver will accept the data
 * DD_EXT	if the receiver doesn't like the data type
 * DD_LEN	if the receiver doesn't like the data size
 * DD_NAK	if the receiver aborts
 */

static int
ddstry(int fd, char *ext, char *name, long size)
{
	WORD hdrlen, i;
	char c;

	/* 4 bytes for extension, 4 bytes for size, 1 byte for
	 * trailing 0
	 */
	hdrlen = 9 + strlen(name);
	i = Fwrite (fd, 2L, &hdrlen);

	/* now send the header */
	if (i != 2) return DD_NAK;
	i =  Fwrite (fd, 4L, ext);
	i += Fwrite (fd, 4L, &size);
	i += Fwrite (fd, (long)strlen(name)+1, name);
	if (i != hdrlen) return DD_NAK;

	/* wait for a reply */
	i = Fread(fd, 1L, &c);
	return (i != 1 ? DD_NAK : c);
}


/* Code for either recipient or originator */


/*------------------------------------------------------------------------------
 * close a drag & drop operation
 */

static void
ddclose (int fd)
{
	(void)Psignal (SIGPIPE, oldpipesig);
	(void)Fclose (fd);
}



/* Code for recipient */


/*------------------------------------------------------------------------------
 * open a drag & drop pipe
 *
 * Input Parameters:
 * ddnam: the pipe's name (from the last word of
 *        the AES message)
 * preferext: a list of DD_NUMEXTS 4 byte extensions we understand
 *            these should be listed in order of preference
 *            if we like fewer than DD_NUMEXTS extensions, the
 *            list should be padded with 0s
 *
 * Output Parameters: none
 *
 * Returns:
 * A (positive) file handle for the drag & drop pipe, on success
 * -1 if the drag & drop is aborted
 * A negative error number if an error occurs while opening the
 * pipe.
 */

static int
ddopen (int ddnam, char *preferext)
{
	int fd;
	char outbuf[DD_EXTSIZE+1];

	pipename[18] = ddnam & 0x00ff;
	pipename[17] = (ddnam & 0xff00) >> 8;

	fd = (int) Fopen(pipename, 2);
	if (fd < 0) return fd;

	outbuf[0] = DD_OK;
	strncpy (outbuf+1, preferext, DD_EXTSIZE);

	oldpipesig = Psignal (SIGPIPE, (LONG)SIG_IGN);

	if (Fwrite (fd, (long)DD_EXTSIZE+1, outbuf) != DD_EXTSIZE+1) {
		ddclose(fd);
		return -1;
	}

	return fd;
}

/*------------------------------------------------------------------------------
 * ddrtry: get the next header from the drag & drop originator
 *
 * Input Parameters:
 * fd:		the pipe handle returned from ddopen()
 *
 * Output Parameters:
 * name:	a pointer to the name of the drag & drop item
 *		(note: this area must be at least DD_NAMEMAX bytes long)
 * whichext:	a pointer to the 4 byte extension
 * size:	a pointer to the size of the data
 *
 * Returns:
 * 0 on success
 * -1 if the originator aborts the transfer
 *
 * Note: it is the caller's responsibility to actually
 * send the DD_OK byte to start the transfer, or to
 * send a DD_NAK, DD_EXT, or DD_LEN reply with ddreply().
 */

static int
ddrtry (int fd, char *name, char *whichext, long *size)
{
	WORD hdrlen;
	int i;
	char buf[80];

	i = (int) Fread (fd, 2L, &hdrlen);
	if (i != 2) {
		return -1;
	}
	if (hdrlen < 9) {	/* this should never happen */
		return -1;
	}
	i = (int) Fread (fd, 4L, whichext);
	if (i != 4) {
		return -1;
	}
	whichext[4] = 0;
	i = (int) Fread (fd, 4L, size);
	if (i != 4) {
		return -1;
	}
	hdrlen -= 8;
	if (hdrlen > DD_NAMEMAX) {
		i = DD_NAMEMAX;
	} else {
		i = hdrlen;
	}
	if (Fread (fd, (long)i, name) != i) {
		return -1;
	}
	hdrlen -= i;

	/* skip any extra header */
	while (hdrlen > 80) {
		Fread (fd, 80L, buf);
		hdrlen -= 80;
	}
	if (hdrlen > 0) {
		Fread(fd, (long)hdrlen, buf);
	}

	return 0;
}

/*------------------------------------------------------------------------------
 * send a 1 byte reply to the drag & drop originator
 *
 * Input Parameters:
 * fd:		file handle returned from ddopen()
 * ack:		byte to send (e.g. DD_OK)
 *
 * Output Parameters:
 * none
 *
 * Returns: 0 on success, -1 on failure
 * in the latter case the file descriptor is closed
 */

static int
ddreply (int fd, int ack)
{
	char c = ack;

	if (Fwrite (fd, 1L, &c) != 1L) {
		Fclose (fd);
	}
	return 0;
}

/******************************************************************
 *	Here are some sample functions that use the		  *
 *	drag & drop functions above to implement the protocol.	  *
 *	These are not complete, working functions yet, but are	  *
 *	intended to serve as a "skeleton" for the actual	  *
 *	functions that would appear in a drag & drop application. *
 *								  *
 *	The following global variables are defined:		  *
 *	gl_ourexts: the drag & drop data types we understand	  *
 ******************************************************************/

/* modify this as necessary */
char ourexts[DD_EXTSIZE] = ".TXT";

/*------------------------------------------------------------------------------
 * rec_ddmsg: given a drag & drop message, act as
 * a recipient and get the data
 *
 * Input Parameters:
 * msg:		Pointer to the 16 byte AES message
 *		returned by evnt_multi or evnt_mesag
 *
 * calls the following functions to actually perform the paste
 * operation:
 *
 *
 * paste_rtf(int win, int fd, long size):
 *	Read "size" bytes from the open file descriptor "fd",
 *	and paste it into the window whose handle is "win".
 *	The data is assumed to be in RTF format.
 *
 * paste_txt(int win, int fd, long size):
 *	Read "size" bytes from the open file descriptor "fd",
 *	and paste it into the window whose handle is "win".
 *	The data is assumed to be ASCII text.
 */

void
rec_ddmsg (WORD msg[8])
{
	int    i;
	char   txtname[DD_NAMEMAX], ext[5];
	char * cmdline, * s;
	long   size;

	int winid  = msg[3];
/*	int msx    = msg[4];*/
/*	int msy    = msg[5];*/
	int kstate = msg[6];
	int pnam   = msg[7];

	int fd = ddopen (pnam, ourexts);
	if (fd < 0) return;

	for(;;) {
 		HwWIND wind;
		
		i = ddrtry (fd, txtname, ext, &size);
		if (i < 0) {
			ddclose(fd);
			return;
		}
		if (strncmp (ext, "ARGS", 4) == 0) {
			WORD cnt;
			
			cmdline = malloc((size_t)size+1);
			if (!cmdline) {
				ddreply (fd, DD_LEN);
				continue;
			}
			ddreply (fd, DD_OK);
			Fread (fd, size, cmdline);
			ddclose (fd);
			cmdline[size] = 0;
			cnt = parseargs (cmdline);
			if (cnt > 0) {
				if ((kstate & K_ALT)
				    || ((wind = hwWind_byHandle (winid)) != NULL
				         && wind->Base.Ident != WIDENT_BRWS)) {
					wind = NULL;
				}
				if (!wind) new_hwWind      ("", cmdline, TRUE);
				else       start_cont_load (wind->Pane, cmdline, NULL, TRUE, TRUE);
			}
			free (cmdline);
			return;
		
		} else if (strncmp (ext, ".TXT", 4) == 0) {
			if (size == 0) {
				ddreply(fd, DD_LEN);
				continue;
			}
			cmdline = malloc((size_t)size+1);
			if (!cmdline) {
				ddreply(fd, DD_LEN);
				continue;
			}
			ddreply (fd, DD_OK);
			Fread (fd, size, cmdline);
			ddclose (fd);
			cmdline[size] = 0;
			s = strchr (cmdline, '\r');
			if (s != NULL) {
				*s = '\0';
			}
			if ((kstate & K_ALT)
			    || ((wind = hwWind_byHandle (winid)) != NULL
			         && wind->Base.Ident != WIDENT_BRWS)) {
				wind = NULL;
			}
			if (!wind) new_hwWind      ("", cmdline, TRUE);
			else       start_cont_load (wind->Pane, cmdline, NULL, TRUE, TRUE);
			free (cmdline);
			return;
		}
		ddreply(fd, DD_EXT);
#ifdef DEBUGGING
		{ char foo[40];
		strcpy(foo, "[1][rec_ddmsg: unknown extension: |");
		strcat(foo, ext);
		strcat(foo, "][OK]");
		debug_alert(1, foo);
		}
#endif /* DEBUGGING */
	}
}

/*------------------------------------------------------------------------------
 * send_ddmsg: construct and send a drag & drop message;
 * we only can send the 1 type of data (specified
 * by ext)
 *
 * Input Parameters:
 * msx, msy:	coordinates where data was dropped
 * kstate:	keyboard state at time of drop
 * name:	name of the data
 * ext:		data type (4 byte extension)
 * size:	size of data
 * data:	pointer to data
 *
 * Output Parameters: none
 *
 * Returns:
 * 0 on success
 * 1 if no window was found
 * 2 if the window belongs to us
 * 3 if the receipient didn't accept the data
 * or a negative error code from the functions
 * above
 */

int
send_ddmsg (WORD msx, WORD msy, WORD kstate,
            char * name, char * ext, long size, char * data)
{
	int   fd;
	short apid, winid;
	short dummy, i;
	char  recexts[DD_EXTSIZE];

	winid = wind_find (msx, msy);
/*
	if (!winid) {
		debug_alert(1, "[1][Window not found][OK]");
		return 1;
	}
*/
	if (!wind_get(winid, WF_OWNER, &apid, &dummy, &dummy, &dummy)) {
		debug_alert(1, "[1][Owner not found][OK]");
		return 2;
	}

	if (apid == gl_apid) {
		HwWIND wind = hwWind_byHandle (winid);
		if (wind && wind->Base.Ident != WIDENT_BRWS) {
			wind = NULL;
		}
		if (!wind) new_hwWind      ("", data, TRUE);
		else       start_cont_load (wind->Pane, data, NULL, TRUE, TRUE);

/*		debug_alert(1, "[1][Same owner][OK]"); */
		return 1;	/* huh? that shouldn't happen */
	}

	fd = ddcreate (apid, winid, msx, msy, kstate, recexts);
	if (fd < 0) {
		debug_alert (1, "[1][Couldn't open pipe][OK]");
		return fd;
	}
	if ((i = ddstry (fd, ext, name, size)) != DD_OK) {
		if (i == DD_EXT) {
			debug_alert(1, "[1][Bad extension][OK]");
		} else {
			debug_alert(1, "[1][Receiver canceled][OK]");
		}
		ddclose(fd);
		return 3;
	}
	Fwrite (fd, size, data);
	ddclose (fd);
	return 0;
}
