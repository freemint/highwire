/*
 * parser.h -- Parser functions for HTML expressions.
 *
 * AltF4 - Jan 14, 2002
 *
 */
#ifndef __PARSER_C__
#define __PARSER_C__


#ifdef __PUREC__
# undef PARSER
typedef struct s_parser * PARSER;
#endif
struct s_parser {
	LOADER   Loader;
	CONTAINR Target;
	FRAME    Frame;
	BOOL     hasStyle;
	PARSESUB Current;
	WCHAR  * Watermark; /* points to the last position that can be written, */
	                    /* 10 bytes reserve behind the high watermark       */
	void       * ResumeFnc; /* fields to store data for the case that a parser */
	const char * ResumePtr; /* function needs to get a break (eg. for loading  */
	const char * ResumeSub; /* additional files)                               */
	short        ResumeErr;
};


PARSER new_parser    (LOADER);
void   delete_parser (PARSER);

int     parser_resume (PARSER, void * func, const char * ptr_or_sub, LOCATION);
				/* Prepares the parser for getting interupted and stores the data
				 * needed for the later resume.  If the location is not NULL it will
				 * be used to start a loader job.  If the function pointer 'func' is
				 * not NULL then 'ptr_or_sub' will be taken as its own data pointer,
				 * else 'ptr_or_sub' will be taken as owned by some subfunction.
				*/
#define parser_resumed(parser) parser_resume (parser, NULL,NULL,NULL);
				/* Shortcut, resets the resume data fields.
				*/

HTMLTAG parse_tag (PARSER, const char ** pptr);
				/* Parses a html TAG expression of the forms
				 *    <TAG>  |  <TAG KEY ...>  |  <TAG KEY=VALUE ...>
				 * The 'pptr' content must point to the first character after the
				 * leading '<' and a possibly '/'.  After processing it is set to
				 * first character behind the expression, either behind the closing
				 * '>' or to a trailing '\0'.
				 * If successful the found known KEYs are stored with their VALUEs
				 * internally and a TAG enum is returned.
				 * Else the symbol TAG_Unknown is returned.
				 * The PARSER argument may be NULL if no KEY storage is needed.
				*/

const char * parse_css (PARSER, const char * ptr, char * takeover);
				/* Parses a whole <style> area from 'ptr' and stores recognized
				 * style set internallly in the parser structure.  Following
				 * get_value..() calls will be served from these sets automatically.
				 * If 'takeover is not NULL it will be used instead of 'ptr' and
				 * stored internal by the parser to be deleted when the parser get
				 * invalidated.
				 * The result value is a pointer to the first not read character
				 * from 'ptr'.
				*/

BOOL    get_value       (PARSER, HTMLKEY, char * output, const size_t max_len);
				/* Finds the VALUE of 'key' that was read while the last parse()
				 * call.  If successful the VALUE will be copied to 'output' up to
				 * 'max_len' character (including the trailing '\0') and a TRUE
				 * will be returned.  Else a FALSE will be returned.
				*/
#define get_value_exists( p, key ) get_value (p, key, NULL,0)
				/* A shorthand that returns TRUE if 'key' was found at all while the
				 * last parse() call.
				*/
char  * get_value_str   (PARSER, HTMLKEY);
				/* Returns the VALUE of 'key' that was read while the last parse()
				 * call as a malloc'ed zero terminated character string.  If not
				 * successful the result is a NULL pointer.
				*/
char    get_value_char  (PARSER, HTMLKEY);
				/* Returns the first character of the VALUE of 'key' that was read
				 * while the last parse() call.  If not successful a '\0' will be
				 * returned.
				*/
WORD    get_value_unum  (PARSER, HTMLKEY, WORD dflt);
				/* Returns the VALUE of 'key' that was read while the last parse()
				 * call as a unsigned short.  If not successful the value of 'dflt'
				 * will be returned instead, which may also be negative.
				*/
WORD    get_value_size  (PARSER, HTMLKEY);
				/* Returns the VALUE of 'key' that was read while the last parse()
				 * call as a signed short.  On success the return value is either a
				 * positive absolute number or a negative fractional of -1024 if a
				 * trailing '%' was found.  Else a zero will be returned.
				*/
WORD    get_value_color (PARSER, HTMLKEY);
				/* Returns the VDI color VALUE of 'key' that was read while the last
				 * parse() call.  If not successful a negative number will be
				 * returned.
				*/


#endif /*__PARSER_C__*/
