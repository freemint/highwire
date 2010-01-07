/*
 * Header file for using the drag & drop
 * protocol.
 * Copyright 1992,1993 Atari Corporation.
 */

#define AP_DRAGDROP	63
#define DD_TRASH     4
#define DD_PRINTER   5
#define DD_CLIPBOARD 6

void rec_ddmsg (WORD msg[8] );
int send_ddmsg (WORD msx, WORD msy, WORD kstate,
                char *name, char *ext, long size, char *data);
