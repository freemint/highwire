/*
 * hw-types.h -- Base Type Definitions.
 *
 */
#ifndef __HW_TYPES_H__
#define __HW_TYPES_H__


/*--- some simple data type definitions ---*/

#undef FALSE
#undef TRUE
#undef BOOL
typedef enum { FALSE = 0, TRUE = 1 } BOOL;
#define BOOL  BOOL
#define FALSE FALSE
#define TRUE  TRUE

#undef BYTE
#undef CHAR
#undef WORD
#undef UWORD
#undef LONG
#undef ULONG
typedef signed   char  BYTE;
typedef unsigned char  CHAR;
#ifdef __PUREC__
typedef signed   int WORD;
#else
typedef signed   short WORD;
#endif
typedef unsigned short UWORD;
typedef signed   long  LONG;
typedef unsigned long  ULONG;


/*--- structure definitions as also provided by gem.h ---*/

#ifdef _GEMLIB_H_
# define GRECT GRECT
# ifdef _GEMLIB_X_H_
#  define WCHAR WCHAR
# endif

#else
# define MAP_BITSTREAM 0
# define MAP_ATARI     1
# define MAP_UNICODE   2
#endif

#ifndef WCHAR
typedef unsigned short WCHAR;  /* 16-bit character, for BICS or Unicode */
#define WCHAR WCHAR
#endif

#ifndef __PXY
typedef struct point_coord {
	WORD p_x;
	WORD p_y;
} PXY;
# define __PXY
#endif

#ifndef __GRECT
typedef struct graphic_rectangle {
	WORD g_x;
	WORD g_y;
	WORD g_w;
	WORD g_h;
} GRECT;
# define __GRECT
#endif

#ifndef COLORS_CHANGED
#	define COLORS_CHANGED 84   /* for shel_write(SWM_BROADCAST, ..) */
#endif

/*--- HighWire specific stuff ---*/

typedef enum {
	ENCODING_Unknown = 0,
	ENCODING_WINDOWS1252, ENCODING_ISO8859_2, ENCODING_ISO8859_15,
	ENCODING_UTF8, ENCODING_UTF16, ENCODING_UTF16LE,
	ENCODING_MACINTOSH, ENCODING_ATARIST, ENCODING_ATARINVDI
} ENCODING;


/*--- assorted stuff ---*/

#define numberof(arr)   (sizeof(arr) / sizeof(*arr))
						/* calculates the element number of an literally array
						 * at compile time.  doesn't work for pointers! */

#if defined(__PUREC__)
# ifndef min
#  define min(a, b)   ((a) < (b) ? (a) : (b))
# endif
# ifndef max
#  define max(a, b)   ((a) > (b) ? (a) : (b))
# endif

#elif defined(__GNUC__)
# ifndef _MACROS_H
#  define max(a,b) ({__typeof__(a)_a=(a); __typeof__(b)_b=(b); (_a>_b?_a:_b);})
#  define min(a,b) ({__typeof__(a)_a=(a); __typeof__(b)_b=(b); (_a<_b?_a:_b);})
# endif
#endif


/*--- Some Error Codes ---*/

#define E_OK 0
#define EINVFN 32 /* Function not implemented. */
#define ENOENT 33
#define EACCDN 36
#define EINVAL 25 /* Invalid argument. */
#define EINTR 128 /* Interrupted function call. */
#define EPROTONOSUPPORT 305 /* Protocol not supported.  */
#define ECONNRESET      316 /* Connection reset by peer. */
#define ETIMEDOUT       320 /* Connection timed out. */


/*--- Macro for prototyping ---*/

#ifndef __CDECL
# ifdef __PUREC__
#  define __CDECL cdecl
# else
#  define __CDECL
# endif
#endif


#endif /*__HW_TYPES_H__*/
