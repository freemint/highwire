/* @(#)highwire/av_prot.c
 */
#include <string.h>
#include <stdlib.h>

#include <gem.h>

#include "global.h"
#include "vaproto.h"
/* #include "av_comm.h" */
#include "hwWind.h" /* new_hwWind, window_raise */

#include "Loader.h"
static BOOL acc_wind_OK = TRUE;
static void handle_avdd (short win_handle, short kstate, char * arg);


/* All GLOBAL memory is merged to one block and allocated in main(). */
char *va_helpbuf;  /* HW_PATH_MAX byte */

short av_shell_id     = -1;   /* Desktop's AES ID */
short av_shell_status = 0;    /* What AV commands can desktop do? */

extern char *thisapp;

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
BOOL
Send_AV (short message, const char * data1, const char * data2)
{
	short msg[8];
	short to_ap_id = av_shell_id;
	
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
			*(char **)(msg+6) = strcpy (va_helpbuf, thisapp);
			break;
		case AV_ACCWINDOPEN:
			if (!cfg_AVWindow) {
				return FALSE;
			}
			msg[3] = *(const short *)data1;
			break;
		case AV_ACCWINDCLOSED:
			if (!cfg_AVWindow) {
				return FALSE;
			}
			msg[3] = *(const short *)data1;
			break;
		case AV_SENDKEY :
			if (!cfg_AVWindow) {
				return FALSE;
			}
			msg[3] = 0x0004;
			msg[4] = 0x1117;	/* ^W */
			break;
    case AV_PATH_UPDATE:
      *(char **)(msg+3) = strcpy(va_helpbuf, data1);
      break;
		case VA_START:
			*(char **)(msg+3) = strcpy(va_helpbuf, data1);
			break;
		default:
			return FALSE; /* not supported here */
	}

	return (appl_write (to_ap_id, 16, msg) > 0);
}


/*============================================================================*/
BOOL
Receive_AV (WORD msg[8])
{
	WORD d;
	char * str_p; /* *arg; */
	WORD  kstate = 0, whandle;
/*	POSENTRY	*va_list = NULL; */
	switch (msg[0]) {
		case VA_PROTOSTATUS :
			if (msg[1] == av_shell_id) {
				av_shell_status = msg[3];
			}
			if (acc_wind_OK && !(av_shell_status & AA_ACCWIND)) {
				acc_wind_OK = FALSE;
			}
#ifdef AVSERVER_TEST
			if (av_shell_status & AA_SENDKEY) puts ("SENDKEY\r");
			else                              puts ("SENDKEY error\r");
			if (av_shell_status & AA_ASKFILEFONT) puts ("ASKFILEFONT\r");
			else                                  puts ("ASKFILEFONT error\r");
			if (av_shell_status & AA_ASKCONFONT) puts ("ASKCONFONT\r");
			else                                 puts ("ASKCONFONT error\r");
			if (av_shell_status & AA_ASKOBJECT) puts ("ASKOBJECT\r");
			else                                puts ("ASKOBJEKT error\r");
			if (av_shell_status & AA_OPENWIND) puts ("OPENWIND\r");
			else                               puts ("OPENWIND error\r");
			if (av_shell_status & AA_STARTPROG) puts ("STARTPROG\r");
			else                                puts ("STARTPROG error\r");
			if (av_shell_status & AA_ACCWIND) puts ("ACCWIND\r");
			else                              puts ("ACCWIND error\r");
			if (av_shell_status & AA_STATUS) puts ("STATUS\r");
			else                             puts ("STATUS error\r");
			if (av_shell_status & AA_COPY_DRAGGED) puts ("COPY_DRAGGED\r");
			else                                   puts ("COPY_DRAGGED error\r");
			if (av_shell_status & AA_DRAG_ON_WINDOW) puts ("DRAG_ON_WINDOW\r");
			else                                   puts ("DRAG_ON_WINDOW error\r");
			if (av_shell_status & AA_EXIT) puts ("EXIT\r");
			else                           puts ("EXIT error\r");
			if (av_shell_status & AA_XWIND) puts ("XWIND\r");
			else                            puts ("XWIND error\r");
			if (av_shell_status & AA_FONTCHANGED) puts ("FONTCHANGED\r");
			else                                  puts ("FONTCHANGED error\r");
			if (av_shell_status & AA_STARTED) puts ("STARTED\r");
			else                              puts ("STARTED error\r");
			if (av_shell_status & AA_SRV_QUOTING) puts ("SRV_QUOTING\r");
			else                                  puts ("SRV_QUOTING error\r");
			if (av_shell_status & AA_FILE) puts ("FILE\r");
			else                           puts ("FILE error\r");
#endif
			break;

		case AV_SENDKEY :  /* doesn't seem to be necessary at all ??? */
				printf ("AV_SENDKEY von %d: %x, %x\r\n", msg[1], msg[3], msg[4]);
				if ((msg[3] == 0x0004) && (msg[4] == 0x1117)) { 	/* ^W */
					window_raise (NULL, TRUE, NULL);
				}
			break;
#if 0
		case VA_DRAG_COMPLETE :
			if (debug_level & DBG_AV)
				debug("VA_DRAG_COMPLETE.\n");
			if (glob_data != NULL)
			{
				free(glob_data);
				glob_data = NULL;
			}
			break;
#endif

		case VA_DRAGACCWIND :				/* bei D&D mit glob. Fensterwechsel */
			str_p   = *(char **)(msg+6);
			whandle = msg[3];
			graf_mkstate (&d, &d, &d, &kstate);
/*			printf ("VA_DRAGACCWIND von %d: %x, %x\r\n", msg[1], msg[3], msg[4]);*/
			if (str_p != NULL) {
				handle_avdd (whandle, kstate, str_p);
			}
			break;
		}
	return TRUE;
}


/*============================================================================*/
void
Init_AV_Protocol(void)
{
	va_helpbuf[0] = '\0';

	av_shell_id = get_avserver();

	Send_AV (AV_PROTOKOLL, NULL, NULL);
}


/*============================================================================*/
void
Exit_AV_Protocol(void)
{
	if (av_shell_status & AA_EXIT) {  /* AV server knows AV_EXIT */
		Send_AV (AV_EXIT, NULL, NULL);
	}
}


/*============================================================================*/
void
send_avwinopen (short handle)
{
	if (acc_wind_OK) {
		Send_AV (AV_ACCWINDOPEN, (char*)&handle, NULL);
	}
}

/*============================================================================*/
void
send_avwinclose (short handle)
{
	if (acc_wind_OK) {
		Send_AV (AV_ACCWINDCLOSED, (char*)&handle, NULL);
	}
}


/*----------------------------------------------------------------------------*/
static void
handle_avdd (short win_handle, short kstate, char * arg)
{
	char   filename[HW_PATH_MAX];
	char * cmd_orig = strdup(arg);
	BOOL   quoted;
	
 	if (win_handle && cmd_orig) {
 		HwWIND wind;
		char * cmd = cmd_orig;
		char * p   = filename;
		char * s;
		
		if (cmd[0] == '\'') {
			cmd++;
			quoted = TRUE;
		} else {
			quoted = FALSE;
		}
		while (cmd[0] != '\0') {
			if (cmd[0] == '\'') {
				quoted = TRUE;
				switch (cmd[1]) {
					case '\'':
						cmd++;
						break;
					case ' ':
						p[0] = '\0';
						s    = cmd +1;
						cmd  = filename;
						new_hwWind ("", cmd, TRUE);
						quoted = FALSE;
						cmd = s;
						if (cmd[0] == '\'')
							cmd++;
						if (cmd[0] != '\0') {
							p = filename;
						}
						cmd++;
						if (cmd[0] == '\'') {
							cmd++;
							quoted = TRUE;
						}
						break;
					default:
						break;
				}
			}
			p++[0] = cmd++[0];
			if (!quoted && cmd[0] == ' ') {
				p[0] = '\0';
				s    = cmd +1;
				cmd  = filename;
				if ((kstate & K_ALT)
				    || ((wind = hwWind_byHandle (win_handle)) != NULL
				         && wind->Base.Ident != WIDENT_BRWS)) {
					wind = NULL;
				}
				if (!wind) new_hwWind      ("", cmd, TRUE);
				else       start_cont_load (wind->Pane, cmd, NULL, TRUE, TRUE);
				cmd = s;
				if (cmd[0] == '\'') {
					quoted = TRUE;
					cmd++;
				}
				if (cmd[0] != '\0') {
					p = filename;
				}
				if (cmd[0] == '\'') {
				  cmd++;
				}
			}
		}
		if (cmd > cmd_orig && cmd[-1] == '\'') {
			p[-1] = '\0';
		} else {
			p[0] = '\0';
		}
		cmd = filename;
		if ((kstate & K_ALT)
		    || ((wind = hwWind_byHandle (win_handle)) != NULL
		         && wind->Base.Ident != WIDENT_BRWS)) {
			wind = NULL;
		}
		if (!wind) new_hwWind      ("", cmd, TRUE);
		else       start_cont_load (wind->Pane, cmd, NULL, TRUE, TRUE);
   }
	
	if (cmd_orig) free (cmd_orig);
}
