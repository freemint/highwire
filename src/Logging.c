/* @(#)highwire/Logging.c
 *
 * This module provides logging functionality for the HighWire HTML browser.
 * It's usefull to inform the user about rendering time, HTML errors, ...
 * It's possible to extend this to a log file, opend on an user command.
 * Rainer Seitel, 2002-04-15
 */


#include <stdarg.h>
#include <stdio.h>

#include "global.h"
#include "Logging.h"


/* With key F7 the user can switch on logging, keyinput.c.
 */
BOOL logging_is_on = FALSE;

/* Set by LOG_FILE.  The console is the screen on real hardware, so a run that
 * needs reading afterwards -- or that ends in a crash -- has nowhere to go
 * otherwise.  Colour escapes are left out of the file: they are VT52, and in a
 * text file they are just noise.
 */
static FILE * log_file = NULL;

void log_setfile (const char * path)
{
	if (log_file) {
		fclose (log_file);
		log_file = NULL;
	}
	if (path && *path && (log_file = fopen (path, "w")) != NULL) {
		/* unbuffered: the interesting runs are the ones that end in a crash,
		 * and a buffer would take the last lines down with it */
		setvbuf (log_file, NULL, _IONBF, 0);
		logging_is_on = TRUE;
	}
}


void init_logging(void)
{
	if (logging_is_on) {
		fprintf(stdout, "\33H\33v");  /* cursor home, enable line wrap */
	}
}


/* errprintf() is for possible errors in HighWire.
 * Same parameters as printf().
 *
 * VT52 console text colors: 0: white, 1: red, 2: green, 3: yellow, 4: blue,
 * 5: magenta, 6: cyan, 7: light grey, 8: dark grey, 9: dark red,
 * 10':': dark green, 11';': ochre, 12'<': dark blue, 13'=': dark magenta,
 * 14'>': dark cyan, 15'?': black.
 */
void errprintf(const char *s, ...)
{
	va_list arglist;

	/* Without a console the VT52 output would scribble over the GEM screen,
	 * so keep quiet unless the user asked for logging.
	 */
	if (!logging_is_on) return;

	if (log_file) {
		va_start(arglist, s);
		vfprintf(log_file, s, arglist);
		va_end(arglist);
		return;
	}
	if (!ignore_colours)
		fprintf(stderr, "\33c7\33b1HighWire: ");  /* print error line in red */
	else
		fprintf(stderr, "\33pHighWire:\33q ");  /* print "HighWire:" inverse */
	va_start(arglist, s);
	vfprintf(stderr, s, arglist);
	va_end(arglist);
	if (!ignore_colours)
		fprintf(stderr, "\33b?");  /* print in black */
}


/* logprintf() is for verbose information for the user.
 * Display on users request.
 * Same parameters as printf().
 */
void logprintf(const short color, const char *s, ...)
{
	va_list arglist;

	if (logging_is_on) {
		if (log_file) {
			va_start(arglist, s);
			vfprintf(log_file, s, arglist);
			va_end(arglist);
			return;
		}
		if (!ignore_colours)
			/* print "HighWire:" in blue, the rest in 'color' */
			fprintf(stdout, "\33c7\33b4HighWire:\33b%c ", (int)color);
		else
			/* print "HighWire:" inverse */
			fprintf(stdout, "\33pHighWire:\33q ");
		va_start(arglist, s);
		vfprintf(stdout, s, arglist);
		va_end(arglist);
	if (!ignore_colours)
		fprintf(stdout, "\33b?");  /* print in black */
	}
}
