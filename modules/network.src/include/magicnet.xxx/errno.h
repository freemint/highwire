#ifndef _ERRNO_H
#define _ERRNO_H

#ifndef _COMPILER_H
#include <compiler.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define	ENOERR		  0		/* no error */
#define E_OK		  ENOERR	/*  "   "   */
#define	EERROR		  1		/* generic error */

#define	EDRNRDY		  2		/* drive not ready */
#define EDRVNR		  EDRNRDY	/*   "    "    "   */
#define	EUKCMD		  3		/* unknown command */
#define EUNCMD		  EUKCMD	/*    "      "     */
#define	ECRC		  4		/* crc error */
#define E_CRC		  ECRC		/*  "   "    */
#define	EBADREQ		  5		/* bad request */
#define EBADRQ		  EBADREQ	/*  "    "     */
#define	ESEEK		  6		/* seek error */
#define E_SEEK		  ESEEK		/*   "    "   */
#define	EUKMEDIA	  7		/* unknown media */
#define EMEDIA		  EUKMEDIA	/*    "     "    */
#define	ESECTOR		  8		/* sector not found */
#define ESECNF		  ESECTOR	/*    "    "   "    */
#define	EPAPER		  9		/* no paper */
#define	EWRITE		 10		/* write fault */
#define EWRITEF		 EWRITE		/*   "     "   */
#define	EREAD		 11		/* read fault */
#define EREADF		 EREAD		/*  "     "   */
#define	EGENERIC	 12		/* general mishap */
#define	EROFS		 13		/* write protect */
#define	ECHMEDIA	 14		/* media change */
#define E_CHNG		 ECHMEDIA	/*   "     "    */

#define	EUKDEV		 15		/* unknown device */
#define EUNDEV		 EUKDEV		/*    "      "    */
#define ENODEV		 EUKDEV		/*    "      "    */
#define	EBADSEC		 16		/* bad sectors */
#define EBADSF		 EBADSEC	/*  "    "     */
#define	EIDISK		 17		/* insert disk */
#define EOTHER		 EIDISK		/*    "    "   */
					/* (gap) */
#define	EINVAL		 25		/* invalid function number */
#define ENOSYS		 32		/*   "        "       "    */
#define	ENOENT		 33		/* file not found */
#define ESRCH		 ENOENT		/* pid not found */
#define ECHILD		 ENOENT		/* no children (wait/waipid) */
#define	EPATH		 34		/* path not found */
#define ENOTDIR		 EPATH
#define	EMFILE		 35		/* no more handles */
#define	EACCESS		 36		/* access denied */
#define	EACCES		 36		/* access denied */
#define EPERM		 EACCESS	/*    "      "   */
#define	EBADF		 37		/* invalid handle */
#define	ENOMEM		 39		/* insufficient memory */
#define	EFAULT		 40		/* invalid memory block address */
#define	ENXIO		 46		/* invalid drive spec */
#define	EXDEV		 48		/* cross device rename */
#define	ENMFILES	 49		/* no more files (fsnext) */
#define ENMFIL		 49		/* no more files (from fsnext) */

#define ELOCKED		 58		/* locking conflict */

#define	EBADARG		 64		/* range error/context unknown */
#define	EINTERNAL	 65		/* internal error */
#define EINTRN		 EINTERNAL	/*    "       "   */
#define	ENOEXEC		 66		/* invalid program load format */
#define EPLFMT		 ENOEXEC	/*     "      "      "    "    */
#define	ESBLOCK		 67		/* set block failed/growth restraints*/
#define EGSBF		 ESBLOCK	/* or memory block growth failure */
					/* (gap) */
# define EMLINK		 80		/* too many symbolic links */
# define ELOOP		 EMLINK
#define EPIPE		 81		/* write to broken pipe */
# define EEXIST		 85		/* file exists, try again later */
# define ENOTEMPTY	 EEXIST
# define ENAMETOOLONG 	 86		/* name too long */
# define ENOTTY		 87
# define ERANGE		 88
# define EDOM		 89
#define EIO		 90		/* I/O error */
#define ENOSPC		 91		/* disk full */

#define EINTR	        128		/* this *should* be fake */

#ifdef __MINT__

/* Network error numbers -- only useful with Kay Roemer's socket library */

#define _NE_BASE 300

#define	ENOTSOCK	(_NE_BASE + 0)	/* Socket operation on non-socket */
#define	EDESTADDRREQ	(_NE_BASE + 1)	/* Destination address required */
#define	EMSGSIZE	(_NE_BASE + 2)	/* Message too long */
#define	EPROTOTYPE	(_NE_BASE + 3)	/* Protocol wrong type for socket */
#define	ENOPROTOOPT	(_NE_BASE + 4)	/* Protocol not available */
#define	EPROTONOSUPPORT	(_NE_BASE + 5)	/* Protocol not supported */
#define	ESOCKTNOSUPPORT	(_NE_BASE + 6)	/* Socket type not supported */
#define	EOPNOTSUPP	(_NE_BASE + 7)	/* Operation not supported */
#define	EPFNOSUPPORT	(_NE_BASE + 8)	/* Protocol family not supported */
#define	EAFNOSUPPORT	(_NE_BASE + 9)	/* Address family not supported by
						protocol */
#define	EADDRINUSE	(_NE_BASE + 10)	/* Address already in use */
#define	EADDRNOTAVAIL	(_NE_BASE + 11)	/* Cannot assign requested address */
#define	ENETDOWN	(_NE_BASE + 12)	/* Network is down */
#define	ENETUNREACH	(_NE_BASE + 13)	/* Network is unreachable */
#define	ENETRESET	(_NE_BASE + 14)	/* Network dropped conn. because of
						reset */
#define	ECONNABORTED	(_NE_BASE + 15)	/* Software caused connection abort */
#define	ECONNRESET	(_NE_BASE + 16)	/* Connection reset by peer */
#define	EISCONN		(_NE_BASE + 17)	/* Socket is already connected */
#define	ENOTCONN	(_NE_BASE + 18)	/* Socket is not connected */
#define	ESHUTDOWN	(_NE_BASE + 19)	/* Cannot send after shutdown */
#define	ETIMEDOUT	(_NE_BASE + 20)	/* Connection timed out */
#define	ECONNREFUSED	(_NE_BASE + 21)	/* Connection refused */
#define	EHOSTDOWN	(_NE_BASE + 22)	/* Host is down */
#define	EHOSTUNREACH	(_NE_BASE + 23)	/* No route to host */
#define	EALREADY	(_NE_BASE + 24)	/* Operation already in progress */
#define	EINPROGRESS	(_NE_BASE + 25)	/* Operation now in progress */
#define EWOULDBLOCK	(_NE_BASE + 26)	/* Operation would block */

#define _NE_MAX		EWOULDBLOCK

#endif

#ifndef AssemB
extern	int	errno;
extern	int	sys_nerr;
extern	char *	sys_errlist[];
#endif /* AssemB */

#ifdef __cplusplus
}
#endif

#endif /* _ERRNO_H */
