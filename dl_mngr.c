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
	struct {
		WORD   Socket;
		WORD   Target;
		char * Buffer;
		long   Size;
		long   Fill;
		long   Blck;
	} Data;
} * SLOT;
static struct s_slot slot_tab[4] = {
	{ DLM_1, DLM_1FILE, DLM_1TEXT, DLM_1BAR, DLM_1BTN, },
	{ DLM_2, DLM_2FILE, DLM_2TEXT, DLM_2BAR, DLM_2BTN, },
	{ DLM_3, DLM_3FILE, DLM_3TEXT, DLM_3BAR, DLM_3BTN, },
	{ DLM_4, DLM_4FILE, DLM_4TEXT, DLM_4BAR, DLM_4BTN, }
};
#define slot_end   (slot_tab + numberof(slot_tab) -1)

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

static char * bttn_text_ok     = "-";
static char * bttn_text_stop   = "-";
static char * bttn_text_cancel = "-";
static size_t file_text_max    = MAX_LEN; /* from gem.h */


/*----------------------------------------------------------------------------*/
#define slot_hide(slot)   {BASE_Flags(slot) |=  OF_HIDETREE;}
#define slot_view(slot)   {BASE_Flags(slot) &= ~OF_HIDETREE;}
#define slot_used(slot) (!(BASE_Flags(slot)  &  OF_HIDETREE))

/*----------------------------------------------------------------------------*/
static SLOT
slot_find (void)
{
	SLOT slot = slot_tab;
	while (!(BASE_Flags(slot) & OF_HIDETREE)) {
		if (++slot > slot_end) return NULL;
	}
	slot->Data.Socket = -1;
	slot->Data.Target = -1;
	slot->Data.Buffer = NULL;
	slot->Data.Size   = -1;
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
	if (slot->Data.Socket >= 0) {
		inet_close (slot->Data.Socket);
		slot->Data.Socket = -1;
	}
	if (slot->Data.Target >= 0) {
		close (slot->Data.Target);
		slot->Data.Target = -1;
	}
	if (slot->Data.Buffer) {
		free (slot->Data.Buffer);
		slot->Data.Buffer = NULL;
	}
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
	}
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


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
recv_job (void * arg, long invalidated)
{
	SLOT slot = slot_byArg (arg);
	BOOL save = FALSE;
	WORD draw = 0, dr_x = 0, dr_w = 0;
	
	if (invalidated) {
		return FALSE;
	}
	
	if (slot->Data.Socket >= 0) {
		long n = BUF_SIZE - slot->Data.Blck;
		if (slot->Data.Size > 0 && n > slot->Data.Size - slot->Data.Fill) {
			n = slot->Data.Size - slot->Data.Fill;
		}
		n = inet_recv (slot->Data.Socket, slot->Data.Buffer + slot->Data.Blck, n);
		if (n > 0) {
			slot->Data.Blck += n;
			if (slot->Data.Fill + slot->Data.Blck == slot->Data.Size) {
				n = -1;
			} else {
				if (slot->Data.Size >= 0) {
					WORD w = ((slot->Data.Fill + slot->Data.Blck) *256)
					       / slot->Data.Size;
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
				save = (slot->Data.Blck > BUF_SIZE /2);
			}
		}
		if (n < 0) {
			inet_close (slot->Data.Socket);
			slot->Data.Socket = -1;
			save = (slot->Data.Blck > 0);
		}
	} else {
		save = (slot->Data.Blck > 0);
	}
	if (save) {
		write (slot->Data.Target, slot->Data.Buffer, slot->Data.Blck);
		slot->Data.Fill += slot->Data.Blck;
		slot->Data.Blck =  0;
	}
	if (slot->Data.Socket < 0) {
		close (slot->Data.Target);
		slot->Data.Target = -1;
		free (slot->Data.Buffer);
		slot->Data.Buffer = NULL;
		
		if (slot->Data.Size < 0) {
			slot->Data.Size = slot->Data.Fill;
			sprintf (INFO_Strng(slot), "%li bytes", slot->Data.Size);
			PBAR_Rect(slot).g_x = 0;
		}
		if (slot->Data.Size == slot->Data.Fill) {
			BTTN_Strng(slot)    = bttn_text_ok;
			PBAR_Fpatt(slot)    = IP_4PATT;
			PBAR_Rect(slot).g_w = 256;
		} else {
			slot_error (slot, "Connection broken!");
		}
/*		dlm_form[slot->Info].ob_spec.tedinfo->te_color |= G_LBLACK <<8;*/
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
	if (slot->Data.Socket < 0) {
		window_raise (dlm_wind, TRUE, NULL);
		return FALSE;
	}
	return TRUE;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
fsel_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	SLOT   slot   = slot_byArg (loader->FreeArg);
	BOOL   ok, err;
	
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
		slot_error (slot, (p ? p : "Connection Error!"));
		ok  = FALSE;
		err = TRUE;
	
	} else {
		PBAR_Flags(slot)   &= ~OF_HIDETREE;
		PBAR_Rect(slot).g_x = 0;
		if ((slot->Data.Size = loader->DataSize) < 0) {
			PBAR_Rect(slot).g_w = 24;
			strcpy (INFO_Strng(slot), "size unknown");
		} else {
			PBAR_Rect(slot).g_w = 1;
			sprintf (INFO_Strng(slot), "%li bytes", slot->Data.Size);
		}
		INFO_Adjst(slot) = TE_RIGHT;
		ok  = TRUE;
		err = FALSE;
	}
	slot_redraw (slot, TRUE);
	window_raise  (dlm_wind, TRUE, NULL);
	
	while (ok) {
		char fsel_file[HW_PATH_MAX];
		WORD butt = -1;
		WORD ret  = strlen (loader->Location->File);
		
		if (ret) {
			const char * p = memchr (loader->Location->File, '?', ret);
			if (p) ret = (WORD)(p - loader->Location->File);
		}
		if (ret) {
			memcpy (fsel_file, loader->Location->File, ret);
		}
		fsel_file[ret] = '\0';
		
		if ((gl_ap_version >= 0x140 && gl_ap_version < 0x200)
		    || gl_ap_version >= 0x300 /* || getcookie(FSEL) */) {
			ret = fsel_exinput (fsel_path, fsel_file, &butt, "Save Target as...");
		} else {
			ret = fsel_input (fsel_path, fsel_file, &butt);
		}
		if (!ret || butt != FSEL_OK || !*fsel_file) {
			ok = FALSE;
		} else {
			char * p = strrchr (fsel_path, '\\');
			long   n = p - fsel_path +1;
			if (n > 0) {
				memmove (fsel_file + n, fsel_file, strlen (fsel_file) +1);
				memcpy  (fsel_file,     fsel_path, n);
			} else {
				ok = FALSE;
			}
		}
		if (ok) {
			slot->Data.Target = open (fsel_file,
			                          O_WRONLY|O_CREAT|O_TRUNC|O_RAW, 0666);
			if (slot->Data.Target < 0) {
				hwUi_warn ("Download", "Cannot create target file.");
			} else if ((slot->Data.Buffer = malloc (BUF_SIZE)) == NULL) {
				hwUi_warn ("Download", "Not enough memory left!");
				ok = FALSE;
			} else {
				if (loader->rdTlen) {
					memcpy (slot->Data.Buffer, loader->rdTemp, loader->rdTlen);
				}
				slot->Data.Blck   = loader->rdTlen;
				slot->Data.Fill   = 0;
				slot->Data.Socket = loader->rdSocket;
				loader->rdSocket  = -1;
				break;
			}
		}
	}
	if (ok) {
		arg = slot_Arg(slot);
		sched_insert (recv_job, arg, (long)arg, 20/*PRIO_RECIVE*/);
	} else if (!err) {
		slot_remove (slot);
	}
	slot_redraw (slot, FALSE);
	
	delete_loader (&loader);
	
	return FALSE;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
dlmngr_handler (OBJECT * tree, WORD obj)
{
	(void)tree;
	
	if (obj < 0) {
		if (!slot_used (slot_tab)) {
			dlm_wind = NULL; /* to be deleted */
		} else {
			window_raise (dlm_wind, FALSE, NULL);
		}
		return (dlm_wind == NULL);;
	
	} else {
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
	} else {
		window_raise (dlm_wind, TRUE, NULL);
	}
	
	if (dlm_wind) {
		const char * host    = location_Host (loc, NULL);
		const char   t_msk[] = "Connecting: %.*s";
		LOADER       ldr;
		
		SLOT slot = slot_find();
		if (!slot) {
			puts ("dl_manager(): no slot free");
			return;
		}
		BTTN_Strng(slot) = bttn_text_stop;
		sprintf (INFO_Strng(slot), t_msk,
		         (int)(INFO_StrLn(slot) -1 -(sizeof(t_msk) -5)),
		         (host ? host : "(???)"));
		if (!host) {
			strcpy (FILE_Strng(slot), "???");
		} else {
			char buf[2048], * p = buf;
			size_t len = location_FullName (loc, buf, sizeof(buf));
			if (len > file_text_max) {
				p += len - file_text_max;
				*p = '®';
			}
			strcpy (FILE_Strng(slot), p);
		}
		window_redraw (dlm_wind, NULL);
		
		ldr = start_objc_load (NULL, NULL, loc, fsel_job, slot_Arg(slot));
		if (ldr) {
			ldr->Referer = location_share (ref);
		}
	}
}
