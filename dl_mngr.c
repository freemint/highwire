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

static OBJECT * form = NULL;
static WINDOW window = NULL;


#define BUF_SIZE 8192ul

typedef struct s_slot {
	const struct {
		WORD Box, File, Text, Bar, Btn;
	} F;
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
	{ { DLM_1, DLM_1FILE, DLM_1TEXT, DLM_1BAR, DLM_1BTN }, },
	{ { DLM_2, DLM_2FILE, DLM_2TEXT, DLM_2BAR, DLM_2BTN }, },
	{ { DLM_3, DLM_3FILE, DLM_3TEXT, DLM_3BAR, DLM_3BTN }, },
	{ { DLM_4, DLM_4FILE, DLM_4TEXT, DLM_4BAR, DLM_4BTN }, }
};
static char * btn_text_ok     = "-";
static char * btn_text_stop   = "-";
static char * btn_text_cancel = "-";
static size_t file_text_max   = HW_PATH_MAX;

/*----------------------------------------------------------------------------*/
static SLOT
slot_find (void)
{
	SLOT slot = slot_tab;
	while (!(form[slot->F.Box].ob_flags & OF_HIDETREE)) {
		if (++slot >= &slot_tab[numberof(slot_tab)]) return NULL;
	}
	form[slot->F.Box].ob_flags &= ~OF_HIDETREE;
	form[slot->F.Btn].ob_state &= ~OS_SELECTED;
	form[slot->F.Text].ob_spec.tedinfo->te_just = TE_LEFT;
	form[slot->F.Bar].ob_spec.obspec.fillpattern = IP_4PATT;
	form[slot->F.Bar].ob_spec.obspec.interiorcol = G_GREEN;
	form[slot->F.Bar].ob_x     = 0;
	form[slot->F.Bar].ob_width = 1;
	slot->Data.Socket = -1;
	slot->Data.Target = -1;
	slot->Data.Buffer = NULL;
	slot->Data.Size   = -1;
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
	if (slot < &slot_tab[numberof(slot_tab)-1]) {
		SLOT next = slot +1;
		if (!(form[next->F.Box].ob_flags & OF_HIDETREE)) {
			OBSPEC spec_file = form[slot->F.File].ob_spec;
			OBSPEC spec_text = form[slot->F.Text].ob_spec;
			OBSPEC spec_btn  = form[slot->F.Btn].ob_spec;
			OBSPEC spec_bar  = form[slot->F.Bar].ob_spec;
			GRECT  rect_bar  = *(GRECT*)&form[slot->F.Bar].ob_x;
			do {
				form[slot->F.File].ob_spec = form[next->F.File].ob_spec;
				form[slot->F.Text].ob_spec = form[next->F.Text].ob_spec;
				form[slot->F.Btn].ob_spec  = form[next->F.Btn].ob_spec;
				form[slot->F.Btn].ob_state = form[next->F.Btn].ob_state;
				form[slot->F.Bar].ob_spec  = form[next->F.Bar].ob_spec;
				*(GRECT*)&form[slot->F.Bar].ob_x = *(GRECT*)&form[next->F.Bar].ob_x;
				slot->Data = next->Data;
				slot       = next;
			} while (++next < &slot_tab[numberof(slot_tab)]
			         && !(form[next->F.Box].ob_flags & OF_HIDETREE));
			form[slot->F.File].ob_spec = spec_file;
			form[slot->F.Text].ob_spec = spec_text;
			form[slot->F.Btn].ob_spec  = spec_btn;
			form[slot->F.Bar].ob_spec  = spec_bar;
			*(GRECT*)&form[slot->F.Bar].ob_x = rect_bar;
		}
	}	
	form[slot->F.Box].ob_flags |= OF_HIDETREE;
}

/*----------------------------------------------------------------------------*/
static SLOT
slot_byArg (void * arg)
{
	SLOT slot = slot_tab;
	while (slot->Data.Buffer != arg) {
		if (++slot >= &slot_tab[numberof(slot_tab)]) return NULL;
	}
	return slot;
}

/*----------------------------------------------------------------------------*/
static void
slot_error (SLOT slot, const char * text)
{
	strncpy (form[slot->F.Text].ob_spec.tedinfo->te_ptext, text,
	         form[slot->F.Text].ob_spec.tedinfo->te_txtlen -1);
	form[slot->F.Btn].ob_state           &= ~OS_DISABLED;
	form[slot->F.Btn].ob_spec.free_string = btn_text_cancel;
	form[slot->F.Bar].ob_spec.obspec.fillpattern = IP_SOLID;
	form[slot->F.Bar].ob_spec.obspec.interiorcol = G_RED;
	if (slot->Data.Size < 0) {
		form[slot->F.Bar].ob_x     = 0;
		form[slot->F.Bar].ob_width = 256;
	}
}

/*----------------------------------------------------------------------------*/
static void
slot_redraw (SLOT slot, BOOL only_this)
{
	GRECT clip;
	clip.g_x = form[ROOT].ob_x;
	clip.g_w = form[ROOT].ob_width;
	clip.g_y = form[ROOT].ob_y + form[slot->F.Box].ob_y;
	clip.g_h = form[only_this ? slot->F.Box : ROOT].ob_height;
	window_redraw (window, &clip);
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
recv_job (void * arg, long invalidated)
{
	SLOT slot = slot_byArg (arg);
	BOOL save = FALSE;
	WORD draw = 0;
	
	if (invalidated) {
		slot_remove (slot);
		slot_redraw (slot, FALSE);
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
					if (w != form[slot->F.Bar].ob_width) {
						form[slot->F.Bar].ob_width = w;
						draw = slot->F.Bar;
					}
				} else {
					if (form[slot->F.Bar].ob_x < 254) {
						form[slot->F.Bar].ob_x++;
					} else {
						form[slot->F.Bar].ob_x = 0;
					}
					draw = form[slot->F.Bar].ob_next;
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
			sprintf (form[slot->F.Text].ob_spec.tedinfo->te_ptext,
			         "%li bytes", slot->Data.Size);
			form[slot->F.Bar].ob_x = 0;
		}
		if (slot->Data.Size == slot->Data.Fill) {
			form[slot->F.Btn].ob_spec.free_string = btn_text_ok;
			form[slot->F.Bar].ob_spec.obspec.fillpattern = IP_SOLID;
			form[slot->F.Bar].ob_width                   = 256;
		} else {
			slot_error (slot, "Connection broken!");
		}
		draw = slot->F.Box;
	}
	if (draw) {
		GRECT clip;
		objc_offset (form, draw, &clip.g_x, &clip.g_y);
		clip.g_w = form[draw].ob_width;
		clip.g_h = form[draw].ob_height;
		window_redraw (window, &clip);
	}
	if (slot->Data.Socket < 0) {
		window_raise (window, TRUE, NULL);
		return FALSE;
	}
	return TRUE;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
fsel_job (void * arg, long invalidated)
{
	LOADER loader = arg;
	SLOT   slot   = loader->FreeArg;
	BOOL   ok;
	
	if (invalidated) {
		delete_loader (&loader);
		return FALSE;
	}
	
/*	printf ("fsel_job: %i -> %li / %li / %li\n",
	        loader->Error, loader->rdTlen, loader->DataFill, loader->DataSize);
*/	
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
		ok = FALSE;
	
	} else {
		if ((slot->Data.Size = loader->DataSize) < 0) {
			strcpy (form[slot->F.Text].ob_spec.tedinfo->te_ptext, "size unknown");
		} else {
			sprintf (form[slot->F.Text].ob_spec.tedinfo->te_ptext,
			         "%li bytes", slot->Data.Size);
		}
		form[slot->F.Text].ob_spec.tedinfo->te_just = TE_RIGHT;
		ok = TRUE;
	}
	slot_redraw (slot, TRUE);
	window_raise  (window, TRUE, NULL);
	
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
		sched_insert (recv_job, slot->Data.Buffer, (long)slot, 20/*PRIO_RECIVE*/);
		form[slot->F.Btn].ob_state &= ~OS_DISABLED;
	} else {
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
		if (form[slot_tab->F.Box].ob_flags & OF_HIDETREE) {
			puts ("schwupps");
			window = NULL; /* to be deleted */
		} else {
			window_raise (window, FALSE, NULL);
		}
		return (window == NULL);;
	
	} else {
		SLOT slot = NULL;
		switch (obj) {
			case DLM_1BTN: slot = &slot_tab[0]; break;
			case DLM_2BTN: slot = &slot_tab[1]; break;
			case DLM_3BTN: slot = &slot_tab[2]; break;
			case DLM_4BTN: slot = &slot_tab[3]; break;
		}
		if (slot) {
			if (!sched_clear ((long)slot)) {
				slot_remove (slot);
				slot_redraw (slot, FALSE);
			}
			if (form[slot_tab->F.Box].ob_flags & OF_HIDETREE) {
				free ((*window->destruct)(window));
				window = NULL;
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
	if (!window) {
		if (!form) {
			short dmy;
			rsrc_gaddr (R_TREE, DLMNGR, &form);
			form_center (form, &dmy,&dmy,&dmy,&dmy);
			form[DLM_1].ob_flags |= OF_HIDETREE;
			form[DLM_2].ob_flags |= OF_HIDETREE;
			form[DLM_3].ob_flags |= OF_HIDETREE;
			form[DLM_4].ob_flags |= OF_HIDETREE;
			file_text_max = min (file_text_max,
			                     strlen (form[DLM_1FILE].ob_spec.free_string));
			file_text_max = min (file_text_max,
			                     strlen (form[DLM_2FILE].ob_spec.free_string));
			file_text_max = min (file_text_max,
			                     strlen (form[DLM_3FILE].ob_spec.free_string));
			file_text_max = min (file_text_max,
			                     strlen (form[DLM_4FILE].ob_spec.free_string));
			btn_text_ok     = form[DLM_1BTN].ob_spec.free_string;
			btn_text_stop   = form[DLM_2BTN].ob_spec.free_string;
			btn_text_cancel = form[DLM_3BTN].ob_spec.free_string;
		}
		window = window_byHandle (formwind_do (form, ROOT, "Download Manager",
		                                       FALSE, dlmngr_handler));
	} else {
		window_raise (window, TRUE, NULL);
	}
	
	if (window) {
		const char * host    = location_Host (loc, NULL);
		const char   t_msk[] = "Connecting: %.*s";
		LOADER       ldr;
		
		SLOT slot = slot_find();
		if (!slot) {
			puts ("dl_manager(): no slot free");
			return;
		}
		form[slot->F.Btn].ob_state |= OS_DISABLED;
		form[slot->F.Btn].ob_spec.free_string = btn_text_stop;
		sprintf (form[slot->F.Text].ob_spec.tedinfo->te_ptext, t_msk,
		         (int)(form[slot->F.Text].ob_spec.tedinfo->te_txtlen -1
		               -(sizeof(t_msk) -5)), (host ? host : "(???)"));
		if (!host) {
			strcpy (form[slot->F.File].ob_spec.tedinfo->te_ptext, "???");
		} else {
			char buf[2048], * p = buf;
			size_t len = location_FullName (loc, buf, sizeof(buf));
			if (len > file_text_max) {
				p += len - file_text_max;
				*p = '®';
			}
			strcpy (form[slot->F.File].ob_spec.free_string, p);
		}
		slot_redraw (slot, TRUE);
		
		if ((ldr = start_objc_load (NULL, NULL, loc, fsel_job, slot)) != NULL) {
			ldr->Referer = location_share (ref);
		}
	}
}
