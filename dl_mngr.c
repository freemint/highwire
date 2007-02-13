#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define __TOS  /* strange, without this poor c wouldn't compile */
#include <gem.h>

#include "file_sys.h"
#include "global.h"
#include "schedule.h"
#include "Location.h"
#include "Loader.h"
#include "Window.h"
#include "highwire.h"
#include "inet.h"

static OBJECT * dlm_form = NULL;
static WINDOW   dlm_wind = NULL;


#define BUF_SIZE 8192ul

typedef struct s_slot {
	const WORD Base, File, Info, Pbar, Bttn;
	struct s_sdat {
		LOCATION Location;
		WORD     Source;
		WORD     Target;
		char   * Buffer;
		long     Size;   /* size of the source or <0 if unknown           */
		long     Fill;   /* number of already stored bytes in target file */
		long     Blck;   /* remaining byte in 'Buffer'                    */
		long     Chnk;   /* !=0: chunked mode                             */
	} Data;
} * SLOT;
static struct s_slot slot_tab[4] = {
	{ DLM_1, DLM_1FILE,DLM_1TEXT,DLM_1BAR,DLM_1BTN, {NULL,-1,-1,NULL,0,0,0,0} },
	{ DLM_2, DLM_2FILE,DLM_2TEXT,DLM_2BAR,DLM_2BTN, {NULL,-1,-1,NULL,0,0,0,0} },
	{ DLM_3, DLM_3FILE,DLM_3TEXT,DLM_3BAR,DLM_3BTN, {NULL,-1,-1,NULL,0,0,0,0} },
	{ DLM_4, DLM_4FILE,DLM_4TEXT,DLM_4BAR,DLM_4BTN, {NULL,-1,-1,NULL,0,0,0,0} }
};
#define slot_end   (slot_tab + numberof(slot_tab) -1)

static char * bttn_text_ok     = "-";
static char * bttn_text_stop   = "-";
static char * bttn_text_cancel = "-";
static size_t file_text_max    = MAX_LEN; /* from gem.h */


/*******************************************************************************
 *
 * Functions to modify the GUI.
*/

/*** some macros to make the code more handy ***/
/* G_BOX: base box covering a slot */
#define BASE_Flags(slot)   dlm_form[(slot)->Base].ob_flags
/* G_STRING */
#define FILE_Strng(slot)   dlm_form[(slot)->File].ob_spec.free_string
#define FILE_StrLn(slot)   strlen (FILE_Strng(slot))
/* G_TEXT */
#define INFO_Ospec(slot)   dlm_form[(slot)->Info].ob_spec
#define INFO_Strng(slot)   dlm_form[(slot)->Info].ob_spec.tedinfo->te_ptext
#define INFO_StrLn(slot)   dlm_form[(slot)->Info].ob_spec.tedinfo->te_txtlen
#define INFO_Adjst(slot)   dlm_form[(slot)->Info].ob_spec.tedinfo->te_just
/* G_BOX: the progress bar */
#define PBAR_Ospec(slot)   dlm_form[(slot)->Pbar].ob_spec
#define PBAR_Flags(slot)   dlm_form[(slot)->Pbar].ob_flags
#define PBAR_Fpatt(slot)   dlm_form[(slot)->Pbar].ob_spec.obspec.fillpattern
#define PBAR_Color(slot)   dlm_form[(slot)->Pbar].ob_spec.obspec.interiorcol
#define PBAR_Rect(slot)    (*(GRECT*)&dlm_form[(slot)->Pbar].ob_x)
/* G_BUTTON */
#define BTTN_State(slot)   dlm_form[(slot)->Bttn].ob_state
#define BTTN_Strng(slot)   dlm_form[(slot)->Bttn].ob_spec.free_string


/*----------------------------------------------------------------------------*/
#define slot_hide(slot)   {BASE_Flags(slot) |=  OF_HIDETREE;}
#define slot_view(slot)   {BASE_Flags(slot) &= ~OF_HIDETREE;}
#define slot_used(slot) (!(BASE_Flags(slot)  &  OF_HIDETREE))

/*----------------------------------------------------------------------------*/
static SLOT
slot_find (void)
{
	SLOT slot = slot_tab;
	while (slot_used(slot)) {
		if (++slot > slot_end) return NULL;
	}
	slot_view (slot);
	INFO_Adjst(slot) =  TE_LEFT;
	PBAR_Flags(slot) |= OF_HIDETREE;
	PBAR_Fpatt(slot) =  IP_SOLID;
	PBAR_Color(slot) =  G_GREEN;
	BTTN_State(slot) &= ~OS_SELECTED;
	return slot;
}

/*----------------------------------------------------------------------------*/
static void
slot_remove (SLOT slot)
{
	if (slot->Data.Source >= 0) {
		LOCATION loc = slot->Data.Location;
		if (PROTO_isRemote (loc->Proto)) inet_close (slot->Data.Source);
		else                             close      (slot->Data.Source);
	}
	if (slot->Data.Target >= 0) {
		close (slot->Data.Target);
	}
	if (slot->Data.Buffer) {
		free (slot->Data.Buffer);
	}
	free_location (&slot->Data.Location);
	
	if (slot < slot_end) {
		SLOT next = slot +1;
		if (slot_used (next)) {
			char * file_text = FILE_Strng(slot);
			OBSPEC info_spec = INFO_Ospec(slot);
			char * bttn_text = BTTN_Strng(slot);
			do {
				FILE_Strng(slot) = FILE_Strng(next);
				INFO_Ospec(slot) = INFO_Ospec(next);
				PBAR_Ospec(slot) = PBAR_Ospec(next);
				PBAR_Rect(slot)  = PBAR_Rect(next);
				BTTN_Strng(slot) = BTTN_Strng(next);
				BTTN_State(slot) = BTTN_State(next);
				slot->Data       = next->Data;
				slot             = next;
			} while (++next <= slot_end && slot_used (next));
			FILE_Strng(slot) = file_text;
			INFO_Ospec(slot) = info_spec;
			BTTN_Strng(slot) = bttn_text;
		}
		slot->Data.Location = NULL;
	}
	slot->Data.Source = -1;
	slot->Data.Target = -1;
	slot->Data.Buffer = NULL;
	slot_hide (slot);
}

/*----------------------------------------------------------------------------*/
#define slot_Arg(slot)   (void*)FILE_Strng(slot)

/*----------------------------------------------------------------------------*/
static SLOT
slot_byArg (void * arg)
{
	SLOT slot = slot_tab;
	while (arg != slot_Arg(slot)) {
		if (++slot > slot_end) return NULL;
	}
	return slot;
}

/*----------------------------------------------------------------------------*/
static void
slot_setfile (SLOT slot, LOCATION loc)
{
	char buf[2048], * p = buf;
	size_t len = location_FullName (loc, buf, sizeof(buf));
	if (len > file_text_max) {
		p += len - file_text_max;
		*p = '®';
	}
	strcpy (FILE_Strng(slot), p);
}

/*----------------------------------------------------------------------------*/
static void
slot_error (SLOT slot, const char * text)
{
	strncpy (INFO_Strng(slot), text, INFO_StrLn(slot) -1);
	INFO_Adjst(slot) =  TE_LEFT;
	BTTN_Strng(slot) =  bttn_text_cancel;
	PBAR_Flags(slot) &= ~OF_HIDETREE;
	PBAR_Color(slot) =  G_RED;
	if (slot->Data.Size < 0) {
		PBAR_Rect(slot).g_x = 0;
		PBAR_Rect(slot).g_w = 256;
	}
}

/*----------------------------------------------------------------------------*/
static void
slot_redraw (SLOT slot, BOOL only_this)
{
	GRECT clip;
	clip.g_x = dlm_form[ROOT].ob_x;
	clip.g_w = dlm_form[ROOT].ob_width;
	clip.g_y = dlm_form[ROOT].ob_y + dlm_form[slot->Base].ob_y;
	clip.g_h = dlm_form[only_this ? slot->Base : ROOT].ob_height;
	window_redraw (dlm_wind, &clip);
}


/*******************************************************************************
 *
 * Job functions (callbacks).
*/
typedef struct s_sdat * SDAT;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
load_job (void * arg, long invalidated)
{
	SLOT slot = slot_byArg (arg);
	SDAT data = &slot->Data;
	
	if (invalidated) {
		return FALSE;
	}
	
	if (data->Source >= 0) {
		long n = read (data->Source, data->Buffer, BUF_SIZE);
		if (n > 0) {
			write (data->Target, data->Buffer, n);
			data->Fill += n;
			n = (data->Fill *256) / data->Size;
			if (n > PBAR_Rect(slot).g_w) {
				GRECT clip;
				objc_offset (dlm_form, slot->Pbar, &clip.g_x, &clip.g_y);
				clip.g_h =  PBAR_Rect(slot).g_h;
				clip.g_x += PBAR_Rect(slot).g_w;
				clip.g_w =  n - PBAR_Rect(slot).g_w;
				PBAR_Rect(slot).g_w = n;
				window_redraw (dlm_wind, &clip);
			}
			
		} else {
			BTTN_Strng(slot) = bttn_text_ok;
			PBAR_Fpatt(slot) = IP_4PATT;
			window_redraw (dlm_wind, NULL);
			close (data->Source);
			data->Source = -1;
		}
	}
	if (data->Source < 0) {
		close (data->Target);
		data->Target = -1;
		free (data->Buffer);
		data->Buffer = NULL;
		window_raise (dlm_wind, TRUE, NULL);
		return FALSE;
	}
	return TRUE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
recv_job (void * arg, long invalidated)
{
	SLOT slot = slot_byArg (arg);
	SDAT data = &slot->Data;
	BOOL save = FALSE;
	WORD draw = 0, dr_x = 0, dr_w = 0;
	
	if (invalidated) {
		return FALSE;
	}
	
	if (data->Chnk < 0) { /* read chunk size */
		char * cr = (data->Blck >= 5
		             ? memchr (data->Buffer +2, '\n', data->Blck -2) : NULL);
		if (cr) {
			*(cr++) = '\0';
			if ((data->Chnk = strtoul (data->Buffer +2, NULL, 16)) > 0) {
				data->Blck -= cr - data->Buffer;
				if (data->Blck > 0) {
					memmove (data->Buffer, cr, data->Blck);
					save = (data->Blck >= data->Chnk);
				}
			} else { /* end download */
				data->Blck = 0; /* ignore remaining data */
				if (data->Source >= 0) {
					inet_close (data->Source);
					data->Source = -1;
				}
			}
		} else if (data->Source < 0) {
			data->Blck = 0; /* chunk header incomplete */
		}
	}
	if (data->Source >= 0) {
		long n = BUF_SIZE - data->Blck;
		if (data->Size > 0 && n > data->Size - data->Fill) {
			n = data->Size - data->Fill;
		}
		n = inet_recv (data->Source, data->Buffer + data->Blck, n);
		if (n > 0) {
			data->Blck += n;
			if (data->Fill + data->Blck == data->Size) {
				n = -1;
			} else {
				if (data->Size >= 0) {
					WORD w = ((data->Fill + data->Blck) *256) / data->Size;
					if (w > PBAR_Rect(slot).g_w) {
						dr_x = PBAR_Rect(slot).g_w;
						dr_w = w - dr_x;
						PBAR_Rect(slot).g_w = w;
						draw = slot->Pbar;
					}
				} else {
					if (PBAR_Rect(slot).g_x < 232) {
						dr_x = PBAR_Rect(slot).g_x++;
						dr_w = 25;
					} else {
						PBAR_Rect(slot).g_x = 0;
					}
					draw = dlm_form[slot->Pbar].ob_next;
				}
				save |= data->Blck > BUF_SIZE /2;
			}
		}
		if (n < 0) {
			inet_close (data->Source);
			data->Source = -1;
			save = (data->Blck > 0);
		}
	} else { /* data->Source < 0 */
		save = (data->Blck > 0);
	}
	if (save && data->Chnk >= 0) {
		size_t len = (data->Chnk && data->Chnk < data->Blck
		              ? data->Chnk : data->Blck);
		write (data->Target, data->Buffer, len);
		data->Fill += len;
		data->Blck -= len;
		if (data->Blck) {
			memmove (data->Buffer, data->Buffer + len, data->Blck);
		}
		if (data->Chnk && !(data->Chnk -= len)) {
			data->Chnk = -1; /* next chunk header to be read */
		}
	}
	if (data->Source < 0 && data->Blck <= 0) {
		close (data->Target);
		data->Target = -1;
		free (data->Buffer);
		data->Buffer = NULL;
		
		if (data->Size < 0 && data->Chnk == 0) {
			data->Size = data->Fill;
			sprintf (INFO_Strng(slot), "%li bytes", data->Size);
			PBAR_Rect(slot).g_x = 0;
		}
		if (data->Size == data->Fill) {
			BTTN_Strng(slot)    = bttn_text_ok;
			PBAR_Fpatt(slot)    = IP_4PATT;
			PBAR_Rect(slot).g_w = 256;
		} else {
			slot_error (slot, "Connection broken!");
		}
		draw = slot->Base;
	}
	if (draw) {
		GRECT clip;
		objc_offset (dlm_form, draw, &clip.g_x, &clip.g_y);
		if (dr_w) {
			clip.g_x += dr_x;
			clip.g_w =  dr_w;
		} else {
			clip.g_w = dlm_form[draw].ob_width;
		}
		clip.g_h = dlm_form[draw].ob_height;
		window_redraw (dlm_wind, &clip);
	}
	if (data->Target < 0) {
		window_raise (dlm_wind, TRUE, NULL);
		return FALSE;
	}
	return TRUE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
fsel_job (void * arg, long invalidated)
{
	SLOT slot = slot_byArg (arg);
	SDAT data = &slot->Data;
	char fsel_file[HW_PATH_MAX];
	char patt[8] = ""; /* enough for "*.xhtml" */
	size_t len;
	
	if (invalidated) {
		return FALSE;
	}
	
	if ((len = strlen (data->Location->File)) > 0) {
		const char * p = memchr (data->Location->File, '?', len);
		if (p) len = (WORD)(p - data->Location->File);
	}
	if (len) {
		memcpy (fsel_file, data->Location->File, len);
		if (len >= 3) {
			char * src = fsel_file + len;
			short  anz = 0;
			do if (--src <= fsel_file) {
				anz = 0;
			} else if (*src == '.') {
				break;
			} else if (*src == '\\' || *src == '/' || ++anz > sizeof(patt)-3) {
				anz = 0;
			} while (anz);
			if (anz) {
				char * dst = patt;
				if (anz > 3 && !sys_XAAES()) anz = 3;
				*(dst++) = '*';
				*(dst++) = '.';
				while (anz--) *(dst++) = toupper (*(++src));
				*dst = '\0';
			}
		}
	}
	fsel_file[len] = '\0';
	
	if (file_selector ("Save Target as...",
	    (*patt ? patt : NULL), fsel_file, fsel_file, sizeof(fsel_file))) {
		data->Target = open (fsel_file, O_WRONLY|O_CREAT|O_TRUNC|O_RAW, 0666);
		if (data->Target < 0) {
			hwUi_warn ("Download", "Cannot create target file.");
		} else {
			SCHED_FUNC job = (PROTO_isRemote (data->Location->Proto)
			                  ? recv_job : load_job);
			sched_insert (job, arg, (long)arg, 20/*PRIO_RECIVE*/);
		}
	} else {
		slot_remove (slot);
	}
	slot_redraw (slot, FALSE);
	
	return FALSE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
conn_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	SLOT   slot   = slot_byArg (loader->FreeArg);
	SDAT   data   = &slot->Data;
	
	if (invalidated) {
		delete_loader (&loader);
		return FALSE;
	}
	
	if (loader->Error) {
		char * p = loader->Data;
		if (p) {
			if (strncmp (p, "HTTP", 4) == 0) {
				while (*(++p) && !isspace(*p));
				while (*(p) && isspace(*(++p)));
				if (!isdigit(*p)) p = loader->Data;
			} else {
				while (isspace(*p)) p++;
			}
			if (*p) {
				char * q = p;
				do if (*q == '\r' || *q == '\n') {
					*q = '\0';
					break;
				} while (*(++q));
			}
			if (!*p) p = NULL;
		}
		data->Size = -1;
		slot_error (slot, (p ? p : "Connection Error!"));
	
	} else {
		PBAR_Flags(slot)   &= ~OF_HIDETREE;
		PBAR_Rect(slot).g_x = 0;
		if ((data->Size = loader->DataSize) < 0) {
			PBAR_Rect(slot).g_w = 24;
			strcpy (INFO_Strng(slot), "size unknown");
		} else {
			PBAR_Rect(slot).g_w = 1;
			sprintf (INFO_Strng(slot), "%li bytes", data->Size);
		}
		INFO_Adjst(slot) = TE_RIGHT;
		if (data->Location != loader->Location) {
			free_location (&data->Location);
			data->Location = location_share (loader->Location);
		}
		if (loader->rdTlen) {
			memcpy (data->Buffer, loader->rdTemp, loader->rdTlen);
		}
		data->Chnk       = (loader->rdChunked ? -1 : 0);
		data->Blck       = loader->rdTlen;
		data->Fill       = 0;
		data->Source     = loader->rdSocket;
		loader->rdSocket = -1;
		arg = slot_Arg(slot);
		sched_insert (fsel_job, arg, (long)arg, 20/*PRIO_RECIVE*/);
	}
	slot_redraw (slot, TRUE);
	window_raise  (dlm_wind, TRUE, NULL);
	
	delete_loader (&loader);
	
	return FALSE;
}


/*******************************************************************************
 *
 * Window handling.
*/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
dlmngr_handler (OBJECT * tree, WORD obj)
{
	(void)tree;
	
	if (obj < 0) { /* window close notification from parent class */
		if (slot_used (slot_tab)) {
			SLOT slot = slot_tab;
			do if (slot->Data.Source < 0) { /* not in use anymore */
				slot_remove (slot);
				slot_redraw (slot, FALSE);
			} else { /* check the next slot */
				slot++;
			} while (slot <= slot_end && slot_used (slot));
		}
		if (!slot_used (slot_tab)) {
			dlm_wind = NULL; /* to be deleted */
		}
		return (dlm_wind == NULL);;
	
	} else { /* some of the buttons got pressed */
		SLOT slot = NULL;
		switch (obj) {
			case DLM_1BTN: slot = &slot_tab[0]; break;
			case DLM_2BTN: slot = &slot_tab[1]; break;
			case DLM_3BTN: slot = &slot_tab[2]; break;
			case DLM_4BTN: slot = &slot_tab[3]; break;
		}
		if (slot) {
			sched_clear ((long)slot_Arg(slot));
			slot_remove (slot);
			slot_redraw (slot, FALSE);
			if (!slot_used (slot_tab)) {
				free ((*dlm_wind->destruct)(dlm_wind));
				dlm_wind = NULL;
			}
		} else {
			printf ("dlmngr_handler: %i\n", obj);
		}
	}
	return FALSE;
}


/*============================================================================*/
void
dl_manager (LOCATION loc, LOCATION ref)
{
	SLOT slot;
	
	if (!dlm_wind) {
		short n;
		if (!dlm_form) {
			rsrc_gaddr (R_TREE, DLMNGR, &dlm_form);
			form_center (dlm_form, &n,&n,&n,&n);
			
			/* store button texts */
			bttn_text_ok     = BTTN_Strng (&slot_tab[0]);
			bttn_text_stop   = BTTN_Strng (&slot_tab[1]);
			bttn_text_cancel = BTTN_Strng (&slot_tab[2]);
			
			for (n = 0; n < numberof(slot_tab); n++) {
				
				/* determine maximum string length for file/url */
				file_text_max = min (file_text_max, FILE_StrLn(&slot_tab[n]));
				
				/* no slots in use yet */
				slot_hide (&slot_tab[n]);
			}
		}
		n = formwind_do (dlm_form, 0, "Download Manager", FALSE, dlmngr_handler);
		dlm_wind = window_byHandle (n);
		if (!dlm_wind) return;  /* something went wrong */
		dlm_wind->Ident = WINDOW_IDENT('D','M','G','R');
	} else {
		window_raise (dlm_wind, TRUE, NULL);
	}
	
	if ((slot = slot_find()) == NULL) {
		const char str_head[] = "[1][HighWire DownloadManager:"
		                           "|========================="
		                           "|No more slot free.]"
		                        "[cancel";
		const char str_tdup[] = "|tidy up]";
		char       form[sizeof(str_head) + sizeof(str_tdup)];
		short cnt = 0;
		slot = slot_tab;
		do if (BTTN_Strng(slot) == bttn_text_ok ||
			    BTTN_Strng(slot) == bttn_text_cancel) {
			cnt++;
		} while (++slot <= slot_end);
		window_redraw (dlm_wind, NULL);
		strcpy (form, str_head);
		if (cnt) strcat (form, str_tdup);
		else     strcat (form, "]");
		if (form_alert ((!cnt ? 1 : 2), form) == 1) {
			cnt = 0;
		} else {   /* clean up finished/broken slots */
			slot = slot_tab;
			do if (BTTN_Strng(slot) != bttn_text_stop) {
				slot_remove (slot);
				slot_redraw (slot, FALSE);
			} else {
				slot++;
			} while (slot <= slot_end && slot_used(slot));
		}
		if (!cnt || (slot = slot_find()) == NULL) {
			return;
		}
	}
	
	if ((slot->Data.Buffer = malloc (BUF_SIZE)) == NULL) {
		hwUi_warn ("DownloadManager", "Not enough memory left!");
		slot_hide (slot);
		return;
	}
	
	slot->Data.Location = location_share (loc);
	slot_setfile (slot, loc);
	
	if (PROTO_isRemote (loc->Proto)) {
		LOADER ldr = start_objc_load (NULL, NULL, loc, conn_job, slot_Arg(slot));
		if (ldr) {
			const char t_msk[] = "Connecting: %.*s";
			sprintf (INFO_Strng(slot), t_msk,
			         (int)(INFO_StrLn(slot) -1 -(sizeof(t_msk) -5)),
			         location_Host (loc, NULL));
			BTTN_Strng(slot) = bttn_text_stop;
			ldr->Referer = location_share (ref);
		} else {
			BTTN_Strng(slot) = bttn_text_cancel;
			slot_error (slot, "Internal Error!");
		}
	
	} else { /* local file */
		if ((slot->Data.Size = file_size (loc)) < 0) {
			BTTN_Strng(slot) = bttn_text_cancel;
			slot_error (slot, "File does not exist!");
		} else {
			char buf[2048];
			location_FullName (loc, buf, sizeof(buf));
			if ((slot->Data.Source = open (buf, O_RDONLY|O_RAW)) < 0) {
				BTTN_Strng(slot) = bttn_text_cancel;
				slot_error (slot, "Cannot read file!");
			} else {
				void * arg = slot_Arg(slot);
				PBAR_Flags(slot)   &= ~OF_HIDETREE;
				PBAR_Rect(slot).g_x = 0;
				PBAR_Rect(slot).g_w = 1;
				INFO_Adjst(slot)    = TE_RIGHT;
				BTTN_Strng(slot)    = bttn_text_stop;
				sprintf (INFO_Strng(slot), "%li bytes", slot->Data.Size);
				sched_insert (fsel_job, arg, (long)arg, 20/*PRIO_RECIVE*/);
			}
		}
	}
	window_redraw (dlm_wind, NULL);
}
