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


void init_logging(void)
{
	fprintf(stdout, "\33H\33v");  /* cursor home, enable line wrap */
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
