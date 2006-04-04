/*
 * Header file for using the drag & drop
 * protocol.
 * Copyright 1992,1993 Atari Corporation.
 */

#define AP_DRAGDROP	63

#define	DD_OK		0
#define DD_NAK		1
#define DD_EXT		2
#define DD_LEN		3
#define DD_TRASH	4
#define DD_PRINTER	5
#define DD_CLIPBOARD	6

/* timeout in milliseconds */
#define DD_TIMEOUT	4000

/* number of bytes of "preferred
 * extensions" sent by receipient during
 * open
 */
#define DD_NUMEXTS	8
#define DD_EXTSIZE	32L

/* max size of a drag&drop item name */
#define DD_NAMEMAX	128

/* max length of a drag&drop header */
#define DD_HDRMAX	(8+DD_NAMEMAX)

#ifndef Word
#define Word short
#define Long long
#endif

void rec_ddmsg(int msg[8] );
int send_ddmsg(int msx, int msy, int kstate, char *name, char *ext, long size, char *data);
