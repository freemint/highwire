/* @(#)highwire/Logging.h
 *
 * This module provides logging functionality for the HighWire HTML browser.
 */

#ifndef VARIADIC            /* Gnu C provides a (non-portable) method to have */
# ifndef __GNUC__           /* the compiler check the arguments of a variadic */
#  define VARIADIC(v,a)     /* function against its format string if this is  */
# else                      /* compatible with printf or scanf.               */
#  define VARIADIC(v,a) __attribute__ ((format (printf, v, a)))
# endif
#endif


#define LOG_WHITE    '0'
#define LOG_BLACK    '?'
#define LOG_RED      '1'
#define LOG_GREEN    '2'
#define LOG_BLUE     '4'
#define LOG_CYAN     '6'
#define LOG_YELLOW   '3'
#define LOG_MAGENTA  '5'
#define LOG_LWHITE   '7'
#define LOG_LBLACK   '8'
#define LOG_LRED     '9'
#define LOG_LGREEN   ':'
#define LOG_LBLUE    '<'
#define LOG_LCYAN    '>'
#define LOG_LYELLOW  ';'
#define LOG_LMAGENTA '='


extern BOOL logging_is_on;


void init_logging(void);

/* errprintf() is for possible errors in HighWire.
 * Same parameters as printf(). */
void errprintf(const char *s, ...) VARIADIC(1,2);

/* logprintf() is for verbose information for the user.
 * Display on users request.
 * Same parameters as printf(). */
void logprintf(const short color, const char *s, ...) VARIADIC(2,3);
