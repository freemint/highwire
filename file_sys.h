/*
 * Wrapper file for an uniform file system interface for different compilers.
*/

#include "hw-types.h"


#if defined (__PUREC__)
# include <tos.h>

# define F_xattr(f,n,a)          Fxattr    (f, n, a)
# define D_xreaddir(s,d,b,x,r)   Dxreaddir (s, d, b, x, r)

#elif defined (LATTICE)
# include <dos.h>
# include <mintbind.h>
# define DTA      struct FILEINFO
# define d_attrib attr
# define d_length size
# define d_time   time
# define d_fname  name

# include <fcntl.h>
# define F_xattr(f,n,a)          Fxattr (f, (char*)n, a)
# define D_xreaddir(s,d,b,x,r)   Dxreaddir (s, d, b, (long*)x, r)

#elif defined (__GNUC__)
# include <unistd.h>
# include <mintbind.h>
# define DTA      _DTA
# define d_attrib dta_attribute
# define d_length dta_size
# define d_time   dta_time
# define d_date   dta_date
# define d_fname  dta_name

# include <fcntl.h>
# define F_xattr(f,n,a)          Fxattr (f, n, a)
# define D_xreaddir(s,d,b,x,r)   Dxreaddir (s, d, b, x, r)
#endif

#ifndef O_RAW    /* Lattice uses this for open() to write binary files */
# define O_RAW 0
#endif

#ifndef S_ISDIR
#	define S_ISDIR(mode) (((mode) & 0170000) == (0040000))
#endif


#if !defined(__XATTR)
#define __XATTR
struct xattr {
	UWORD st_mode;
	LONG  st_ino;
	UWORD st_dev;
	UWORD st_rdev;
	UWORD st_nlink;
	UWORD st_uid;
	UWORD st_gid;
	ULONG st_size;
	LONG  st_blksize;
	LONG  st_blocks;
	struct {
		union {
			unsigned long  tv_sec; /* actually time&date in DOSTIME format */
			struct {
				unsigned short time;
				unsigned short date;
			} d;
		} u;
	} st_mtim;
	struct {
		union {
			unsigned long  tv_sec; /* actually time&date in DOSTIME format */
			struct {
				unsigned short time;
				unsigned short date;
			} d;
		} u;
	} st_atim;
	struct {
		union {
			unsigned long  tv_sec; /* actually time&date in DOSTIME format */
			struct {
				unsigned short time;
				unsigned short date;
			} d;
		} u;
	} st_ctim;
	UWORD st_attr;
	UWORD reserved2;
	ULONG reserved3[2];
};
#endif
