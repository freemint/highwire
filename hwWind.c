#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <gem.h>

#include <gemx.h>

#ifdef __PUREC__
# define CONTAINR struct s_containr *
# define HISTORY  struct s_history *
#endif

#include "global.h"
#include "Containr.h"
#include "Loader.h"
#include "Location.h"
#include "Form.h"
#include "cache.h"
#include "Logging.h"

#define WINDOW_t HwWIND
#include "hwWind.h"
static BOOL vTab_evMessage (HwWIND, WORD msg[], PXY mouse, UWORD kstate);
static void vTab_evButton  (HwWIND, WORD bmask, PXY mouse, UWORD kstate, WORD);
static void vTab_evKeybrd  (HwWIND, WORD scan, WORD ascii, UWORD kstate);
static void vTab_drawWork  (HwWIND, const GRECT *);
static void vTab_drawIcon  (HwWIND, const GRECT *);
static void vTab_raised    (HwWIND, BOOL topNbot);
static void vTab_moved     (HwWIND);
static BOOL vTab_sized     (HwWIND);
static void vTab_iconified (HwWIND);

#define Curr   Base.Curr


#define HISTORY_LAST 20


static WORD  info_fgnd = G_BLACK, info_bgnd = G_WHITE;
static WORD  inc_xy = 0;
static WORD  widget_b, widget_w, widget_h;
static BOOL  bevent;
static GRECT desk_area;
static GRECT curr_area;
static WORD  tbar_set = +21;
#if (_HIGHWIRE_INFOLINE_ == 0)
static WORD wind_kind = NAME|CLOSER|FULLER|MOVER|SMALLER|SIZER;
#else
static WORD wind_kind = NAME|CLOSER|FULLER|MOVER|SMALLER/*|SIZER|INFO|LFARROW*/;
#endif

WORD   hwWind_Mshape = ARROW;
HwWIND hwWind_Focus  = NULL;


static void draw_infobar (HwWIND This, const GRECT * p_clip, const char * info);
static void draw_toolbar (HwWIND This, const GRECT * p_clip, BOOL all);
static BOOL chng_toolbar (HwWIND, UWORD, UWORD, WORD);
static void wnd_hdlr (HW_EVENT, long, CONTAINR, const void *);


typedef struct {
	WORD Offset;
	WORD Fgnd, Bgnd;
	WORD Data[15], Fill[15];
}  TOOLBTN;
static TOOLBTN hw_buttons[] = {
#define	TBAR_LEFT 0
	{	-1, G_BLACK, G_GREEN,
		{	0x0400, 0x0C00, 0x1400, 0x27E0, 0x4090, 0x8048, 0x8044, 0x4042,
			0x27C2, 0x1442, 0x0C42, 0x0442, 0x0042, 0x005A, 0x0066 },
		{	0x0000, 0x0000, 0x0800, 0x1800, 0x3F60, 0x7FB0, 0x7FB8, 0x3FBC,
			0x183C, 0x083C, 0x003C, 0x003C, 0x003C, 0x0025, 0x0000 } },
#define	TBAR_RGHT 1
	{	20, G_BLACK, G_GREEN,
		{	0x0040, 0x0060, 0x0050, 0x0FC8, 0x1204, 0x2402, 0x4402, 0x8404,
			0x87C8, 0x8450, 0x8460, 0x8440, 0x8400, 0xB400, 0xCC00 },
		{	0x0000, 0x0000, 0x0020, 0x0030, 0x0DF8, 0x1BFC, 0x3BFC, 0x7BF8,
			0x7830, 0x7820, 0x7800, 0x7800, 0x7800, 0x4800, 0x0000 } },
#define	TBAR_HOME 2
	{	41, G_BLACK, G_WHITE,
		{	0x0100, 0x3280, 0x3540, 0x3AA0, 0x3550, 0x2AA8, 0x5554, 0xC006,
			0x5EF4, 0x5294, 0x5294, 0x5EB4, 0x4094, 0x4094, 0x7FFC },
		{	0x0000, 0x0100, 0x0280, 0x0540, 0x0AA0, 0x1550, 0x2AA8, 0x3FF8,
			0x2108, 0x2D68, 0x2D68, 0x2148, 0x3F68, 0x3F68, 0x0000 } },
#define	TBAR_REDO 3
	{	72, G_BLACK, G_YELLOW,
		{	0x03E0, 0x0C10, 0x1F08, 0x2084, 0x0044, 0x10C6, 0x2882, 0x4444,
			0x8228, 0xC610, 0x4400, 0x4208, 0x21F0, 0x1060, 0x0F80 },
		{	0x0000, 0x03E0, 0x00F0, 0x0078, 0x0038, 0x0038, 0x107C, 0x3838,
			0x7C10, 0x3800, 0x3800, 0x3C00, 0x1E00, 0x0F80, 0x0000 } },
#define	TBAR_OPEN 4
	{	93, G_BLACK, G_CYAN,
		{	0x0780, 0x1fe8, 0x3078, 0x4038, 0x0078, 0x3800, 0x47c0, 0x8020,
			0x8ffe, 0x9002, 0x9004, 0xa004, 0xa008, 0xc008, 0xfff0 },
		{	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3800, 0x7fc0,
			0x7000, 0x6ffc, 0x6ff8, 0x5ff8, 0x5ff0, 0x3ff0, 0x0000 } },
#define	TBAR_STOP 5
	{114, G_WHITE, G_RED,
		{	0x0FE0, 0x1010, 0x2008, 0x4004, 0x975A, 0xA2DA, 0xA2DA, 0x92DA,
			0x8AD2, 0x8AD2, 0xB292, 0x4004, 0x2008, 0x1010, 0x0FE0 },
		{	0x0000, 0x0FE0, 0x1FF0, 0x3FF8, 0x68A4, 0x5D24, 0x5D24, 0x6D24,
			0x752D, 0x752D, 0x4D6C, 0x3FF8, 0x1FF0, 0x0FE0, 0x0000 } }
#define	TBAR_EDIT 6
};
#define TBAR_LEFT_MASK (1 << TBAR_LEFT)
#define TBAR_RGHT_MASK (1 << TBAR_RGHT)
#define TBAR_HOME_MASK (1 << TBAR_HOME)
#define TBAR_REDO_MASK (1 << TBAR_REDO)
#define TBAR_OPEN_MASK (1 << TBAR_OPEN)
#define TBAR_STOP_MASK (1 << TBAR_STOP)

typedef struct {
	WORD Visible;
	WORD Shift;
	WORD Cursor;
	WORD Length;
	char Text[1024];
} TBAREDIT;
#define TbarEdit(w) ((TBAREDIT*)(w->History + HISTORY_LAST +2))


/*============================================================================*/
void
hwWind_setup (HWWIND_SET set, long arg)
{
	switch (set) {
		
		case HWWS_INFOBAR:
			if (arg & 1) wind_kind |=  INFO;
			else         wind_kind &= ~INFO;
			if (arg & 4) {
				wind_kind &= ~(SIZER|LFARROW|HSLIDE);
			} else {
				wind_kind |=   SIZER;
				if (arg & 2) wind_kind |=  LFARROW;
				else         wind_kind &= ~LFARROW;
			}
			break;
		
		case HWWS_TOOLBAR:
			if      (arg > 0) tbar_set = +21;
			else if (arg < 0) tbar_set = -21;
			else              tbar_set = 0;
			break;
	}
}


/*============================================================================*/
HwWIND
new_hwWind (const char * name, const char * url, LOCATION loc)
{
	HwWIND This = malloc (sizeof (struct hw_window) +
	                      sizeof (HISTORY) * HISTORY_LAST +
	                      sizeof (TBAREDIT) +1);
	TBAREDIT * edit;
	short  i;
	
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
#if 0 /* this doesn't work with MagiC yet */
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
#else
			info_bgnd = G_LWHITE;
#endif
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
	
	window_ctor (This, wind_kind, NULL, FALSE);
	This->Base.evMessage = vTab_evMessage;
	This->Base.evButton  = vTab_evButton;
	This->Base.evKeybrd  = vTab_evKeybrd;
	This->Base.drawWork  = vTab_drawWork;
	This->Base.drawIcon  = vTab_drawIcon;
	This->Base.raised    = vTab_raised;
	This->Base.moved     = vTab_moved;
	This->Base.sized     = vTab_sized;
	This->Base.iconified = vTab_iconified;
	
	This->shaded  = FALSE;
	This->isBusy  = 0;
	This->loading = 0;
	This->Pane    = new_containr (NULL);
	This->Active  = NULL;
	This->Input   = NULL;
	containr_register (This->Pane, wnd_hdlr, (long)This);
	
	This->HistUsed = 0;
	This->HistMenu = 0;
	for (i = 0; i <= HISTORY_LAST; This->History[i++] = NULL);
	
	hwWind_setName (This, (name && *name ? name : url));
	This->Stat[0] = '\0';
	This->Info[0] = '\0';
	
	if (wind_kind & (HSLIDE|LFARROW|SIZER)) {
		if (wind_kind & HSLIDE) {
			wind_set (This->Base.Handle, WF_HSLIDE,   0, 0,0,0);
			wind_set (This->Base.Handle, WF_HSLSIZE, -1, 0,0,0);
		}
		This->IbarH = 0;
	} else {
		This->IbarH = widget_h - widget_b -1;
	}
	This->TbarH    = (tbar_set > 0 ? tbar_set : 0);
	This->TbarMask = 0;
	This->TbarActv = (This->TbarH && !url && !loc ? TBAR_EDIT : -1);
	for (i = 0; i < numberof(hw_buttons); i++) {
		This->TbarElem[i].Offset = hw_buttons[i].Offset;
		This->TbarElem[i].Width  = 21;
	}
	This->TbarElem[TBAR_EDIT].Offset = This->TbarElem[TBAR_EDIT -1].Offset
	                                 + This->TbarElem[TBAR_EDIT -1].Width *3 /2;
	edit = TbarEdit (This);
	edit->Text[0] = '\0';
	edit->Length  = 0;
	edit->Shift   = 0;
	edit->Cursor  = 0;

	if (bevent) {
		wind_set (This->Base.Handle, WF_BEVENT, 0x0001, 0,0,0);
	}
	if (info_bgnd & 0x0080) {
		wind_set(This->Base.Handle, WF_COLOR, W_HBAR,   info_bgnd, info_bgnd, -1);
		wind_set(This->Base.Handle, WF_COLOR, W_HSLIDE, info_bgnd, info_bgnd, -1);
	}
	window_raise (&This->Base, TRUE, &curr_area);
	hwWind_redraw (This, NULL);

	if ((url && *url) || loc) {
		start_page_load (This->Pane, url, loc, TRUE, NULL);
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
	short i;
	
#ifdef GEM_MENU
	if (This == hwWind_Top) {
		HwWIND next = hwWind_Next (This);
		if (next) {
			menu_history (next->History, next->HistUsed, next->HistMenu);
		} else {
			menu_history (NULL,0,0);
		}
	}
#endif
	for (i = 0; i < This->HistUsed; history_destroy (&This->History[i++]));
	
	delete_containr ((CONTAINR*)&This->Pane);

	if (hwWind_Focus == This) {
		hwWind_Focus = NULL;
	}
	
	free (window_dtor(This));
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
	wind_set_str (This->Base.Handle, WF_NAME, This->Name);
}

/*----------------------------------------------------------------------------*/
static void
draw_busybar (HwWIND This, const GRECT * area, const GRECT * clip)
{
	WORD  width = (This->isBusy - This->loading) +1;
	WORD  style;
	GRECT rect;
	PXY   p[2];
	p[0].p_x = area->g_x +2;
	p[0].p_y = area->g_y +2;
	p[1].p_x = area->g_w -4 - This->IbarH;
	p[1].p_y = area->g_h -4;
	if (p[1].p_x >= width) {
		p[0].p_x += p[1].p_x - width;
		p[1].p_x =  width;
		style = (ignore_colours ? 1 : 4);
	} else {
		style = (ignore_colours ? 4 : 8);
	}
	rect = *(GRECT*)p;
	if (rc_intersect (clip, &rect)) {
		p[1].p_x += p[0].p_x -1;
		p[1].p_y += p[0].p_y -1;
		vsf_perimeter(vdi_handle, PERIMETER_ON);
		vsf_interior (vdi_handle, FIS_PATTERN);
		vsf_style    (vdi_handle, style);
		vsf_color    (vdi_handle, G_RED);
		v_bar        (vdi_handle, (short*)p);
		vsf_perimeter(vdi_handle, PERIMETER_OFF);
		vsf_interior (vdi_handle, FIS_SOLID);
	}
}

/*----------------------------------------------------------------------------*/
static void
draw_infobar (HwWIND This, const GRECT * p_clip, const char * info)
{
	GRECT area, clip, rect;
	short x_btn, dmy, fnt, pnt;
	
	if (This->Base.isIcon || This->shaded) return;
	
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
		rect = *p_clip;
	
	} else {
		clip.g_w -= This->IbarH +1;
		if (!rc_intersect (&desk_area, &clip)) {
			return;
		}
		wind_update (BEG_UPDATE);
		wind_get_grect (This->Base.Handle, WF_FIRSTXYWH, &rect);
	}
	
	vsf_interior (vdi_handle, FIS_SOLID);
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
	while (rect.g_w > 0 && rect.g_h > 0) {
		if (rc_intersect (&clip, &rect)) {
			PXY p[2];
			p[1].p_x = (p[0].p_x = rect.g_x) + rect.g_w -1;
			p[1].p_y = (p[0].p_y = rect.g_y) + rect.g_h -1;
			v_bar (vdi_handle, (short*)p);
			
			if (This->isBusy) {
				vs_clip_pxy (vdi_handle, p);
				draw_busybar (This, &area, &rect);
				vsf_color (vdi_handle, info_bgnd & 0x000F);
			}
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
		wind_get_grect (This->Base.Handle, WF_NEXTXYWH, &rect);
	}
	v_show_c (vdi_handle, 1);
	
	vst_alignment (vdi_handle, TA_LEFT, TA_BASE, &dmy, &dmy);
	
	if (!p_clip) {
		wind_update (END_UPDATE);
	}
}

/*------------------------------------------------------------------------------
 * Set Horizontal scroller space text - only works on top windows
*/
static void
hwWind_setHSInfo (HwWIND This, const char * info)
{
	short dmy;
	PXY   p[2], clip[2];

	if (This != hwWind_Top || This->Base.isIcon || This->shaded) {
		return;
	} else {
		short top;
		wind_get (0, WF_TOP, &top, &dmy, &dmy, &dmy);
		if (top != This->Base.Handle) {
			return;
		}
	}
	p[0].p_y = This->Work.g_y + This->Work.g_h                       +1;
	p[1].p_y = This->Curr.g_y + This->Curr.g_h            - widget_b -1;
	p[0].p_x = This->Curr.g_x                  + widget_w - widget_b +1;
	p[1].p_x = This->Curr.g_x + This->Curr.g_w - widget_w + widget_b -2;
	clip[0].p_x = max (p[0].p_x, desk_area.g_x);
	clip[0].p_y = max (p[0].p_y, desk_area.g_y);
	clip[1].p_x = min (p[1].p_x, desk_area.g_x + desk_area.g_w -1);
	clip[1].p_y = min (p[1].p_y, desk_area.g_y + desk_area.g_h -1);
	if (clip[0].p_x > clip[1].p_x || clip[0].p_y > clip[1].p_y) {
		return;
	}
	vs_clip_pxy (vdi_handle, clip);
	
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

	v_hide_c     (vdi_handle);
	v_bar        (vdi_handle, (short*)p);

	vswr_mode (vdi_handle, MD_TRANS);

	if (This->isBusy) {
		GRECT area = *(GRECT*)p;
		area.g_w -= area.g_x -1;
		area.g_h -= area.g_y -1;
		clip[1].p_x -= clip[0].p_x -1;
		clip[1].p_y -= clip[0].p_y -1;
		draw_busybar (This, &area, (GRECT*)clip);
	}
	
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
	
	if (info && !(This->Base.isScrn && This->Base.isFull)) {
		if (wind_kind & INFO) {
			wind_set_str (This->Base.Handle, WF_INFO, info);
		}
		if (wind_kind & (LFARROW|HSLIDE)) {
			hwWind_setHSInfo(This, info);
		} else if (This->IbarH) {
			draw_infobar (This, NULL, info);
		}
	}
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_raised (HwWIND This, BOOL topNbot)
{
	HwWIND new_top = hwWind_Top;
	WORD mx, my, u;
	
	if (topNbot && !This->Base.isScrn && (wind_kind & (LFARROW|HSLIDE))) {
		hwWind_setHSInfo (This, (This->Info[0] ? This->Info : This->Stat));
	}
	graf_mkstate (&mx, &my, &u,&u);
	check_mouse_position (mx, my);
	
#ifdef GEM_MENU
	if (new_top) {
		menu_history (new_top->History, new_top->HistUsed, new_top->HistMenu);
	}
#endif
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_moved (HwWIND This)
{
	wind_get_grect (This->Base.Handle, WF_WORKXYWH, &This->Work);
	containr_relocate (This->Pane, This->Work.g_x, This->Work.g_y + This->TbarH);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
vTab_sized (HwWIND This)
{
	TBAREDIT * edit = TbarEdit (This);
	GRECT work;
	
	if (This->Base.isScrn) {
		if (This->Base.isFull) {
			This->IbarH = 0;
		
		} else {
			char * info = (This->Info[0] ? This->Info : This->Stat);
			if (wind_kind & INFO) {
				wind_set_str (This->Base.Handle, WF_INFO, info);
			}
			if (wind_kind & (LFARROW|HSLIDE)) {
				hwWind_setHSInfo(This, info);
			} else {
				This->IbarH = widget_h - widget_b -1;
			}
			wind_set_str (This->Base.Handle, WF_NAME, This->Name);
		}
		
	} else if (!This->Base.isFull) {
		curr_area.g_w = This->Curr.g_w;
		curr_area.g_h = This->Curr.g_h;
	}
	
	wind_get_grect (This->Base.Handle, WF_WORKXYWH, &This->Work);
	work = This->Work;
	work.g_y += This->TbarH;
	work.g_h -= This->TbarH + This->IbarH;
	containr_calculate (This->Pane, &work);
	
	work.g_w -= This->TbarElem[TBAR_EDIT].Offset +6;
	if ((edit->Visible = work.g_w /8 -1) < 7) {
		edit->Visible = 7;
	}
	This->TbarElem[TBAR_EDIT].Width = edit->Visible *8 +6;
	if (edit->Length <= edit->Visible) {
		edit->Shift = 0;
	} else if (edit->Shift > edit->Length - edit->Visible) {
		edit->Shift = edit->Length - edit->Visible;
	} else if (edit->Shift < edit->Cursor - edit->Visible) {
		edit->Shift = edit->Cursor - edit->Visible;
	}
	
	return TRUE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_iconified (HwWIND This)
{
	short mx, my, u;
	
	if (This->Base.isIcon) {
		if (This->TbarActv == TBAR_EDIT) {
			TBAREDIT * edit = TbarEdit (This);
			edit->Cursor = 0;
			edit->Shift  = 0;
			chng_toolbar (This, 0, 0, -1);
		}
	} else {
		if (wind_kind & (LFARROW|HSLIDE)) {
			hwWind_setHSInfo (This, (This->Info[0] ? This->Info : This->Stat));
		}
	}
	graf_mkstate (&mx, &my, &u,&u);
	check_mouse_position (mx, my);
}


/*============================================================================*/
void
hwWind_close (HwWIND This, UWORD state)
{
	if (!(state & K_ALT)) {
		delete_hwWind (This);
	
	} else if (tbar_set) {
		GRECT work = This->Work;
		TBAREDIT * edit = TbarEdit (This);
		edit->Text[0] = '\0';
		edit->Length  = 0;
		edit->Shift   = 0;
		edit->Cursor  = 0;
		This->TbarActv = -1;
		if (This->TbarH) {
			if (tbar_set > 0) tbar_set = -tbar_set;
			This->TbarH = 0;
		} else {
			if (tbar_set < 0) tbar_set = -tbar_set;
			This->TbarH = tbar_set;
		}
		work.g_y += This->TbarH;
		work.g_h -= This->TbarH + This->IbarH;
		containr_calculate (This->Pane, &work);
		work.g_y -= This->TbarH;
		work.g_h += This->TbarH;
		hwWind_redraw (This, &work);
	}
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
		wind_get_grect (This->Base.Handle, WF_FIRSTXYWH, &clip);
		while (clip.g_w > 0 && clip.g_h > 0) {
			if (rc_intersect (&work, &clip)) {
				containr_scroll (cont, &clip, dx, dy);
			}
			wind_get_grect (This->Base.Handle, WF_NEXTXYWH, &clip);
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
hwWind_history (HwWIND This, UWORD menu, BOOL renew)
{
	if (menu < This->HistUsed) {
		HISTENTR entr[100];
		UWORD    num;
		
		history_update (This->Pane, This->History[This->HistMenu]);
		
		num = containr_process (This->Pane, This->History[menu],
		                        This->History[This->HistMenu],
		                        entr, numberof(entr));
		if (!num) {
			UWORD bttn_on  = TBAR_REDO_MASK|TBAR_OPEN_MASK;
			UWORD bttn_off = 0;
			if (This->HistMenu != menu) {
#ifdef GEM_MENU
				if (This == hwWind_Top) {
					menu_history (This->History, This->HistUsed, menu);
				}
#endif
				This->HistMenu = menu;
				if (This->HistMenu > 0) {
					bttn_on  |= TBAR_LEFT_MASK|TBAR_HOME_MASK;
				} else {
					bttn_off |= TBAR_LEFT_MASK|TBAR_HOME_MASK;
				}
				if (This->HistMenu < This->HistUsed -1) {
					bttn_on  |= TBAR_RGHT_MASK;
				} else {
					bttn_off |= TBAR_RGHT_MASK;
				}
			}
			chng_toolbar (This, bttn_on, bttn_off, -1);
		
		} else {
			UWORD i;
			This->History[menu]->Text[0] = '*';
			if (This->HistMenu != menu) {
#ifdef GEM_MENU
				if (This == hwWind_Top) {
					menu_history (This->History, This->HistUsed, -1);
				}
#endif
				This->HistMenu = menu;
			}
			if (renew) {
				i = 0;
				do if (PROTO_isRemote (entr[i].Location->Proto)) {
					CACHED cached = cache_lookup (entr[i].Location, 0, NULL);
					if (cached) cache_clear (cached);
				} while (++i < num);
			}
			i = 0;
			do {
				LOADER ldr = start_cont_load (entr[i].Target,
				                              NULL, entr[i].Location, FALSE, TRUE);
				if (ldr) {
					ldr->Encoding = entr[i].Encoding;
					ldr->ScrollV  = entr[i].ScrollV;
					ldr->ScrollH  = entr[i].ScrollH;
				}
			} while (++i < num);
		}
	}
}

/*============================================================================*/
void
hwWind_undo (HwWIND This, BOOL redo)
{
	if (redo) {
		hwWind_history (This, This->HistMenu +1, FALSE);
	} else if (This->HistMenu) {
		hwWind_history (This, This->HistMenu -1, FALSE);
	}
}


/*============================================================================*/
HwWIND
hwWind_Next (HwWIND This)
{
	WINDOW next;
	if (!This) {
		extern WINDOW window_Top;
		next = window_Top;
	} else {
		next = This->Base.Next;
	}
	while (next && next->drawWork != vTab_drawWork) {
		next = next->Next;
	}
	return (HwWIND)next;
}

/*============================================================================*/
HwWIND
hwWind_byHandle (WORD hdl)
{
	HwWIND wind = hwWind_Top;
	while (wind && wind->Base.Handle != hdl) {
		wind = hwWind_Next (wind);
	}
	return wind;
}

/*============================================================================*/
HwWIND
hwWind_byValue (long val)
{
	HwWIND wind = hwWind_Top;
	while (wind && wind != (HwWIND)val) {
		wind = hwWind_Next (wind);
	}
	return wind;
}

/*============================================================================*/
HwWIND
hwWind_byContainr (CONTAINR cont)
{
	HwWIND wind = (cont ? hwWind_Top : NULL);
	while (wind && wind->Pane != cont->Base) {
		wind = hwWind_Next (wind);
	}
	return wind;
}


/*----------------------------------------------------------------------------*/
static void
draw_toolbar (HwWIND This, const GRECT * p_clip, BOOL all)
{
	GRECT clip, area = This->Work;
	area.g_h = This->TbarH;
	clip = area;
	
	if (rc_intersect (p_clip, &clip)) {
		PXY  p[4] = { {0,0}, {14,14}, };
		clip.g_w += clip.g_x -1;
		clip.g_h += clip.g_y -1;
		vs_clip_pxy (vdi_handle, (PXY*)&clip);
		v_hide_c (vdi_handle);
		if (all) {
			vsf_color (vdi_handle, (ignore_colours ? G_WHITE : G_LWHITE));
			v_bar   (vdi_handle, (short*)&clip);
			p[3].p_x = (p[2].p_x = area.g_x) + area.g_w -1;
			p[3].p_y =  p[2].p_y = area.g_y  + area.g_h -1;
			vsl_color (vdi_handle, G_BLACK);
			v_pline (vdi_handle, 2, (short*)(p +2));
		}
		vsf_color (vdi_handle, G_WHITE);
		
		p[3].p_y = (p[2].p_y = area.g_y +3) +15 -1;
		if (all) {
			MFDB scrn = { NULL, }, icon = { NULL, 15,15,1, 1, 1, 0,0,0 };
			WORD off[] = { G_LBLACK, 0 };
			TOOLBTN * btn = hw_buttons;
			int i;
			for (i = 0; i < numberof(hw_buttons); i++, btn++) {
				p[2].p_x = area.g_x + This->TbarElem[i].Offset +3;
				p[3].p_x = p[2].p_x + This->TbarElem[i].Width  -7;
				icon.fd_addr = btn->Data;
				if (This->TbarMask & (1 << i)) {
					short c_lu, c_rd;
					PXY l[4];
					if (!ignore_colours) {
						vrt_cpyfm (vdi_handle,
						           MD_TRANS, (short*)p, &icon, &scrn, &btn->Fgnd);
						icon.fd_addr = btn->Fill;
						vrt_cpyfm (vdi_handle,
						           MD_TRANS, (short*)p, &icon, &scrn, &btn->Bgnd);
					} else {
						vrt_cpyfm (vdi_handle, MD_TRANS, (short*)p, &icon, &scrn, off);
					}
					l[0].p_x = p[2].p_x -1;
					l[2].p_y = p[2].p_y -1;
					if (i == This->TbarActv) {
						l[0].p_y = l[2].p_y;
						l[1].p_x = p[3].p_x +1;
						l[1].p_y = p[3].p_y +1;
						vswr_mode (vdi_handle, MD_XOR);
						v_bar     (vdi_handle, (short*)l);
						vswr_mode (vdi_handle, MD_TRANS);
						l[1].p_x++;
						l[1].p_y++;
						c_rd = G_LWHITE;
						c_lu = G_BLACK;
					} else {
						l[1].p_x = p[3].p_x +2;
						l[1].p_y = p[3].p_y +2;
						c_rd = G_LBLACK;
						c_lu = G_WHITE;
					}
					l[2].p_x = l[1].p_x;
					l[0].p_y = l[1].p_y;
					vsl_color (vdi_handle, c_rd);
					v_pline (vdi_handle, 3, (short*)l);
					l[1].p_y = --l[2].p_y;  --l[2].p_x;
					l[1].p_x = --l[0].p_x;  --l[0].p_y;
					vsl_color (vdi_handle, c_lu);
					v_pline (vdi_handle, 3, (short*)l);
					l[1].p_x = --l[0].p_x;
					l[1].p_y = --l[2].p_y;
					l[3].p_x = l[2].p_x += 2;
					l[3].p_y = l[0].p_y += 1;
					vsl_color (vdi_handle, G_BLACK);
					v_pline (vdi_handle, 4, (short*)l);
				} else {
					vrt_cpyfm (vdi_handle, MD_TRANS, (short*)p, &icon, &scrn, off);
					if (ignore_colours) {
						vsf_interior (vdi_handle, FIS_PATTERN);
						vsf_style    (vdi_handle, 3);
						v_bar        (vdi_handle, (short*)(p +2));
						vsf_interior (vdi_handle, FIS_SOLID);
					}
				}
			}
		}
		p[1].p_x = area.g_x + This->TbarElem[TBAR_EDIT].Offset;
		p[2].p_x = p[1].p_x + This->TbarElem[TBAR_EDIT].Width -1;
		if (p[1].p_x <= clip.g_w && p[2].p_x >= clip.g_x) {
			TBAREDIT * edit = TbarEdit (This);
			BOOL       actv = (This->TbarActv == TBAR_EDIT);
			BOOL       bgnd;
			if (!all) {
				p[1].p_x += 2;
				p[1].p_y =  p[2].p_y -1;
				p[2].p_x -= 2;
				p[2].p_y =  p[3].p_y +1;
				bgnd = TRUE;
			} else {
				p[0].p_x = ++p[1].p_x;
				p[2].p_x--;
				p[0].p_y = p[3].p_y += 1;
				p[1].p_y = p[2].p_y -= 2;
				vsl_color (vdi_handle, G_LBLACK);
				v_pline (vdi_handle, 3, (short*)p);
				if (actv) {
					p[1].p_x = --p[0].p_x; p[3].p_x = ++p[2].p_x;
					p[1].p_y = --p[2].p_y; p[3].p_y = ++p[0].p_y;
					vsl_color (vdi_handle, G_BLACK);
					v_pline (vdi_handle, 4, (short*)p);
					p[1].p_x += 2;
					p[1].p_y += 2;
					p[2].p_x -= 2;
					p[2].p_y =  p[0].p_y -1;
					bgnd = TRUE;
				} else {
					p[1].p_x = p[2].p_x; ++p[0].p_x;
					p[1].p_y = p[0].p_y; ++p[2].p_y;
					vsl_color (vdi_handle, G_WHITE);
					v_pline (vdi_handle, 3, (short*)p);
					p[1].p_x =  p[0].p_x +1;
					p[1].p_y =  p[2].p_y;
					p[2].p_x -= 2;
					p[2].p_y =  p[0].p_y;
					bgnd = FALSE;
				}
			}
			if (bgnd) {
				v_bar (vdi_handle, (short*)(p +1));
				p[1].p_x++;
				p[2].p_x--;
			}
			p[0] = p[1];
			if (*edit->Text) {
				p[2].p_x -= p[1].p_x -1;
				p[2].p_y -= p[1].p_y -1;
				if (rc_intersect (p_clip, (GRECT*)(p +1))) {
					short dmy;
					p[2].p_x += p[1].p_x -1;
					p[2].p_y += p[1].p_y -1;
					vs_clip_pxy (vdi_handle, p +1);
					vst_font      (vdi_handle, 1);
					vst_point     (vdi_handle, 12, &dmy, &dmy, &dmy, &dmy);
					vst_effects   (vdi_handle, TXT_NORMAL);
					vst_alignment (vdi_handle, TA_LEFT, TA_TOP, &dmy, &dmy);
					vst_color     (vdi_handle, (actv ? G_BLACK : G_LBLACK));
					v_gtext       (vdi_handle, p[0].p_x, p[0].p_y,
					               edit->Text + edit->Shift);
					vst_alignment (vdi_handle, TA_LEFT, TA_BASE, &dmy, &dmy);
				}
			}
			if (actv) {
				vs_clip_pxy (vdi_handle, (PXY*)&clip);
				p[1].p_x = (p[0].p_x += (edit->Cursor - edit->Shift) *8 -1) +1;
				p[1].p_y =  p[0].p_y +16;
				vswr_mode (vdi_handle, MD_XOR);
				v_bar     (vdi_handle, (short*)p);
				vswr_mode (vdi_handle, MD_TRANS);
			}
		}
		v_show_c (vdi_handle, 1);
		vs_clip_off (vdi_handle);
	}
}

/*----------------------------------------------------------------------------*/
static BOOL
chng_toolbar (HwWIND This, UWORD on, UWORD off, WORD active)
{
	BOOL  chng;
	UWORD mask = 1;
	short lft = 0, rgt = -1;
	short i;
	
	if (This->TbarActv != active) {
		if (This->TbarActv >= 0) {
			lft = rgt = This->TbarActv;
		}
		if (active >= 0) {
			if (lft > active) lft = active;
			if (rgt < active) rgt = active;
		}
		This->TbarActv = active;
		chng = TRUE;
	} else {
		chng = FALSE;
	}
	if (on | off) for (i = 0; i < numberof(hw_buttons); i++, mask <<= 1) {
		if (This->TbarMask & mask) {
			if (off & mask) {
				This->TbarMask &= ~mask;
				if (lft > i) lft = i;
				if (rgt < i) rgt = i;
			}
		} else {
			if (on & mask) {
				This->TbarMask |= mask;
				if (lft > i) lft = i;
				if (rgt < i) rgt = i;
			}
		}
	}
	if (lft <= rgt && This->TbarH && !This->Base.isIcon && !This->shaded) {
		GRECT clip, area;
		clip.g_x = This->Work.g_x + This->TbarElem[lft].Offset -3;
		clip.g_w = This->TbarElem[rgt].Offset + This->TbarElem[rgt].Width
		         - This->TbarElem[lft].Offset +21;
		clip.g_y = This->Work.g_y;
		clip.g_h = This->TbarH -1;
		wind_get_grect (This->Base.Handle, WF_FIRSTXYWH, &area);
		while (area.g_w > 0 && area.g_h > 0) {
			if (rc_intersect (&clip, &area)) {
				draw_toolbar (This, &area, TRUE);
			}
			wind_get_grect (This->Base.Handle, WF_NEXTXYWH, &area);
		}
	}
	return chng;
}

/*----------------------------------------------------------------------------*/
static void
updt_toolbar (HwWIND This, const char * text)
{
	TBAREDIT * edit = TbarEdit (This);
	size_t     len  = strlen (text);
	
	if (len >= sizeof(edit->Text)) {
		len = sizeof(edit->Text);
	}
	strncpy (edit->Text, text, len)[len] = '\0';
	edit->Length = len;
	edit->Cursor = 0;
	edit->Shift  = 0;
	This->TbarActv = TBAR_EDIT;
	chng_toolbar (This, 0, 0, -1);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_drawWork (HwWIND This, const GRECT * clip)
{
	char * info = (!This->IbarH ? NULL :*This->Info ? This->Info :This->Stat);
	short  tbar = (!This->TbarH ? -32000 : This->Work.g_y + This->TbarH -1);
	if (clip->g_y <= tbar) {
		draw_toolbar (This, clip, TRUE);
	}
	if (info) {
		draw_infobar (This, clip, info);
	}
	containr_redraw (This->Pane, clip);
		
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static UWORD logo_data[] = {
	0x000c,0x0000, 0x0003,0xef00, 0x001f,0x7600, 0x0031,0xfb80,
	0x0031,0xf080, 0x0006,0x70c0, 0x00ff,0xec00, 0x0087,0x7fc0,
	0x0181,0xe0e0, 0x0001,0xf020, 0x005f,0xd860, 0x00ef,0x7f00,
	0x00c3,0xff80, 0x00c7,0xf080, 0x0007,0xf180, 0x0007,0xf800,
	0x000f,0xf000, 0x001f,0x7800, 0x001e,0xf800, 0x001d,0xec00,
	0x003e,0x7c00, 0x003d,0x7c00, 0x0078,0x2400, 0x00f8,0xfc00,
	0x00d8,0x2600, 0x00d0,0x7600, 0x01b1,0x2600, 0x01ff,0xfe00,
	0x0362,0x3200, 0x0260,0x3300, 0x0660,0x3300, 0x0460,0x3300
};
MFDB logo_icon = { logo_data, 32,32, 2, 0, 1, 0,0,0 };
/* - - - - - - - - - - - - - - - - - - - - - - - */
static void
vTab_drawIcon (HwWIND This, const GRECT * clip)
{
	MFDB  scrn = { NULL, };
	short color[2];
	PXY   lu ,p[4];
	GRECT work;
	
	wind_get_grect (This->Base.Handle, WF_WORKXYWH, &work);
	lu.p_x = work.g_x + (work.g_w - logo_icon.fd_w) /2;
	lu.p_y = work.g_y + (work.g_h - logo_icon.fd_h) /2;
	color[0] = (This->isBusy ? G_WHITE : G_BLACK);
	vsf_color (vdi_handle, G_LWHITE);
	p[1].p_x = (p[0].p_x = clip->g_x) + clip->g_w -1;
	p[1].p_y = (p[0].p_y = clip->g_y) + clip->g_h -1;
	v_hide_c (vdi_handle);
	v_bar (vdi_handle, (short*)p);
	p[2] = lu;
	p[3] = *(PXY*)&logo_icon.fd_w;
	if (rc_intersect (clip, (GRECT*)(p +2))) {
		p[1].p_x = (p[0].p_x = p[2].p_x - lu.p_x) + p[3].p_x -1;
		p[1].p_y = (p[0].p_y = p[2].p_y - lu.p_y) + p[3].p_y -1;
		p[3].p_x += p[2].p_x -1;
		p[3].p_y += p[2].p_y -1;
		vrt_cpyfm (vdi_handle, MD_TRANS, (short*)p, &logo_icon, &scrn, color);
	}
	v_show_c (vdi_handle, 1);
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
wnd_hdlr (HW_EVENT event, long arg, CONTAINR cont, const void * gen_ptr)
{
	HwWIND wind = hwWind_byValue (arg);
	WORD   old_busy;
	
	if (!wind) return;
	
	old_busy = wind->isBusy;
	
	switch (event) {
		
		case HW_PageCleared:
			if (wind->Active == cont) {
				wind->Active = NULL;
				wind->Input  = NULL;
			}
			break;
		
		case HW_PageUpdated:
			if (!wind->Base.isIcon) {
				hwWind_redraw  (wind, gen_ptr);
			}
			break;
		
		case HW_PageStarted:
			if (!wind->loading++) {
				if (wind->HistUsed) {
					char * flag = wind->History[wind->HistMenu]->Text;
					if (*flag == ' ') {
						if (cont->Parent) {
							*flag = '.';
						}
						if (((CONTAINR)wind->Pane)->Mode) {
							history_update (wind->Pane, wind->History[wind->HistMenu]);
						}
					}
				}
				if (!cont->Parent && wind->TbarH) {
					updt_toolbar (wind, gen_ptr);
				}
				chng_toolbar  (wind, 0, (UWORD)~TBAR_STOP_MASK, -1);
			}
			if (!cont->Parent) {
				hwWind_setName (wind, gen_ptr);
			} else {
				old_busy = ++wind->isBusy;
				hwWind_setInfo (wind, gen_ptr, TRUE);
				wind->isBusy--;
			}
			goto case_HW_ActivityBeg;
		
		case HW_SetTitle:
			if (!cont->Parent) {
				hwWind_setName (wind, gen_ptr);
				break;
			}
		case HW_SetInfo:
			hwWind_setInfo (wind, gen_ptr, TRUE);
			break;
		
		case HW_PageFinished: {
			WORD bttn_on = TBAR_REDO_MASK|TBAR_OPEN_MASK;
			if (wind->loading && !--wind->loading) {
				char flag = (!wind->HistUsed
				             ? ' ' : wind->History[wind->HistMenu]->Text[0]);
				if (flag != '*' && gen_ptr) {
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
			if (wind->HistUsed > 1) {
				if (wind->HistMenu > 0) {
					bttn_on |= TBAR_LEFT_MASK|TBAR_HOME_MASK;
				}
				if (wind->HistMenu < wind->HistUsed -1) {
					bttn_on |= TBAR_RGHT_MASK;
				}
			}
			chng_toolbar  (wind, bttn_on, 0, -1);
			hwWind_redraw (wind, gen_ptr);
			if (gen_ptr) {
				FRAME active = hwWind_setActive (wind, cont, NULL);
#if defined(GEM_MENU) && (_HIGHWIRE_ENCMENU_ == 1)
				update_menu (active->Encoding,
				             (active->MimeType == MIME_TXT_PLAIN));
#endif
			}
		}
		goto case_HW_ActivityEnd;
			
		case HW_ImgBegLoad:
			old_busy = ++wind->isBusy;
			hwWind_setInfo (wind, gen_ptr, TRUE);
			wind->isBusy--;
			goto case_HW_ActivityBeg;
		
		case HW_ImgEndLoad:
			if (!wind->Base.isIcon && gen_ptr) {
				hwWind_redraw  (wind, gen_ptr);
			}
			goto case_HW_ActivityEnd;
		
		case_HW_ActivityBeg:
			gen_ptr = NULL;
		case HW_ActivityBeg: {
			BOOL was_busy = (wind->isBusy > 0);
			wind->isBusy += (gen_ptr && *(const long *)gen_ptr > 0
			                 ? *(const long *)gen_ptr : 1);
			if (!was_busy) {
				if (wind->Base.isIcon) {
					hwWind_redraw (wind, NULL); /* update icon */
				} else {
					graf_mouse (hwWind_Mshape = BUSYBEE, NULL);
				}
				chng_toolbar (wind, TBAR_STOP_MASK, 0, -1);
			}
		}	break;
		
		case_HW_ActivityEnd:
			gen_ptr = NULL;
		case HW_ActivityEnd:
			if (gen_ptr && *(const long *)gen_ptr > 0) {
				if (wind->isBusy < *(const long *)gen_ptr) {
					wind->isBusy -= *(const long *)gen_ptr;
				} else {
					wind->isBusy = 0;
				}
			}
			if (!wind->isBusy || !--wind->isBusy) {
				if (wind->Stat[0]) {
					old_busy = wind->isBusy;
				}
				hwWind_setInfo (wind, "", TRUE);
				if (wind->Base.isIcon) {
					hwWind_redraw (wind, NULL); /* update icon */
				} else {
					short mx, my, u;
					graf_mkstate (&mx, &my, &u,&u);
					check_mouse_position (mx, my);
				}
				chng_toolbar (wind, 0, TBAR_STOP_MASK, -1);
			}
			break;
		
		default:
			errprintf ("wind_handler (%i, %p)\n", event, cont);
	}
	
	if (old_busy != wind->isBusy && !wind->Base.isIcon && wind->IbarH) {
		draw_infobar (wind, NULL, (*wind->Info ? wind->Info : wind->Stat));
	}
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
vTab_evMessage (HwWIND This, WORD msg[], PXY mouse, UWORD kstate)
{
	BOOL found = TRUE;
	BOOL check = FALSE;
	switch (msg[0]) {
		case WM_ARROWED:
			switch (msg[4]) {
				case WA_LFLINE:
					hwWind_undo (This, ((kstate & (K_RSHIFT|K_LSHIFT)) != 0));
					break;
				
				case WA_UPLINE: {
					FRAME active = hwWind_ActiveFrame (This);
					if (active) {
						hwWind_scroll (This, active->Container, 0, -scroll_step);
					}
				}	break;
				case WA_DNLINE: {
					FRAME active = hwWind_ActiveFrame (This);
					if (active) {
						hwWind_scroll (This, active->Container, 0, +scroll_step);
					}
				}	break;
			}
			break;
		case 22360: /* shaded */
			This->shaded = TRUE;
			if (This->TbarActv == TBAR_EDIT) {
				TBAREDIT * edit = TbarEdit (This);
				edit->Cursor = 0;
				edit->Shift  = 0;
				chng_toolbar (This, 0, 0, -1);
			}
			break;
		case 22361: /* unshaded */
			This->shaded = FALSE;
			break;
		case WM_CLOSED:
			hwWind_close (This, kstate);
			check = (hwWind_Top != NULL);
			break;
		
		default:
			found = FALSE;
	}
	if (check) {
		check_mouse_position (mouse.p_x, mouse.p_y);
	}
	return found;
}

/*============================================================================*/
HwWIND
hwWind_mouse  (WORD mx, WORD my, GRECT * watch)
{
	GRECT  clip;
	WORD   whdl = wind_find (mx, my);
	HwWIND wind = hwWind_byHandle (whdl);
	
	/* check wether the found window is of the correct type */
	if (wind && wind->Base.drawWork != vTab_drawWork) {
		return (NULL);
	}
	
	if (wind && !wind->Base.isIcon) {
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
			if (wind->Base.isIcon) {
				wind = NULL;
			
			} else {
				if (wind->TbarH) {
					if (my >= clip.g_y + wind->TbarH) {
						clip.g_y += wind->TbarH;
						clip.g_h -= wind->TbarH;
					} else {
						clip.g_h =  wind->TbarH;
						wind = NULL;
					}
				}
				if (wind && wind->IbarH) {
					WORD y = clip.g_y + clip.g_h - wind->IbarH;
					if (my < y) {
						clip.g_h -= wind->IbarH;
					} else {
						clip.g_y += wind->IbarH;
						clip.g_h =  wind->IbarH;
						wind = NULL;
					}
				}
				rc_intersect (&clip, watch);
			}
		}
	}
	return wind;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_evButton (HwWIND This, WORD bmask, PXY mouse, UWORD kstate, WORD clicks)
{
	WORD mx = mouse.p_x, my = mouse.p_y;
	WORD ib_x = 0, ib_y = 0, tb_n = -1;
	
	if (This->TbarH && my < This->Work.g_y + This->TbarH) {
		short x = mx - This->Work.g_x;
		tb_n = numberof(This->TbarElem) -1;
		do if (x >= This->TbarElem[tb_n].Offset) {
			if (x > This->TbarElem[tb_n].Offset + This->TbarElem[tb_n].Width) {
				tb_n = -1;
			} else if (tb_n == TBAR_EDIT ?  This->TbarMask & TBAR_STOP_MASK
			                             : !(This->TbarMask & (1 << tb_n))) {
				This = NULL;
			}
			break;
		} while (tb_n--);
	
	} else if (This->IbarH &&
		       ((ib_y = This->Work.g_y + This->Work.g_h - This->IbarH) > my ||
		        (ib_x = This->Work.g_x + This->Work.g_w - This->IbarH) > mx)) {
		ib_y = ib_x = 0;
	}
	
	if (!This) {
		short dmy;
		wind_update (BEG_MCTRL);
		evnt_button (1, 0x03, 0x00, &dmy, &dmy, &dmy, &dmy);
		wind_update (END_MCTRL);
	
	} else if (tb_n == TBAR_EDIT) {
		TBAREDIT * edit = TbarEdit (This);
		WORD x = This->Work.g_x + This->TbarElem[TBAR_EDIT].Offset;
		WORD c = edit->Cursor;
		if (This->Input) {
			WORDITEM word = input_activate (This->Input, -1);
			if (word) {
				GRECT clip;
				long  lx, ly;
				FRAME frame = (FRAME)dombox_Offset (&word->line->Paragraph->Box,
				                                    &lx, &ly);
				clip.g_x = lx + frame->clip.g_x - frame->h_bar.scroll
				              + word->h_offset;
				clip.g_y = ly + frame->clip.g_y - frame->v_bar.scroll
				              + word->line->OffsetY - word->word_height;
				clip.g_w = word->word_width;
				clip.g_h = word->word_height + word->word_tail_drop;
				hwWind_redraw (This, &clip);
			}
			This->Input = NULL;
		}
		edit->Cursor = (mx - x) /8 + edit->Shift;
		if (edit->Cursor > edit->Length) {
			edit->Cursor = edit->Length;
		}
		hwWind_raise (This, TRUE);
		if (!chng_toolbar (This, 0, 0, TBAR_EDIT) && c != edit->Cursor) {
			GRECT clip;
			clip.g_x = x;
			clip.g_w = This->TbarElem[TBAR_EDIT].Width;
			clip.g_y = This->Work.g_y;
			clip.g_h = This->TbarH;
			draw_toolbar (This, &clip, FALSE);
		}
		This = NULL;
	
	} else if (tb_n >= 0) {
		short     event = wind_update (BEG_MCTRL);
		EVMULT_IN m_in  = { MU_BUTTON|MU_M1, 1, 0x03, 0x00, MO_LEAVE, };
		EVMULT_OUT out;
		short     hist  = This->HistMenu;
		short     shift;
		chng_toolbar (This, 0, 0, tb_n);
		if (tb_n <= TBAR_RGHT) {
			m_in.emi_flags |= MU_TIMER;
			m_in.emi_tlow  =  500;
			m_in.emi_thigh =  0;
		}
		m_in.emi_m1.g_x = This->Work.g_x + This->TbarElem[tb_n].Offset;
		m_in.emi_m1.g_y = This->Work.g_y;
		m_in.emi_m1.g_w = m_in.emi_m1.g_h = This->TbarElem[tb_n].Width;
		do {
			WORD       msg[8];
			event = evnt_multi_fast (&m_in, msg, &out);
			shift = out.emo_kmeta & (K_RSHIFT|K_LSHIFT);
			if (event & MU_TIMER) {
				char * array[HISTORY_LAST];
				short  i, n = 0;
				if (tb_n) {
					for (i = hist +1; i < This->HistUsed;
					     array[n++] = This->History[i++]->Text +1);
				} else {
					for (i = hist -1; i >= 0;
					     array[n++] = This->History[i--]->Text +1);
				}
				array[n] = NULL;
				n = HW_form_popup (array, m_in.emi_m1.g_x,
				                   m_in.emi_m1.g_y + m_in.emi_m1.g_h -1, FALSE);
				if (n < 0) {
					tb_n = -1;
				} else {
					hist  += (tb_n ? +1 + n : -1 - n);
					tb_n  =  TBAR_REDO;
					shift =  TRUE;
				}
				m_in.emi_m1leave = MO_LEAVE;
				break;
			}
			if (event & MU_M1) {
				if (m_in.emi_m1leave) {
					chng_toolbar (This, 0, 0, -1);
					m_in.emi_m1leave = MO_ENTER;
				} else { /* entered */
					chng_toolbar (This, 0, 0, tb_n);
					m_in.emi_m1leave = MO_LEAVE;
				}
			}
		} while (!(event & MU_BUTTON));
		wind_update (END_MCTRL);
		if (out.emo_kmeta & K_ALT) {
			cache_clear (NULL);
			if (shift) tb_n = -1;
		}
		if (m_in.emi_m1leave) switch (tb_n) {
			case TBAR_LEFT: hwWind_undo     (This, FALSE);        break;
			case TBAR_RGHT: hwWind_undo     (This, TRUE);         break;
			case TBAR_HOME: hwWind_history  (This, 0,    FALSE);  break;
			case TBAR_REDO: hwWind_history  (This, hist, !shift); break;
			case TBAR_STOP: containr_escape (This->Pane);         break;
			case TBAR_OPEN: menu_open       (TRUE);
			default:        chng_toolbar (This, 0, 0, -1);
		}
		This = NULL;
	
	} else if (ib_y) {
		short     event = wind_update (BEG_MCTRL);
		EVMULT_IN m_in  = { MU_BUTTON|MU_M1, 1, 0x03, 0x00, MO_LEAVE, };
		PXY c[5], w[5];
		c[1].p_x = (c[0].p_x = desk_area.g_x) + desk_area.g_w -1;
		c[1].p_y = (c[0].p_y = desk_area.g_y) + desk_area.g_h -1;
		vs_clip_pxy (vdi_handle, c);
		c[0].p_x            = ib_x +1;
		c[0].p_y            = ib_y +1;
		c[1].p_x = c[1].p_y = This->IbarH;
		vsf_interior (vdi_handle, FIS_SOLID);
		vswr_mode (vdi_handle, MD_XOR);
		v_hide_c  (vdi_handle);
		wind_get_grect (This->Base.Handle, WF_FIRSTXYWH, (GRECT*)w);
		while (w[1].p_x > 0 && w[1].p_y > 0) {
			if (rc_intersect ((GRECT*)c, (GRECT*)w)) {
				w[1].p_x += w[0].p_x -1;
				w[1].p_y += w[0].p_y -1;
				v_bar (vdi_handle, (short*)w);
			}
			wind_get_grect (This->Base.Handle, WF_NEXTXYWH, (GRECT*)w);
		}
		c[0] = *(PXY*)&This->Curr;
		c[1].p_x = c[2].p_x = (c[3].p_x = c[0].p_x) + This->Curr.g_w -1;
		c[3].p_y = c[2].p_y = (c[1].p_y = c[0].p_y) + This->Curr.g_h -1;
		c[4].p_x = c[0].p_x;
		c[4].p_y = c[0].p_y +1;
		w[0] = *(PXY*)&This->Work;
		w[1].p_x = w[2].p_x = (w[3].p_x = w[0].p_x) + This->Work.g_w -1;
		w[1].p_y = w[0].p_y;
		w[3].p_y = w[2].p_y = ib_y;
		w[4].p_x = w[0].p_x;
		w[4].p_y = w[0].p_y +1;
		m_in.emi_m1.g_x = mx;
		m_in.emi_m1.g_y = my;
		m_in.emi_m1.g_w = m_in.emi_m1.g_h = 1;
		vsl_type  (vdi_handle, USERLINE);
		vsl_udsty (vdi_handle, (short)0xAAAA);
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
		vs_clip_off (vdi_handle);
		v_show_c  (vdi_handle, 1);
		vsl_type  (vdi_handle, SOLID);
		vswr_mode (vdi_handle, MD_TRANS);
		wind_update (END_MCTRL);
		c[1].p_x = c[2].p_x - c[0].p_x +1;
		c[1].p_y = c[2].p_y - c[0].p_y +1;
		window_resize (&This->Base, (GRECT*)c, FALSE);
		This = NULL;
	
	} else if (This->TbarActv == TBAR_EDIT) {
		TBAREDIT * edit = TbarEdit (This);
		edit->Shift = 0;
		chng_toolbar (This, 0, 0, -1);
	}
	if (This) {
		button_clicked (This->Pane, bmask, clicks, kstate, mouse);
	}
}


/*----------------------------------------------------------------------------*/
FILE *
open_scrap (BOOL rdNwr)
{
	char path[HW_PATH_MAX] = "";
	const char * mode = (rdNwr ? "rb" : "wb");
	FILE * file;
	
	if (scrp_read (path) <= 0 || !path[0]) {
		file = NULL;
	
	} else {
		char * scrp = strchr (path, '\0');
		if (scrp[-1] != '\\') *(scrp++) = '\\';
		if (path[0]  >= 'a')  path[0]  -= ('a' - 'A');
		strcpy (scrp, "scrap.txt");
		if ((file = fopen (path, mode)) == NULL && rdNwr) {
			strcpy (scrp, "SCRAP.TXT");
			file = fopen (path, mode);
		}
	}
	return file;
}

/*============================================================================*/
static void
vTab_evKeybrd (HwWIND This, WORD scan, WORD ascii, UWORD kstate)
{
	BOOL more = FALSE;
	
	if (!This->TbarH && !This->Input) {
		more = TRUE;
	
	} else if (This->Input) {
		GRECT    clip, rect;
		INPUT    next;
		WORD     key  = (scan << 8) | ascii;
		WORDITEM word = input_keybrd (This->Input, key, kstate, &clip, &next);
		if (word) {
			FRAME frame = containr_Frame ((CONTAINR)This->Active);
			long  x, y;
			dombox_Offset (&word->line->Paragraph->Box, &x, &y);
			x += (long)clip.g_x + word->h_offset
			     + frame->clip.g_x - frame->h_bar.scroll;
			y += (long)clip.g_y + word->line->OffsetY
			     + frame->clip.g_y - frame->v_bar.scroll;
			
			if (next != This->Input) {
				if (next) {
					PXY pxy = { 0, 0 };
					input_handle (next, pxy, &rect, NULL);
				}
				This->Input = next;
			} else {
				next = NULL;
			}
			clip.g_x = x;
			clip.g_y = y;
			hwWind_redraw (This, &clip);
			if (next) {
				long dx = 0, dy = 0, d;
				x -= rect.g_x + frame->clip.g_x;
				y -= rect.g_y + frame->clip.g_y;
				if (frame->h_bar.on) {
					d = frame->h_bar.scroll + frame->clip.g_w;
					if ((d = x + rect.g_w - frame->clip.g_w) > 0) x -= (dx = d -2);
					if ((d = x)                              < 0) x -= (dx = d -2);
				}
				if (frame->v_bar.on) {
					if ((d = y + rect.g_h - frame->clip.g_h) > 0) y -= (dy = d -2);
					if ((d = y)                              < 0) y -= (dy = d -2);
				}
				if (dx || dy) {
					hwWind_scroll (This, This->Active, dx, dy);
					d = ((rect.g_w > (dx >= 0 ? dx : -dx)) &&
					     (rect.g_h > (dx >= 0 ? dx : -dx)));
				} else {
					d = TRUE;
				}
				if (d) {
					rect.g_x = x + frame->clip.g_x;
					rect.g_y = y + frame->clip.g_y;
					hwWind_redraw (This, &rect);
				}
			}
		}
	
	} else if (This->TbarActv != TBAR_EDIT) {
		if (ascii == 27 && !containr_escape (This->Pane)
		    && !This->Base.isIcon && !This->shaded) {
			TBAREDIT * edit = TbarEdit (This);
			edit->Cursor = 0;
			edit->Shift  = 0;
			chng_toolbar (This, 0, 0, TBAR_EDIT);
			This = NULL;
		}
		more = TRUE;
	
	} else {
		TBAREDIT * edit = TbarEdit (This);
		GRECT      clip = { 0,0,0, };
		WORD       scrl = 0;
		BOOL       chng = FALSE;
		
		if (kstate & K_CTRL) switch (scan) {
			
			case 0x2E: /* ^C -- copy to clipboard */
				if (edit->Length) {
					FILE * file = open_scrap (FALSE);
					if (file) {
						fwrite (edit->Text, 1, edit->Length, file);
						fclose (file);
					}
				}
				break;
			
			case 0x2F: /* ^V -- copy from clipboard */
				if (edit->Length < sizeof(edit->Text) -1) {
					FILE * file = open_scrap (TRUE);
					if (file) {
						char buf[256];
						size_t len = sizeof(edit->Text) - edit->Length -1;
						len = fread (buf, 1, min (len, sizeof (buf)), file);
						fclose (file);
						if (len > 0) {
							char * ptr = edit->Text + edit->Cursor;
							char * src = edit->Text + edit->Length;
							char * dst = src + len;
							do {
								*(dst--) = *(src--);
							} while (src >= ptr);
							memcpy (ptr, buf, len);
							edit->Length += len;
							scrl = +len;
							chng = TRUE;
						}
					}
				}
				break;
			
			case 0x73: /* ^left */
				if (edit->Cursor) {
					short crs = edit->Cursor;
					while (--crs && isalnum (edit->Text[crs]));
					scrl = crs - edit->Cursor;
				}
				break;
			
			case 0x74: /* ^right */
				if (edit->Cursor < edit->Length) {
					short crs = edit->Cursor;
					while (++crs < edit->Length && isalnum (edit->Text[crs]));
					scrl = crs - edit->Cursor;
				}
				break;
			
			default:
				edit->Cursor = 0;
				edit->Shift  = 0;
				chng_toolbar (This, 0, 0, -1);
				more = TRUE;
			
		} else if (ascii > ' ' && ascii < 127 &&
		       (ascii < '0' || ascii > '9' || !(kstate & (K_RSHIFT|K_LSHIFT)))) {
			if (edit->Length < sizeof(edit->Text) -1) {
				char * end = edit->Text + edit->Length;
				char * dst = edit->Text + edit->Cursor;
				do {
					*(end +1) = *(end);
				} while (--end >= dst);
				*dst = ascii;
				edit->Length++;
				scrl = +1;
				chng = TRUE;
			}
		
		} else if (ascii) switch (ascii) {
			
			case 8: /* backspace */
				if (!edit->Cursor) {
					break;
				
				} else if (kstate & (K_RSHIFT|K_LSHIFT)) {
					char * dst = edit->Text;
					char * src = edit->Text + edit->Cursor;
					while ((*(dst++) = *(src++)) != '\0');
					edit->Length -= edit->Cursor;
					edit->Cursor = edit->Shift = 0;
					clip.g_w = edit->Visible *8 +1;
					break;
				
				} else if (--edit->Cursor < edit->Shift) {
					edit->Shift = edit->Cursor;
				}
			case 127: /* delete */
				if (edit->Cursor < edit->Length) {
					short  crs = edit->Cursor - edit->Shift;
					char * dst = edit->Text + edit->Cursor, * src = dst;
					if (kstate & (K_RSHIFT|K_LSHIFT)) {
						*dst = '\0';
						edit->Length = edit->Cursor;
					} else {
						while ((*(dst++) = *(++src)) != '\0');
						edit->Length--;
					}
					if (edit->Shift && edit->Length - edit->Shift < edit->Visible) {
						edit->Shift = max (edit->Length - edit->Visible, 0);
						clip.g_w = (edit->Visible +1) *8 +2;
					} else {
						clip.g_x = crs *8;
						clip.g_w = (edit->Visible - crs) *8 +2;
					}
				}
				break;
			
			case 27: /* escape */
				if (edit->Length) {
					edit->Length  = 0;
					edit->Cursor  = 0;
					edit->Shift   = 0;
					edit->Text[0] = '\0';
					clip.g_w = This->TbarElem[TBAR_EDIT].Width -4;
				}
				break;
			
			case 13: /* enter/return */
				if (edit->Length) {
					if (edit->Text[0] != '/' && strchr (edit->Text, ':') == NULL) {
						/*
						 * if no protocol, drive letter or local path is given
						 * we assume that the user means HTTP
						*/
						const char http[] = "http://";
						size_t     gap    = sizeof(http) -1;
						if (edit->Length < sizeof(edit->Text) - gap) {
							memmove (edit->Text + gap, edit->Text, edit->Length +1);
							memcpy  (edit->Text,       http,       gap);
						}
					}
					start_page_load (This->Pane, edit->Text, NULL, TRUE, NULL);
					chng_toolbar (This, 0, 0, -1);
					break;
				}
			case 9: /* tab */
				edit->Cursor = 0;
				edit->Shift  = 0;
				chng_toolbar (This, 0, 0, -1);
				break;
			
			case 52: /* shift+left */
				scrl = edit->Shift - edit->Cursor;
				break;
			
			case 54: /* shift+right */
				if (edit->Length > edit->Visible) {
					scrl = edit->Shift + edit->Visible - edit->Cursor;
					break;
				}
			case 55: /* shift+home */
				scrl = edit->Length - edit->Cursor;
				break;
		
		} else switch (scan) {
			
			case 0x4B: /* left */
				if (edit->Cursor) {
					scrl = -1;
				}
				break;
			
			case 0x4D: /*right */
				if (edit->Cursor < edit->Length) {
					scrl = +1;
				}
				break;
			
			case 0x47: /* home */
				scrl = -edit->Cursor;
				break;
			
			case 0x4F: /* end */
				scrl = edit->Length - edit->Cursor;
				break;
			
			case 0x52: /* insert */
				if (This->Active) {
					FRAME frame = containr_Frame ((CONTAINR)This->Active);
					if (frame) {
						edit->Length = location_FullName (frame->Location,
						                              edit->Text, sizeof(edit->Text));
						if (edit->Length >= sizeof(edit->Text)) {
							edit->Length = sizeof(edit->Text) -1;
						}
						if ((edit->Cursor = edit->Length) <= edit->Visible) {
							edit->Shift = 0;
						} else {
							edit->Shift = edit->Length - edit->Visible;
						}
						clip.g_w = This->TbarElem[TBAR_EDIT].Width -4;
					}
				}
				break;
			
			case 97: /* undo */
				edit->Cursor = 0;
				edit->Shift  = 0;
				chng_toolbar (This, 0, 0, -1);
				break;
		}
		
		if (scrl > 0) {
			short old_crs = edit->Cursor - edit->Shift;
			edit->Cursor += scrl;
			if (edit->Shift < edit->Cursor - edit->Visible) {
				edit->Shift = edit->Cursor - edit->Visible;
				clip.g_w    = This->TbarElem[TBAR_EDIT].Width -4;
			} else {
				clip.g_x    = old_crs *8;
				clip.g_w    = (chng && edit->Cursor < edit->Length
				               ? edit->Visible - old_crs : scrl) *8 +2;
			}
		} else if (scrl) {
			edit->Cursor += scrl;
			if (edit->Shift > edit->Cursor) {
				edit->Shift = edit->Cursor;
				clip.g_w    = This->TbarElem[TBAR_EDIT].Width -4;
			} else {
				clip.g_x    = (edit->Cursor - edit->Shift) *8;
				clip.g_w    = -scrl *8 +2;
			}
		}
		if (clip.g_w) {
			clip.g_x += This->Work.g_x + This->TbarElem[TBAR_EDIT].Offset +2;
			clip.g_y =  This->Work.g_y;
			clip.g_h =  This->TbarH;
			draw_toolbar (This, &clip, FALSE);
		}
	}
	
	if (more) {
		key_pressed (scan, ascii, kstate);
	}
}


/*============================================================================*/
FRAME
hwWind_setActive (HwWIND This, CONTAINR cont, INPUT input)
{
	FRAME active = NULL;
	
	if (This) {
		if (cont &&  This->Pane && cont->Base != This->Pane) {
			printf ("hwWind_setActive() ERROR: container not in window!\n");
			return NULL;
		}
		if (cont && (active = containr_Frame (cont)) != NULL) {
			This->Active = cont;
			This->Input  = input;
		} else {
			This->Active = NULL;
			This->Input  = NULL;
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

