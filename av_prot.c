/* @(#)highwire/av_prot.c
 */
#include <string.h>
#include <stdlib.h>

#include <gem.h>

#include "global.h"
#include "vaproto.h"

/* #include "av_comm.h" */


/* All GLOBAL memory is merged to one block and allocated in main(). */
char *va_helpbuf;  /* HW_PATH_MAX byte */

short av_shell_id     = -1;   /* Desktop's AES ID */
short av_shell_status = 0;    /* What AV commands can desktop do? */

             
#ifdef AVWIND
#include "Window.h" /* cause of window_raise */

BOOL	wind_cycle = TRUE; 
#endif

/*============================================================================*/
short
get_avserver(void)
{
	short        ret    = -100;
	const char * av_env = getenv("AVSERVER");
	if (av_env) {
		char av_envname[9];
		strncpy(av_envname,av_env, 8);
		av_envname[8] = '\0';
		while (strlen (av_envname) < 8) {
			strcat(av_envname, " ");
		}
		ret = appl_find (av_envname);
	}
	return ret;
}


/*============================================================================*/
BOOL Send_AV(short message, const char *data1, const char *data2)
{
	short msg[8];
	short to_ap_id;
	
	to_ap_id=av_shell_id;
	
	(void)data2;  /* unused at the moment, so suppress warning by Pure C */

	/* - 100 to ap id would be no AV server */
	if (to_ap_id == -100)
		return FALSE;

	msg[0] = message;
	msg[1] = gl_apid;
	msg[7] = msg[6] = msg[5] = msg[4] = msg[3] = msg[2] = 0;

	switch (message)
	{
/*		case PDF_AV_OPEN_FILE:
			to_ap_id = appl_find("MYPDF   ");

			*(char **)&msg[3] = strcpy(va_helpbuf, data1);
			break;  */ /* commented out for the moment */
		case AV_EXIT:
			msg[3] = gl_apid;
			break;
		case AV_PROTOKOLL:
			msg[3] = VV_START | VV_ACC_QUOTING; 
			/* msg[4] = 0;  initialized above */
			/* msg[5] = 0;  initialized above */
			*(char **)(msg+6) = strcpy(va_helpbuf, "HIGHWIRE");
			break;
#ifdef AVWIND
		case AV_ACCWINDOPEN:
			*(char **)(msg+3) = va_helpbuf;
			printf ("AV_ACCWINDOPEN: %s\r\n",va_helpbuf);
			break;
		case AV_ACCWINDCLOSED:
			*(char **)(msg+3) = va_helpbuf;
			printf ("AV_ACCWINDCLOSE: %s\r\n",va_helpbuf);
			break;
#endif
		case VA_START:
			*(char **)(msg+3) = strcpy(va_helpbuf, data1);
			break;
		default:
			break;
	}

	return appl_write(to_ap_id, 16, msg);
}


/*============================================================================*/
BOOL
Receive_AV(const short msg[8])
{
	switch (msg[0]) {
		case VA_PROTOSTATUS :
			if (msg[1] == av_shell_id) 
				av_shell_status = msg[3];
#ifdef AVWIND
			if (wind_cycle && !(av_shell_status & AA_ACCWIND))
				wind_cycle = FALSE;
#endif
#ifdef AVSERVER_TEST
			if (av_shell_status & AA_SENDKEY) puts ("SENDKEY\r"); else puts ("SENDKEY error\r");
			if (av_shell_status & AA_ASKFILEFONT) puts ("ASKFILEFONT\r");else puts ("ASKFILEFONT error\r");
			if (av_shell_status & AA_ASKCONFONT) puts ("ASKCONFONT\r");else puts ("ASKCONFONT error\r");
			if (av_shell_status & AA_ASKOBJECT) puts ("ASKOBJECT\r");else puts ("ASKOBJEKT error\r");
			if (av_shell_status & AA_OPENWIND) puts ("OPENWIND\r");else puts ("OPENWIND error\r");
			if (av_shell_status & AA_STARTPROG) puts ("STARTPROG\r");else puts ("STARTPROG error\r");
			if (av_shell_status & AA_ACCWIND) puts ("ACCWIND\r");else puts ("ACCWIND error\r");
			if (av_shell_status & AA_STATUS) puts ("STATUS\r");else puts ("STATUS error\r");
			if (av_shell_status & AA_COPY_DRAGGED) puts ("COPY_DRAGGED\r");else puts ("COPY_DRAGGED error\r");
			if (av_shell_status & AA_DRAG_ON_WINDOW) puts ("DRAG_ON_WINDOW\r");else puts ("DRAG_ON_WINDOW error\r");
			if (av_shell_status & AA_EXIT) puts ("EXIT\r");else puts ("EXIT error\r");
			if (av_shell_status & AA_XWIND) puts ("XWIND\r");else puts ("XWIND error\r");
			if (av_shell_status & AA_FONTCHANGED) puts ("FONTCHANGED\r");else puts ("FONTCHANGED error\r"); 
			if (av_shell_status & AA_STARTED) puts ("STARTED\r");else puts ("STARTED error\r");
			if (av_shell_status & AA_SRV_QUOTING) puts ("SRV_QUOTING\r");else puts ("SRV_QUOTING error\r");
			if (av_shell_status & AA_FILE) puts ("FILE\r");else puts ("FILE error\r");
#endif
			break;

		case AV_SENDKEY :
#ifdef AVWIND
			if (av_shell_status & AA_SENDKEY) {
				printf ("receive_AV: VA_SENDKEY %x\r\n", msg[0]);
				if (/*(msg[3] == 4) && */ (msg[4] == 0x0017)) 	/* ^W */
				{
					printf ("AV_SENDKEY CTRL-W\r\n"); 
					window_raise (NULL, TRUE, NULL);
				}
			}
#endif		
			break;	
	}
	return TRUE;
}


/*============================================================================*/
void Init_AV_Protocol(void)
{
	va_helpbuf[0] = '\0';

	av_shell_id = get_avserver();

	Send_AV(AV_PROTOKOLL, NULL, NULL);
}


/*============================================================================*/
void Exit_AV_Protocol(void)
{
	if (av_shell_status & AA_EXIT)  /* AV server knows AV_EXIT */
		Send_AV(AV_EXIT, NULL, NULL);
}


#ifdef AVWIND
/*============================================================================*/
void send_avwinopen(short handle)
{
	sprintf (va_helpbuf, "%hi", handle);
	Send_AV(AV_ACCWINDOPEN, NULL, NULL);
}


/*============================================================================*/
void send_avwinclose(short handle)
{
	sprintf (va_helpbuf, "%hi", handle);
	Send_AV(AV_ACCWINDCLOSED, NULL, NULL);

}
#endif
