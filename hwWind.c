#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <gem.h>

#ifdef __PUREC__
# define CONTAINR struct s_containr *
# define HISTORY  struct s_history *
#endif

#include "global.h"
#include "Containr.h"
#include "hwWind.h"
#include "Loader.h"
#include "Logging.h"


#define HISTORY_LAST 20


static WORD  inc_xy = 0;
static BOOL  bevent;
static GRECT desk_area;
static GRECT curr_area;

WORD   hwWind_Mshape = ARROW;
HwWIND hwWind_Top    = NULL;
HwWIND hwWind_Focus  = NULL;


static void set_size (HwWIND, const GRECT *);
static void wnd_hdlr (HW_EVENT, long, CONTAINR, const void *);


/*============================================================================*/
HwWIND
new_hwWind (const char * name, const char * info, const char * url)
{
	HwWIND This = malloc (sizeof (struct hw_window) +
	                      sizeof (HISTORY) * HISTORY_LAST);
#if (_HIGHWIRE_INFOLINE_==TRUE)
	WORD   kind = NAME|INFO|CLOSER|FULLER|MOVER|SMALLER|HSLIDE|SIZER;
#else
	WORD   kind = NAME|CLOSER|FULLER|MOVER|SMALLER|HSLIDE|SIZER;
#endif	
	short  i;
	
	This->Next = hwWind_Top;
	hwWind_Top = This;
	
	if (!inc_xy) {
		WORD out, u;
		bevent = (appl_xgetinfo(AES_WINDOW, &out, &u,&u,&u) && (out & 0x20));
		wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &desk_area);
		wind_calc_grect (WC_BORDER, VSLIDE|HSLIDE, &desk_area, &curr_area);
		curr_area.g_w -= desk_area.g_w;
		curr_area.g_h -= desk_area.g_h;
		inc_xy = (curr_area.g_w >= curr_area.g_h
		          ? curr_area.g_w : curr_area.g_h) -1;
		if (desk_area.g_w < 800) {
			curr_area = desk_area;
			curr_area.g_w = (desk_area.g_w *2) /3;
		} else {
			curr_area.g_x = desk_area.g_x + inc_xy;
			curr_area.g_y = desk_area.g_y + inc_xy;
			curr_area.g_w = (desk_area.g_w *3) /4;
			curr_area.g_h = (desk_area.g_h *3) /4;
		}
	
	} else {
		curr_area.g_x += inc_xy;
		if (curr_area.g_x + curr_area.g_w > desk_area.g_x + desk_area.g_w) {
			curr_area.g_x = desk_area.g_x;
		}
		curr_area.g_y += inc_xy;
		if (curr_area.g_y + curr_area.g_h > desk_area.g_y + desk_area.g_h) {
			curr_area.g_y = desk_area.g_y;
		}
	}
	
	This->Handle  = wind_create_grect (kind, &desk_area);
	This->isFull  = FALSE;
	This->isIcon  = FALSE;
	This->isBusy  = 0;
	This->loading = 0;
	This->Pane    = new_containr (NULL);
	This->Active  = NULL;
	containr_register (This->Pane, wnd_hdlr, (long)This);
	
	This->HistUsed = 0;
	This->HistMenu = 0;
	for (i = 0; i <= HISTORY_LAST; This->History[i++] = NULL);
	
	set_size (This, &curr_area);
	hwWind_setName (This, name);
#if (_HIGHWIRE_INFOLINE_==TRUE)
	This->Info[0] = '\0';
	hwWind_setInfo (This, info, TRUE);
#endif

	wind_set(This->Handle,WF_HSLSIZE,1000,0,0,0);
	
	if (bevent) {
		wind_set (This->Handle, WF_BEVENT, 0x0001, 0,0,0);
	}
	wind_open_grect (This->Handle, &This->Curr);
	
	if (url && *url) {
		new_loader_job (url, NULL, This->Pane, ENCODING_WINDOWS1252, -1,-1);
	}
	
#ifdef GEM_MENU
	menu_history (NULL,0,0);
#endif
	
	return This;
}

/*============================================================================*/
void
delete_hwWind (HwWIND This)
{
	HwWIND * ptr = &hwWind_Top;
	short    i;
	
#ifdef GEM_MENU
	if (This == hwWind_Top) {
		if (This->Next) {
			HwWIND next = This->Next;
			menu_history (next->History, next->HistUsed, next->HistMenu);
		} else {
			menu_history (NULL,0,0);
		}
	}
#endif
	for (i = 0; i < This->HistUsed; history_destroy (&This->History[i++]));
	
	while (*ptr) {
		if (*ptr == This) {
			*ptr = This->Next;
			break;
		}
		ptr = &(*ptr)->Next;
	}
	delete_containr ((CONTAINR*)&This->Pane);

	wind_close  (This->Handle);
	wind_delete (This->Handle);
	
	if (hwWind_Focus == This) {
		hwWind_Focus = NULL;
	}
	free (This);
}


/*============================================================================*/
void
hwWind_setName (HwWIND This, const char * name)
{
	if (name && *name) {
		strncpy (This->Name, name, sizeof(This->Name));
		This->Name[sizeof(This->Name)-1] = '\0';
	} else {
		This->Name[0] = '\0';
	}
	wind_set_str (This->Handle, WF_NAME, This->Name);
}

/*============================================================================*/
#if (_HIGHWIRE_INFOLINE_==TRUE)
void
hwWind_setInfo (HwWIND This, const char * info, BOOL statNinfo)
{
	if (statNinfo) {
		if (info && *info) {
			strncpy (This->Stat, info, sizeof(This->Stat));
			This->Stat[sizeof(This->Stat)-1] = '\0';
			info = This->Stat;
		} else {
			This->Stat[0] = '\0';
			info = This->Info;
		}
	
	} else {
		if (info && *info) {
			strncpy (This->Info, info, sizeof(This->Info));
			This->Info[sizeof(This->Info)-1] = '\0';
			info = This->Info;
		} else {
			This->Info[0] = '\0';
			info = This->Stat;
		}
	}
	wind_set_str (This->Handle, WF_INFO, info);
}
#endif

/*----------------------------------------------------------------------------*/
static
void set_size (HwWIND This, const GRECT * curr)
{
	wind_set_grect (This->Handle, WF_CURRXYWH, curr);
	if (!wind_get_grect (This->Handle, WF_CURRXYWH, &This->Curr)
	    || This->Curr.g_w <= 0 || This->Curr.g_h <= 0) {
		This->Curr = *curr;    /* avoid a Geneva-bug where curr becomes 0,0,0,0 */
	}                         /* if the window was created but not opend       */
	wind_get_grect (This->Handle, WF_WORKXYWH, &This->Work);
	containr_calculate (This->Pane, &This->Work);
}

/*============================================================================*/
void
hwWind_move (HwWIND This, PXY pos)
{
	if (This->isIcon) {
		GRECT curr;
		wind_get_grect (This->Handle, WF_CURRXYWH, &curr);
		*(PXY*)&curr = pos;
		wind_set_grect (This->Handle, WF_CURRXYWH, &curr);
	} else {
		*(PXY*)&This->Curr = pos;
		wind_set_grect (This->Handle, WF_CURRXYWH, &This->Curr);
		wind_get_grect (This->Handle, WF_WORKXYWH, &This->Work);
		This->isFull = FALSE;
		containr_relocate (This->Pane, This->Work.g_x, This->Work.g_y);
	}
}

/*============================================================================*/
void
hwWind_resize (HwWIND This, const GRECT * curr)
{
	if (!This->isIcon) {
		GRECT prev = This->Curr;
		set_size (This, curr);
		curr_area.g_w = This->Work.g_w;
		curr_area.g_h = This->Work.g_h;
		This->isFull = FALSE;
		hwWind_redraw (This, &prev);
	}
}

/*============================================================================*/
void
hwWind_full (HwWIND This)
{
	if (!This->isIcon) {
		WORD  mode = (This->isFull ? WF_PREVXYWH : WF_FULLXYWH);
		GRECT prev = This->Curr;
		GRECT curr;
		wind_get_grect (This->Handle, mode, &curr);
		set_size (This, &curr);
		This->isFull = (mode == WF_FULLXYWH);
		if (prev.g_x == This->Curr.g_x && prev.g_y == This->Curr.g_y) {
			hwWind_redraw (This, &prev);
		}
	}
}

/*============================================================================*/
void
hwWind_iconify (HwWIND This, const GRECT * icon)
{
	if (icon) {
		if (!This->isIcon) {
			if (This->isFull) {
				wind_get_grect (This->Handle, WF_PREVXYWH, &This->Curr);
			}
			wind_set_grect (This->Handle, WF_ICONIFY, icon);
			This->isIcon = TRUE;
		}
	} else {
		if (This->isIcon) {
			wind_set_grect (This->Handle, WF_UNICONIFY, &This->Curr);
			if (This->isFull) {
				wind_get_grect (This->Handle, WF_FULLXYWH, &This->Curr);
				wind_set_grect (This->Handle, WF_CURRXYWH, &This->Curr);
			}
			This->isIcon = FALSE;
		}
	}
}

/*============================================================================*/
void
hwWind_raise (HwWIND This, BOOL topNbot)
{
	HwWIND new_top = NULL;
	
	if (topNbot) {
		if (This != hwWind_Top) {
			HwWIND * ptr = &hwWind_Top;
			new_top = This;
			while (*ptr) {
				if (*ptr == This) {
					*ptr       = This->Next;
					This->Next = hwWind_Top;
					hwWind_Top = This;
					break;
				}
				ptr = &(*ptr)->Next;
			}
		}
		wind_set (This->Handle, WF_TOP, 0,0,0,0);
		
	} else {
		if (This->Next) {
			HwWIND * ptr = &hwWind_Top;
			if (This == hwWind_Top) {
				new_top = This->Next;
			}
			while (*ptr) {
				if (*ptr == This) {
					*ptr = This->Next;
					break;
				}
				ptr = &(*ptr)->Next;
			}
			while (*ptr) {
				ptr = &(*ptr)->Next;
			}
			*ptr       = This;
			This->Next = NULL;
		}
		wind_set (This->Handle, WF_BOTTOM, 0,0,0,0);
	}
	
#ifdef GEM_MENU
	if (new_top) {
		menu_history (new_top->History, new_top->HistUsed, new_top->HistMenu);
	}
#endif
}

/*============================================================================*/
void
hwWind_scroll (HwWIND This, CONTAINR cont, long dx, long dy)
{
	GRECT work = This->Work;
	
	if (cont->Base != This->Pane) {
		printf ("hwWind_scroll() ERROR: container not in window!\n");
		return;
	}
	
	if (containr_shift (cont, &dx, &dy)
	    && rc_intersect (&desk_area,  &work)
	    && rc_intersect (&cont->Area, &work)) {
		GRECT clip;
		wind_update (BEG_UPDATE);
		wind_get_grect (This->Handle, WF_FIRSTXYWH, &clip);
		while (clip.g_w > 0 && clip.g_h > 0) {
			if (rc_intersect (&work, &clip)) {
				containr_scroll (cont, &clip, dx, dy);
			}
			wind_get_grect (This->Handle, WF_NEXTXYWH, &clip);
		}
		wind_update (END_UPDATE);
	}
}


/*----------------------------------------------------------------------------*/
static void
hist_append (HwWIND This, CONTAINR sub)
{
	WORD prev = This->HistMenu;
	WORD menu = prev +1;
	
	if (!This->HistUsed) {
		prev = -1;
		menu = 0;
	
	} else if (menu < This->HistUsed) {
		do {
			history_destroy (&This->History[--This->HistUsed]);
		} while (menu < This->HistUsed);
		if (prev > menu) {
			prev = -1;
		}
	
	} else if (menu > HISTORY_LAST) {
		short i;
		history_destroy (&This->History[0]);
		for (i = 0; i < HISTORY_LAST; i++) {
			This->History[i] = This->History[i +1];
		}
		menu = This->HistUsed = HISTORY_LAST;
		prev--;
	}
	
	if (sub) {
		This->History[menu] = history_create (sub, This->Stat,
		                                 (prev < 0 ? NULL : This->History[prev]));
	} else {
		This->History[menu] = history_create (This->Pane, This->Name, NULL);
	}
	if (prev >= 0) {
		This->History[prev]->Text[0] = ' ';
	}
	This->History[menu]->Text[0] = ' ';
	This->HistMenu = menu;
	This->HistUsed++;
	
#ifdef GEM_MENU
	if (This == hwWind_Top) {
		menu_history (This->History, This->HistUsed, menu);
	}
#endif
}

/*============================================================================*/
void
hwWind_history (HwWIND This, UWORD menu)
{
	if (menu < This->HistUsed) {
		HISTENTR entr[100];
		UWORD i;
		UWORD num = containr_process (This->Pane, This->History[menu],
		                              This->History[This->HistMenu],
		                              entr, numberof(entr));
		
		This->History[menu]->Text[0] = '*';
		if (This->HistMenu != menu) {
#ifdef GEM_MENU
			if (This == hwWind_Top) {
				menu_history (This->History, This->HistUsed, -1);
			}
#endif
			This->HistMenu = menu;
		}
		for (i = 0; i < num; i++) {
			new_loader_job (NULL, entr[i].Location, entr[i].Target,
			                entr->Encoding, -1, -1);
		}
	}
}

/*============================================================================*/
void
hwWind_undo (HwWIND This, BOOL redo)
{
	if (redo) {
		hwWind_history (This, This->HistMenu +1);
	} else if (This->HistMenu) {
		hwWind_history (This, This->HistMenu -1);
	}
}


/*============================================================================*/
HwWIND
hwWind_byHandle (WORD hdl)
{
	HwWIND wind = hwWind_Top;
	while (wind) {
		if (wind->Handle == hdl) {
			break;
		}
		wind = wind->Next;
	}
	return wind;
}

/*============================================================================*/
HwWIND
hwWind_byValue (long val)
{
	HwWIND wind = hwWind_Top;
	while (wind) {
		if (wind == (HwWIND)val) {
			break;
		}
		wind = wind->Next;
	}
	return wind;
}


/*============================================================================*/
void
hwWind_redraw (HwWIND This, const GRECT * clip)
{
	GRECT work, rect = desk_area;
	
	wind_update (BEG_UPDATE);
	
	if (clip && !rc_intersect (clip, &rect)) {
		rect.g_w = rect.g_h = 0;
	}
	
	if (!This->isIcon) {
		work = This->Work;
		if (!rc_intersect (&rect, &work)) {
			rect.g_w = rect.g_h = 0;
		} else {
			wind_get_grect (This->Handle, WF_FIRSTXYWH, &rect);
		}
		while (rect.g_w > 0 && rect.g_h > 0) {
			if (rc_intersect (&work, &rect)) {
				containr_redraw (This->Pane, &rect);
			}
			wind_get_grect (This->Handle, WF_NEXTXYWH, &rect);
		}
	
	} else /* This->isIcon */ {
		static UWORD data[] = {
			0x000c,0x0000, 0x0003,0xef00, 0x001f,0x7600, 0x0031,0xfb80,
			0x0031,0xf080, 0x0006,0x70c0, 0x00ff,0xec00, 0x0087,0x7fc0,
			0x0181,0xe0e0, 0x0001,0xf020, 0x005f,0xd860, 0x00ef,0x7f00,
			0x00c3,0xff80, 0x00c7,0xf080, 0x0007,0xf180, 0x0007,0xf800,
			0x000f,0xf000, 0x001f,0x7800, 0x001e,0xf800, 0x001d,0xec00,
			0x003e,0x7c00, 0x003d,0x7c00, 0x0078,0x2400, 0x00f8,0xfc00,
			0x00d8,0x2600, 0x00d0,0x7600, 0x01b1,0x2600, 0x01ff,0xfe00,
			0x0362,0x3200, 0x0260,0x3300, 0x0660,0x3300, 0x0460,0x3300
		};
		static MFDB icon = { data, 32,32, 2, 0, 1, 0,0,0 };
		
		MFDB  scrn = { NULL, };
		short color[2];
		PXY   lu;
		
		wind_get_grect (This->Handle, WF_WORKXYWH, &work);
		lu.p_x = work.g_x + (work.g_w - icon.fd_w) /2;
		lu.p_y = work.g_y + (work.g_h - icon.fd_h) /2;
		color[0] = (This->isBusy ? G_WHITE : G_BLACK);
		if (!rc_intersect (&rect, &work)) {
			rect.g_w = rect.g_h = 0;
		} else {
			wind_get_grect (This->Handle, WF_FIRSTXYWH, &rect);
		}
		vsf_color (vdi_handle, G_LWHITE);
		while (rect.g_w > 0 && rect.g_h > 0) {
			if (rc_intersect (&work, &rect)) {
				PXY p[4];
				p[1].p_x = (p[0].p_x = rect.g_x) + rect.g_w -1;
				p[1].p_y = (p[0].p_y = rect.g_y) + rect.g_h -1;
				v_hide_c (vdi_handle);
				v_bar (vdi_handle, (short*)p);
				p[2] = lu;
				p[3] = *(PXY*)&icon.fd_w;
				if (rc_intersect (&rect, (GRECT*)(p +2))) {
					p[1].p_x = (p[0].p_x = p[2].p_x - lu.p_x) + p[3].p_x -1;
					p[1].p_y = (p[0].p_y = p[2].p_y - lu.p_y) + p[3].p_y -1;
					p[3].p_x += p[2].p_x -1;
					p[3].p_y += p[2].p_y -1;
					vrt_cpyfm (vdi_handle, MD_TRANS, (short*)p, &icon, &scrn, color);
				}
				v_show_c (vdi_handle, 1);
			}
			wind_get_grect (This->Handle, WF_NEXTXYWH, &rect);
		}
	}
	
	wind_update (END_UPDATE);
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
wnd_hdlr (HW_EVENT event, long arg, CONTAINR cont, const void * gen_ptr)
{
	HwWIND wind = hwWind_byValue (arg);
	
	if (wind) switch (event) {
		
		case HW_PageCleared:
			if (wind->Active == cont) {
				wind->Active = NULL;
			}
			break;
		
		case HW_PageStarted:
			if (!wind->loading++ && wind->HistUsed) {
				char * flag = wind->History[wind->HistMenu]->Text;
				if (*flag == ' ' && cont->Parent) {
					*flag = '.';
				}
			}
			if (!wind->isBusy++) {
				if (wind->isIcon) {
					hwWind_redraw (wind, NULL); /* update icon */
				} else {
					graf_mouse (hwWind_Mshape = BUSYBEE, NULL);
				}
			}
		case HW_SetTitle:
			if (!cont->Parent) {
				hwWind_setName (wind, gen_ptr);
				break;
			}
		case HW_SetInfo:
#if (_HIGHWIRE_INFOLINE_==TRUE)
			hwWind_setInfo (wind, gen_ptr, TRUE);
#endif
			break;
		
		case HW_ImgBegLoad:
			if (!wind->isBusy++) {
				if (wind->isIcon) {
					hwWind_redraw (wind, NULL); /* update icon */
				} else {
					graf_mouse (hwWind_Mshape = BUSYBEE, NULL);
				}
			}
#if (_HIGHWIRE_INFOLINE_==TRUE)
			hwWind_setInfo (wind, gen_ptr, TRUE);
#endif
			break;
		
		case HW_PageFinished:
			if (wind->loading && !--wind->loading) {
				char flag = (!wind->HistUsed
				             ? ' ' : wind->History[wind->HistMenu]->Text[0]);
				if (flag != '*') {
					hist_append (wind, (flag == ' ' ? NULL : cont));
				} else {
					wind->History[wind->HistMenu]->Text[0] = ' ';
#ifdef GEM_MENU
					if (wind == hwWind_Top) {
						menu_history (wind->History, wind->HistUsed, wind->HistMenu);
					}
#endif
				}
				wind->Info[0] = '\0';
			}
			if (gen_ptr) {
				FRAME active = hwWind_setActive (wind, cont);
#ifdef GEM_MENU
#	if (_HIGHWIRE_ENCMENU_==TRUE)
				update_menu (active->Encoding,
				             (active->MimeType == MIME_TXT_PLAIN));
#	endif
#endif
			}
		case HW_ImgEndLoad:
#if (_HIGHWIRE_INFOLINE_==TRUE)
			hwWind_setInfo (wind, "", TRUE);
#endif
			if (wind->isBusy) {
				wind->isBusy--;
			}
			if (wind->isIcon) {
				if (!wind->isBusy) {
					hwWind_redraw  (wind, NULL); /* update icon */
				}
			} else {
				if (gen_ptr) {
					hwWind_redraw  (wind, gen_ptr);
				}
				if (!wind->isBusy) {
					short mx, my, u;
					graf_mkstate (&mx, &my, &u,&u);
					check_mouse_position (mx, my);
				}
			}
			break;
			
		default:
			errprintf ("wind_handler (%i, %p)\n", event, cont);
	}
}


/*============================================================================*/
FRAME
hwWind_setActive (HwWIND This, CONTAINR cont)
{
	FRAME active = NULL;
	
	if (This) {
		if (cont &&  This->Pane && cont->Base != This->Pane) {
			printf ("hwWind_setActive() ERROR: container not in window!\n");
			return NULL;
		}
		if (cont && (active = containr_Frame (cont)) != NULL) {
			This->Active = cont;
		} else {
			This->Active = NULL;
		}
	}
	return active;
}


/*============================================================================*/
FRAME
hwWind_ActiveFrame (HwWIND This)
{
	return ((This || (This = hwWind_Top) != NULL) && This->Active
	        ? containr_Frame (((CONTAINR)This->Active)) : NULL);
}

