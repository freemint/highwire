/*
 * hw-types.h -- Base Type Definitions.
 *
 */
#ifndef __HW_TYPES_H__
#define __HW_TYPES_H__


/*--- some simple data type definitions ---*/

typedef enum { FALSE = (1!=1), TRUE = (1==1) } BOOL;
#define BOOL  BOOL
#define FALSE FALSE
#define TRUE  TRUE

typedef signed   char  BYTE;
typedef unsigned char  CHAR;
typedef signed   short WORD;
typedef unsigned short UWORD;
typedef signed   long  LONG;
typedef unsigned long  ULONG;


/*--- structure definitions as also provided by gem.h ---*/

#ifdef _GEMLIB_H_
# define PXY   PXY
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

#ifndef PXY
typedef struct point_coord {
	short p_x;
	short p_y;
} PXY;
# define PXY PXY
#endif

#ifndef GRECT
typedef struct graphic_rectangle {
	short g_x;
	short g_y;
	short g_w;
	short g_h;
} GRECT;
# define GRECT GRECT
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

#ifndef E_OK
#define E_OK 0
#endif
#ifndef EINVFN
#define EINVFN 32 /* Function not implemented. */
#endif
#ifndef EINVAL
#define EINVAL 25 /* Invalid argument. */
#endif
#ifndef EINTR
#define EINTR 128 /* Interrupted function call. */
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 320 /* Connection timed out. */
#endif


/*--- Macro for prototyping ---*/

#ifndef __CDECL
# ifdef __PUREC__
#  define __CDECL cdecl
# else
#  define __CDECL
# endif
#endif


#endif /*__HW_TYPES_H__*/
