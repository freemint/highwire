#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <gem.h>

#include <gemx.h>

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


static WORD  info_fgnd = G_BLACK, info_bgnd = G_WHITE;
static WORD  inc_xy = 0;
static WORD  widget_b, widget_w, widget_h;
static BOOL  bevent;
static GRECT desk_area;
static GRECT curr_area;
#if (_HIGHWIRE_INFOLINE_ == 0)
static WORD  wind_kind = NAME|CLOSER|FULLER|MOVER|SMALLER|SIZER;
#else
static WORD  wind_kind = NAME|CLOSER|FULLER|MOVER|SMALLER|SIZER |INFO|LFARROW;
#endif

WORD   hwWind_Mshape = ARROW;
HwWIND hwWind_Top    = NULL;
HwWIND hwWind_Focus  = NULL;


static void draw_infobar (HwWIND This, const GRECT * p_clip, const char * info);
static void set_size (HwWIND, const GRECT *);
static void wnd_hdlr (HW_EVENT, long, CONTAINR, const void *);


/*============================================================================*/
void
hwWind_setup (HWWIND_SET set, long arg)
{
	switch (set) {
		
		case HWWS_INFOBAR:
			if (arg & 1) wind_kind |=  INFO;
			else         wind_kind &= ~INFO;
			if      (arg & 4) wind_kind &= ~(HSLIDE|LFARROW|SIZER);
			else if (arg & 2) wind_kind |=  LFARROW;
			else              wind_kind &= ~LFARROW;
			break;
	}
}


/*============================================================================*/
HwWIND
new_hwWind (const char * name, const char * url, LOCATION loc)
{
	HwWIND This = malloc (sizeof (struct hw_window) +
	                      sizeof (HISTORY) * HISTORY_LAST);
	short  i;
	
	This->Next = hwWind_Top;
	hwWind_Top = This;
	
	if (!inc_xy) {
		WORD out, u;
		bevent = (appl_xgetinfo(AES_WINDOW, &out, &u,&u,&u) && (out & 0x20));
		wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &desk_area);
		wind_calc_grect (WC_BORDER, VSLIDE|HSLIDE, &desk_area, &curr_area);
		widget_b = desk_area.g_x - curr_area.g_x;
		widget_w = curr_area.g_w - desk_area.g_w;
		widget_h = curr_area.g_h - desk_area.g_h;
		inc_xy   = max (widget_w, widget_h) - widget_b;
		if (desk_area.g_w < 800) {
			curr_area = desk_area;
			curr_area.g_w = (desk_area.g_w *2) /3;
		} else {
			curr_area.g_x = desk_area.g_x + inc_xy;
			curr_area.g_y = desk_area.g_y + inc_xy;
			curr_area.g_w = (desk_area.g_w *3) /4;
			curr_area.g_h = (desk_area.g_h *3) /4;
		}
		if (!ignore_colours) {
			u   = W_HELEV;
			out = 0;
			if (wind_get (0, WF_DCOLOR, &u, &out, &u, &u) && out) {
				info_bgnd = out | 0x00F0;
				if ((info_bgnd & 0x000F) == G_BLACK) {
					info_fgnd = G_WHITE;
				}
			} else {
				info_bgnd = G_LWHITE;
			}
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
	
	This->Handle  = wind_create_grect (wind_kind, &desk_area);
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
	
	hwWind_setName (This, (name && *name ? name : url));
	This->Stat[0] = ' ';
	This->Info[0] = '\0';
	if (wind_kind & INFO) {
		wind_set_str (This->Handle, WF_INFO, This->Info);
	}
	if (wind_kind & (HSLIDE|LFARROW|SIZER)) {
		if (wind_kind & HSLIDE) {
			wind_set (This->Handle, WF_HSLIDE,   0, 0,0,0);
			wind_set (This->Handle, WF_HSLSIZE, -1, 0,0,0);
		}
		This->TbarH = 0;
		This->IbarH = 0;
	} else {
		This->TbarH = 0;
		This->IbarH = widget_h - widget_b -1;
	}
	set_size (This, &curr_area);

	if (bevent) {
		wind_set (This->Handle, WF_BEVENT, 0x0001, 0,0,0);
	}
	if (info_bgnd & 0x0080) {
		wind_set (This->Handle, WF_COLOR, W_HBAR,   info_bgnd, info_bgnd, -1);
		wind_set (This->Handle, WF_COLOR, W_HSLIDE, info_bgnd, info_bgnd, -1);
	}
	wind_open_grect (This->Handle, &This->Curr);
	if (wind_kind & (HSLIDE|LFARROW|SIZER)) {
		hwWind_setInfo (This, "", TRUE);
	} else if (This->IbarH) {
		draw_infobar (This, &This->Work, "");
	}

	if ((url && *url) || loc) {
		new_loader_job (url, loc, This->Pane);
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

/*----------------------------------------------------------------------------*/
static void
draw_infobar (HwWIND This, const GRECT * p_clip, const char * info)
{
	GRECT area, clip;
	PXY   p[2];
	short x_btn, dmy, fnt, pnt;
	
	if (This->isIcon) return;
	
	area = This->Work;
	area.g_y += area.g_h - This->IbarH +1;
	area.g_h =             This->IbarH -1;
	clip  = area;
	x_btn = area.g_x + area.g_w - This->IbarH;
	
	if (p_clip) {
		clip.g_y--;
		clip.g_h++;
		if (clip.g_y >= p_clip->g_y + p_clip->g_h) {
			return;
		}
		*(GRECT*)p = *p_clip;
	
	} else {
		clip.g_w -= This->IbarH +1;
		if (!rc_intersect (&desk_area, &clip)) {
			return;
		}
		wind_update (BEG_UPDATE);
		wind_get_grect (This->Handle, WF_FIRSTXYWH, (GRECT*)p);
	}
	
	vsf_interior (vdi_handle, FIS_SOLID);
	vsf_style    (vdi_handle, 4);
	vsf_color    (vdi_handle, info_bgnd & 0x000F);
	
	fnt = (inc_xy < 16 ? 1 : fonts[header_font][0][0]);
	if ((fnt = vst_font (vdi_handle, fnt)) == 1) {
		pnt = (inc_xy < 16 ? 11 : 12);
		vst_point (vdi_handle, pnt, &dmy,&dmy,&dmy,&dmy);
	} else {
		pnt = 14;
		vst_height (vdi_handle, pnt, &dmy,&dmy,&dmy,&dmy);
	}
	vst_color     (vdi_handle, info_fgnd);
	vst_map_mode  (vdi_handle, 1);
	vst_effects   (vdi_handle, TXT_NORMAL);
	
	v_hide_c (vdi_handle);
	while (p[1].p_x > 0 && p[1].p_y > 0) {
		if (rc_intersect (&clip, (GRECT*)p)) {
			p[1].p_x += p[0].p_x -1;
			p[1].p_y += p[0].p_y -1;
			v_bar (vdi_handle, (short*)p);
			
			if (*info && p[0].p_x < x_btn) {
				short rgt = p[1].p_x;
				if (p[1].p_x >= x_btn){
					p[1].p_x = x_btn -1;
				}
				vs_clip_pxy (vdi_handle, p);
				vst_alignment (vdi_handle, TA_LEFT, TA_TOP, &dmy, &dmy);
				v_ftext (vdi_handle, area.g_x +1, area.g_y -1, info);
				if (!p_clip) {
					vs_clip_off (vdi_handle);
				}
				p[1].p_x = rgt;
			}
			if (p_clip) {
				vs_clip_pxy (vdi_handle, p);
				vsl_color   (vdi_handle, info_fgnd & 0x000F);
				if (p[0].p_y < area.g_y) {
					PXY l[2];
					l[1].p_x = (l[0].p_x = area.g_x) + area.g_w -1;
					l[1].p_y =  l[0].p_y = area.g_y -1;
					v_pline (vdi_handle, 2, (short*)l);
				}
				if (p[1].p_x >= x_btn) {
					p[0].p_x = p[1].p_x = x_btn;
					v_pline (vdi_handle, 2, (short*)p);
					p[0].p_x++;
					p[0].p_y = area.g_y;
					p[1].p_x = p[1].p_y = This->IbarH -1;
					draw_border ((GRECT*)p, G_WHITE, G_LBLACK, 1);
					if (fnt != 1) {
						vst_font  (vdi_handle, 1);
						vst_point (vdi_handle, (inc_xy < 16 ? 11 : 12),
						           &dmy,&dmy,&dmy,&dmy);
					}
					vst_alignment (vdi_handle, TA_CENTER, TA_TOP, &dmy, &dmy);
					v_gtext (vdi_handle,
					         p[0].p_x + This->IbarH /2, p[0].p_y +1, "");
				}
				vs_clip_off (vdi_handle);
				break;
			}
		}
		wind_get_grect (This->Handle, WF_NEXTXYWH, (GRECT*)p);
	}
	v_show_c (vdi_handle, 1);
	
	vst_alignment (vdi_handle, TA_LEFT, TA_BASE, &dmy, &dmy);
	
	if (!p_clip) {
		wind_update (END_UPDATE);
	}
}

/*----------------------------------------------------------------------------*/
/* Set Horizontal scroller space text - only works on top windows */

static void
hwWind_setHSInfo (HwWIND This, const char * info)
{
	short dmy;
	PXY   p[2];

	if (This != hwWind_Top || This->isIcon) {
		return;
	} else {
		short top;
		wind_get (0, WF_TOP, &top, &dmy, &dmy, &dmy);
		if (top != This->Handle)
			return;
	}
	
	p[0].p_y = This->Work.g_y + This->Work.g_h                       +1;
	p[1].p_y = This->Curr.g_y + This->Curr.g_h            - widget_b -1;
	p[0].p_x = This->Curr.g_x                  + widget_w - widget_b +1;
	p[1].p_x = This->Curr.g_x + This->Curr.g_w - widget_w + widget_b -2;
	
	vswr_mode    (vdi_handle, MD_REPLACE);
	vsf_interior (vdi_handle, FIS_SOLID);
	vsf_style    (vdi_handle, 4);
	vsf_color    (vdi_handle, info_bgnd & 0x000F);
	
	dmy = (inc_xy < 16 ? 1 : fonts[header_font][0][0]);
	if (vst_font (vdi_handle, dmy) == 1) {
		vst_point (vdi_handle, (inc_xy < 16 ? 11 : 12), &dmy, &dmy, &dmy, &dmy);
	} else {
		vst_height (vdi_handle, 14, &dmy, &dmy, &dmy, &dmy);
	}
	vst_color     (vdi_handle, info_fgnd);
	vst_map_mode  (vdi_handle, 1);
	vst_effects   (vdi_handle, TXT_NORMAL);
	vst_alignment (vdi_handle, TA_LEFT, TA_DESCENT, &dmy, &dmy);

	vs_clip_pxy (vdi_handle, p);

	v_hide_c     (vdi_handle);
	v_bar        (vdi_handle, (short*)p);

	vswr_mode (vdi_handle, MD_TRANS);

	v_ftext  (vdi_handle, p[0].p_x +1, p[1].p_y -1, info);
	v_show_c (vdi_handle, 1);
	
	vst_alignment (vdi_handle, TA_LEFT, TA_BASE, &dmy, &dmy);

	vs_clip_off (vdi_handle);
}

/*============================================================================*/
void
hwWind_setInfo (HwWIND This, const char * info, BOOL statNinfo)
{
	if (statNinfo) {
		if (info && *info) {
			strncpy (This->Stat, info, sizeof(This->Stat));
			This->Stat[sizeof(This->Stat)-1] = '\0';
			info = This->Stat;
		} else if (This->Stat[0]) {
			This->Stat[0] = '\0';
			info = This->Info;
		} else {
			info = (This->Info[0] ? This->Info : NULL);
		}
	
	} else {
		if (info && *info) {
			strncpy (This->Info, info, sizeof(This->Info));
			This->Info[sizeof(This->Info)-1] = '\0';
			info = This->Info;
		} else if (This->Info[0]) {
			This->Info[0] = '\0';
			info = This->Stat;
		} else {
			info = (This->Stat[0] ? This->Stat : NULL);
		}
	}
	
	if (info) {
		if (wind_kind & INFO) {
			wind_set_str (This->Handle, WF_INFO, info);
		}
		if (wind_kind & (LFARROW|HSLIDE)) {
			hwWind_setHSInfo(This, info);
		} else if (This->IbarH) {
			draw_infobar (This, NULL, info);
		}
	}
}


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
	if (This->TbarH || This->IbarH) {
		GRECT work = This->Work;
		work.g_y += This->TbarH;
		work.g_h -= This->TbarH + This->IbarH;
		containr_calculate (This->Pane, &work);
	} else {
		containr_calculate (This->Pane, &This->Work);
	}
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
		if (wind_kind & (LFARROW|HSLIDE)) {
			hwWind_setHSInfo (This, (This->Info[0] ? This->Info : This->Stat));
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
			if (wind_kind & (LFARROW|HSLIDE)) {
				hwWind_setHSInfo (This, (This->Info[0] ? This->Info : This->Stat));
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
		if (wind_kind & (LFARROW|HSLIDE)) {
			hwWind_setHSInfo (This, (This->Info[0] ? This->Info : This->Stat));
		}
	
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
			LOADER ldr = new_loader_job (NULL, entr[i].Location, entr[i].Target);
			loader_setParams (ldr, entr->Encoding, -1, -1);
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
	char * info = (!This->IbarH ? NULL : *This->Info ? This->Info : This->Stat);
	
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
				if (info) {
					draw_infobar (This, &rect, info);
				}
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
			hwWind_setInfo (wind, gen_ptr, TRUE);
			break;
		
		case HW_ImgBegLoad:
			if (!wind->isBusy++) {
				if (wind->isIcon) {
					hwWind_redraw (wind, NULL); /* update icon */
				} else {
					graf_mouse (hwWind_Mshape = BUSYBEE, NULL);
				}
			}
			hwWind_setInfo (wind, gen_ptr, TRUE);
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
#if defined(GEM_MENU) && (_HIGHWIRE_ENCMENU_ == 1)
				update_menu (active->Encoding,
				             (active->MimeType == MIME_TXT_PLAIN));
#endif
			}
		case HW_ImgEndLoad:
			hwWind_setInfo (wind, "", TRUE);
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
HwWIND
hwWind_mouse  (WORD mx, WORD my, GRECT * watch)
{
	GRECT  clip;
	WORD   whdl = wind_find (mx, my);
	HwWIND wind = hwWind_byHandle (whdl);
	
	if (wind && !wind->isIcon) {
		clip = wind->Work;
	} else {
		wind_get_grect (whdl, WF_WORKXYWH, &clip);
	}
	if (mx < clip.g_x || mx >= clip.g_x + clip.g_w ||
	    my < clip.g_y || my >= clip.g_y + clip.g_h) {
		watch->g_x = mx;
		watch->g_y = my;
		watch->g_w = watch->g_h = 1;
		wind = NULL;
	
	} else {
		wind_get_grect (whdl, WF_FIRSTXYWH, watch);
		while (watch->g_w > 0 && watch->g_h > 0 &&
			    (mx < watch->g_x || mx >= watch->g_x + watch->g_w ||
			     my < watch->g_y || my >= watch->g_y + watch->g_h)) {
			wind_get_grect (whdl, WF_NEXTXYWH, watch);
		}
		if (watch->g_w <= 0 || watch->g_h <= 0) {
			watch->g_x = mx;
			watch->g_y = my;
			watch->g_w = watch->g_h = 1;
			wind = NULL;
		
		} else if (wind) {
			if (wind->isIcon) {
				wind = NULL;
			
			} else if (wind->IbarH) {
				WORD y = clip.g_y + clip.g_h - wind->IbarH;
				if (my < y) {
					clip.g_h -= wind->IbarH;
				} else {
					clip.g_y += wind->IbarH;
					clip.g_h =  wind->IbarH;
					wind = NULL;
				}
				rc_intersect (&clip, watch);
			}
		}
	}
	return wind;
}


/*============================================================================*/
HwWIND
hwWind_button (WORD mx, WORD my)
{
	HwWIND wind = hwWind_byCoord (mx, my);
	WORD   ib_x = 0, ib_y = 0;
	if (wind && wind->IbarH &&
		 ((ib_y = wind->Work.g_y + wind->Work.g_h - wind->IbarH) > my ||
		  (ib_x = wind->Work.g_x + wind->Work.g_w - wind->IbarH) > mx)) {
		ib_y = 0;
	}
	if (!wind) {
		short dmy;
		wind_update (BEG_MCTRL);
		evnt_button (1, 0x03, 0x00, &dmy, &dmy, &dmy, &dmy);
		wind_update (END_MCTRL);
	
	} else if (ib_y) {
		short     event = wind_update (BEG_MCTRL);
		EVMULT_IN m_in  = { MU_BUTTON|MU_M1, 1, 0x03, 0x00, MO_LEAVE, };
		PXY c[5], w[5];
		c[0] = c[4] = *(PXY*)&wind->Curr;
		c[1].p_x = c[2].p_x = (c[3].p_x = c[0].p_x) + wind->Curr.g_w -1;
		c[3].p_y = c[2].p_y = (c[1].p_y = c[0].p_y) + wind->Curr.g_h -1;
		c[4].p_y++;
		w[0] = w[4] = *(PXY*)&wind->Work;
		w[1].p_x = w[2].p_x = (w[3].p_x = w[0].p_x) + wind->Work.g_w -1;
		w[1].p_y = w[0].p_y;
		w[3].p_y = w[2].p_y = ib_y;
		w[4].p_y++;
		m_in.emi_m1.g_x = mx;
		m_in.emi_m1.g_y = my;
		m_in.emi_m1.g_w = m_in.emi_m1.g_h = 1;
		vswr_mode (vdi_handle, MD_XOR);
		vsl_type  (vdi_handle, USERLINE);
		vsl_udsty (vdi_handle, 0xAAAA);
		v_hide_c  (vdi_handle);
		do {
			EVMULT_OUT out;
			WORD       msg[8];
			v_pline  (vdi_handle, 5, (short*)c);
			v_pline  (vdi_handle, 5, (short*)w);
			v_show_c (vdi_handle, 1);
			event = evnt_multi_fast (&m_in, msg, &out);
			v_hide_c (vdi_handle);
			v_pline  (vdi_handle, 5, (short*)c);
			v_pline  (vdi_handle, 5, (short*)w);
			if (event & MU_M1) {
				WORD dx = out.emo_mouse.p_x - m_in.emi_m1.g_x;
				WORD dy = out.emo_mouse.p_y - m_in.emi_m1.g_y;
				WORD d;
				if ((d = (w[2].p_x += dx) - w[0].p_x -80) < 0) {
					dx       -= d;
					w[2].p_x -= d;
				}
				if ((d = (w[2].p_y += dy) - w[0].p_y -40) < 0) {
					dy       -= d;
					w[2].p_y -= d;
				}
				w[1].p_x = w[2].p_x;
				w[3].p_y = w[2].p_y;
				c[1].p_x = c[2].p_x += dx;
				c[3].p_y = c[2].p_y += dy;
				*(PXY*)&m_in.emi_m1 = out.emo_mouse;
			}
		} while (!(event & MU_BUTTON));
		v_show_c  (vdi_handle, 1);
		vsl_type  (vdi_handle, SOLID);
		vswr_mode (vdi_handle, MD_TRANS);
		wind_update (END_MCTRL);
		c[1].p_x = c[2].p_x - c[0].p_x +1;
		c[1].p_y = c[2].p_y - c[0].p_y +1;
		hwWind_resize (wind, (GRECT*)c);
		wind = NULL;
	}
	return wind;
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

