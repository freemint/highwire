/* @(#)highwire/av_prot.c
 */


#include <string.h>
#include <stdlib.h>

#include <gem.h>

#include "global.h"
#include "av_comm.h"


static short av_shell_id = -1,     /* Desktop's AES ID */
             av_shell_status = 0;  /* What AV commands can desktop do? */

/* All GLOBAL memory is merged to one block and allocated in main(). */
char *va_helpbuf;  /* HW_PATH_MAX byte */


static short get_avserver(void)
{
	short ret;
	const char *av_env;

	if ((av_env = getenv("AVSERVER")) != NULL)
	{
		ret = appl_find(av_env);

		if (ret >= 0)
			return ret;
	}
	else if ((ret = appl_find("AVSERVER")) >= 0)
		return ret;
	else if ((ret = appl_find("JINNEE  ")) >= 0)
		return ret;
	else if ((ret = appl_find("THING   ")) >= 0)
		return ret;
	else if ((ret = appl_find("MAGXDESK")) >= 0)
		return ret;
	else if ((ret = appl_find("GEMINI  ")) >= 0)
		return ret;
	else if ((ret = appl_find("STRNGSRV")) >= 0)
		return ret;
	else if ((ret = appl_find("DIRECT  ")) >= 0)
		return ret;
	else if ((ret = appl_find("EASY    ")) >= 0)
		return ret;
	else if ((ret = appl_find("KAOS    ")) >= 0)
		return ret;
	else if ((ret = appl_find("CAT     ")) >= 0)
		return ret;

	return -100;
}


BOOL Send_AV(short to_ap_id, short message, const char *data1, const char *data2)
{
	short msg[8];

	(void)data2;  /* unused at the moment, so suppress warning by Pure C */

	/* - 100 to ap id would be no AV server */
	if (to_ap_id == -100)
		return FALSE;

	msg[0] = message;
	msg[1] = gl_apid;
	msg[7] = msg[6] = msg[5] = msg[4] = msg[3] = msg[2] = 0;

	switch (message)
	{
		case PDF_AV_OPEN_FILE:
			to_ap_id = appl_find("MYPDF   ");

			*(char **)&msg[3] = strcpy(va_helpbuf, data1);
			break;
		case AV_EXIT:
			msg[3] = gl_apid;
			break;
		case AV_PROTOKOLL:
			msg[3] = (2|16);  /* VA_START, quoting */
			/* msg[4] = 0;  initialized above */
			/* msg[5] = 0;  initialized above */
			*(char **)&msg[6] = strcpy(va_helpbuf, "HIGHWIRE");
			break;
		case VA_PROTOSTATUS:
			msg[3] = 0x4011;  /* AV_SENDKEY, AV_OPENWIND, quoting */
			/* msg[4] = 0;  initialized above */
			/* msg[5] = 0;  initialized above */
			*(char **)&msg[6] = strcpy(va_helpbuf, "HIGHWIRE");
			break;
		case VA_START:
			*(char **)&msg[3] = strcpy(va_helpbuf, data1);
			break;
		default:
			break;
	}

	return appl_write(to_ap_id, 16, msg);
}


BOOL
Receive_AV(const short msg[8])
{
	switch (msg[0])
	{
		case AV_PROTOKOLL:
			Send_AV(msg[1], VA_PROTOSTATUS, NULL, NULL);
			break;
		case VA_PROTOSTATUS: {
			if (msg[1] == av_shell_id)
				av_shell_status = msg[3];
			break;
		}
	}
	return TRUE;
}


void Init_AV_Protocol(void)
{
	va_helpbuf[0] = '\0';

	av_shell_id = get_avserver();

	Send_AV(av_shell_id, AV_PROTOKOLL, NULL, NULL);
}


void Exit_AV_Protocol(void)
{
	if (av_shell_status & 1024)  /* AV server knows AV_EXIT */
		Send_AV(av_shell_id, AV_EXIT, NULL, NULL);
}


#if 0

BOOL send_vastart(char *path, char *cmdline)
{
	short i;
	char progname[9];
	char *dummy;

	strncpy(progname, path, 8);
	progname[8] = '\0';

	dummy = strrchr(progname, '.');
	if (dummy)
		dummy[0] = '\0';

	/* make certain the name is 8 char long */
	while (strlen(progname) < 8)
		strcat(progname, " ");

	/* make certain the name is uppercase */
	strupr(progname);

	if ((i = appl_find(progname)) >= 0)
	{
		strcpy(va_helpbuf, (char *)&cmdline[1]);

		send_extmessage(i, VA_START, 0, (int)(((long)va_helpbuf >> 16) & 0x0000ffff), (int)((long)va_helpbuf & 0x0000ffffL), 0, 0, 0);

		return TRUE;
	}

	return FALSE;
}

#endif
