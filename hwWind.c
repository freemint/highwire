/*
 ** 
 **
 ** Changes
 ** Author         Date           Desription
 ** P Slegg        14-Aug-2009    Remove the limit on paste buffer size by dynamically allocating space.
 **
 */

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
#include "dragdrop.h"
#include "bookmark.h"


#define WINDOW_t HwWIND
#include "hwWind.h"
static WINDOW vTab_destruct(HwWIND);
static BOOL vTab_evMessage (HwWIND, WORD msg[], PXY mouse, UWORD kstate);
static void vTab_evButton  (HwWIND, WORD bmask, PXY mouse, UWORD kstate, WORD);
static void vTab_evKeybrd  (HwWIND, WORD scan, WORD ascii, UWORD kstate);
static void vTab_drawWork  (HwWIND, const GRECT *);
static void vTab_drawIcon  (HwWIND, const GRECT *);
static BOOL vTab_close     (HwWIND, UWORD kstate);
static void vTab_raised    (HwWIND, BOOL topNbot);
static void vTab_moved     (HwWIND);
static BOOL vTab_sized     (HwWIND);
static void vTab_iconified (HwWIND);

#define Curr   Base.Curr


#define HISTORY_LAST 20

typedef struct s_url_hist * URLHIST;
struct s_url_hist {
	URLHIST Next;
	char    Menu[40];
	char    Link[1];
};
static URLHIST url_hist = NULL;
#define        URL_HIST_MAX 10

#define MAX_CLIPBOARD_LENGTH 4096

static WORD  info_fgnd = G_BLACK, info_bgnd = G_WHITE;
static WORD  inc_xy = 0;
static WORD  widget_b, widget_w, widget_h;
static GRECT desk_area;
static GRECT brws_curr = { -1, -1, 0, 0 };
static GRECT bmrk_curr = { -1, -1, 0, 0 };
static UWORD brws_gmsk = 0x0000u;
static UWORD bmrk_gmsk = 0x0000u;
#define GEO_CFG_XY 0xF000u /* XY read from config */
#define GEO_USR_XY 0x00F0u /* XY modified by user */
#define GEO_CFG_WH 0x0F00u /* WH read from config */
#define GEO_USR_WH 0x000Fu /* WH modified by user */
static WORD  tbar_set  = +21;
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
			0x752D, 0x752D, 0x4D6C, 0x3FF8, 0x1FF0, 0x0FE0, 0x0000 } },
#define	TBAR_EDIT 6
#define	TBAR_HIST 7
/*	{150, G_BLACK, G_GREEN,
		{	0x0000, 0x7FFC, 0x4004, 0x2008, 0x2008, 0x1010, 0x1010, 0x0820,
			0x0820, 0x0440, 0x0440, 0x0280, 0x0280, 0x0100, 0x0100 },
		{	0x0000, 0x0000, 0x3FF8, 0x1FF0, 0x1FF0, 0x0FE0, 0x0FE0, 0x07C0,
			0x07C0, 0x0380, 0x0380, 0x0100, 0x0100, 0x0000, 0x0000 } }
*/
/*	{150, G_LBLACK, G_WHITE,
		{	0x0000, 0x7FFC, 0x4004, 0x2008, 0x2008, 0x1010, 0x1010, 0x0820,
			0x0820, 0x0440, 0x0440, 0x0280, 0x0280, 0x0100, 0x0100 },
		{	0x0000, 0x0001, 0x0004, 0x0008, 0x0008, 0x0010, 0x0010, 0x0020,
			0x0020, 0x0040, 0x0040, 0x0080, 0x0080, 0x0100, 0x0100 } }
*/
	{150, G_LBLACK, G_WHITE,
		{	0x0000, 0x7FFC, 0x4004, 0x27C8, 0x2448, 0x1290, 0x1290, 0x0920,
			0x0920, 0x0440, 0x0440, 0x0280, 0x0280, 0x0100, 0x0100 },
		{	0x0000, 0x0001, 0x0004, 0x07C8, 0x0408, 0x0210, 0x0210, 0x0120,
			0x0120, 0x0040, 0x0040, 0x0080, 0x0080, 0x0100, 0x0100 } }
};
#define TBAR_LEFT_MASK (1 << TBAR_LEFT)
#define TBAR_RGHT_MASK (1 << TBAR_RGHT)
#define TBAR_HOME_MASK (1 << TBAR_HOME)
#define TBAR_REDO_MASK (1 << TBAR_REDO)
#define TBAR_OPEN_MASK (1 << TBAR_OPEN)
#define TBAR_STOP_MASK (1 << TBAR_STOP)
#define TBAR_HIST_MASK (1 << TBAR_HIST)

typedef struct {
	WORD Visible;
	WORD Shift;
	WORD Cursor;
	WORD Length;
	char Text[1024];
} TBAREDIT;
#define TbarEdit(w) ((TBAREDIT*)(w->History + HISTORY_LAST +2))


/*----------------------------------------------------------------------------*/
static BOOL
wgeo_read (char * buff, GRECT * rect)
{
	BOOL   ret = FALSE;
	char * str = buff;
	if (*str != '+') {
		WORD w = strtol (str, &str, 16);
		if (*str == 'x' && str > buff) {
			WORD h = strtol (++str, &str, 16);
			if (str > buff) {
				rect->g_w = w;
				rect->g_h = h;
				buff = str;
				ret  = TRUE;
			}
		}
	}
	if (*str == '+') {
		WORD x = strtol (++str, &str, 16);
		if (*str == '+' && str > buff +1) {
			WORD y = strtol (++str, &str, 16);
			if (str > buff +2) {
				rect->g_x = x;
				rect->g_y = y;
				buff = str;
				ret  = TRUE;
			}
		}
	}
	return ret;
}

/*----------------------------------------------------------------------------*/
static UWORD
wgeo_calc (GRECT * rect, GRECT * curr)
{
	UWORD mask = 0x0000u;
	
	if (rect->g_w > 0 && rect->g_h > 0) {
		WORD w = ((long)rect->g_w * desk_area.g_w) / 0x7FFFu;
		WORD h = ((long)rect->g_h * desk_area.g_h) / 0x7FFFu;
		if ((curr->g_w = max (w, inc_xy *7)) > desk_area.g_w) {
			curr->g_w = desk_area.g_w;
		}
		if ((curr->g_h = max (h, inc_xy *6)) > desk_area.g_h) {
			curr->g_h = desk_area.g_h;
		}
		mask     |= GEO_CFG_WH;
		if (curr->g_w != w || curr->g_h != h) {
			mask  |= GEO_USR_WH;
		}
	}
	if (rect->g_x >= 0 && rect->g_y >= 0) {
		curr->g_x = ((long)rect->g_x * (desk_area.g_w - desk_area.g_x))
		            / 0x7FFFu + desk_area.g_x;
		curr->g_y = ((long)rect->g_y * (desk_area.g_h - desk_area.g_y))
		            / 0x7FFFu + desk_area.g_y;
		mask     |= GEO_CFG_XY;
		if (curr->g_x + curr->g_w > desk_area.g_x + desk_area.g_w) {
			curr->g_x = desk_area.g_x + desk_area.g_w - curr->g_w;
			if (curr->g_x < desk_area.g_x) curr->g_x = desk_area.g_x;
			mask  |= GEO_USR_XY;
		}
		if (curr->g_y + curr->g_h > desk_area.g_y + desk_area.g_h) {
			curr->g_y = desk_area.g_y + desk_area.g_h - curr->g_h;
			if (curr->g_y < desk_area.g_y) curr->g_y = desk_area.g_y;
			mask  |= GEO_USR_XY;
		}
	}
	*rect = *curr;
	
	return mask;
}

/*----------------------------------------------------------------------------*/
static BOOL
wgeo_write (const GRECT * rect, UWORD mask, char * buff)
{
	char * p = buff;
	if ((mask & GEO_USR_WH) || ((mask & GEO_CFG_WH) && (mask & GEO_USR_XY))) {
		WORD w = ((long)rect->g_w * 0x7FFF) / desk_area.g_w;
		WORD h = ((long)rect->g_h * 0x7FFF) / desk_area.g_h;
		sprintf (p, "%04hXx%04hX", w, h);
		p = strchr (p, '\0');
	}
	if ((mask & GEO_USR_XY) || ((mask & GEO_CFG_XY) && (mask & GEO_USR_WH))) {
		WORD x = ((long)(rect->g_x - desk_area.g_x) * 0x7FFF)
		         / (desk_area.g_w - desk_area.g_x);
		WORD y = ((long)(rect->g_y - desk_area.g_y) * 0x7FFF)
		         / (desk_area.g_h - desk_area.g_y);
		sprintf (p, "+%04hX+%04hX", x, y);
		p = strchr (p, '\0');
	}
	return (p > buff);
}


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
		
		case HWWS_GEOMETRY:
			wgeo_read ((char*)arg, &brws_curr);
			break;

		case HWWS_BOOKMGEO:
			wgeo_read ((char*)arg, &bmrk_curr);
			break;
	}
}

/*============================================================================*/
void
hwWind_store (HWWIND_SET set)
{
	switch (set) {
		
		case HWWS_INFOBAR:
		case HWWS_TOOLBAR:
			break;   /* not in use yet */
		
		case HWWS_GEOMETRY: {
			char buff[30];
			if (wgeo_write (&brws_curr, brws_gmsk, buff)) {
				save_config ("BRWSR_GEO", buff);
			}
		}	break;

		case HWWS_BOOKMGEO: {
			char buff[30];
			if (wgeo_write (&bmrk_curr, bmrk_gmsk, buff)) {
				save_config ("BOOKM_GEO", buff);
			}
		}	break;
	}
}


/*============================================================================*/
HwWIND
new_hwWind (const char * name, const char * url, BOOL topNbot)
{
	HwWIND This = malloc (sizeof (struct hw_window) +
	                      sizeof (HISTORY) * HISTORY_LAST +
	                      sizeof (TBAREDIT) +1);
	TBAREDIT * edit;
	short      i;
	GRECT      curr;
	ULONG      ident = (url && url == bkm_File ? WIDENT_BMRK : WIDENT_BRWS);
	                                       /* special case for bookmark window */
	if (!inc_xy) {
		wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &desk_area);
		wind_calc_grect (WC_BORDER, VSLIDE|HSLIDE, &desk_area, &curr);
		widget_b = desk_area.g_x - curr.g_x;
		widget_w = curr.g_w - desk_area.g_w;
		widget_h = curr.g_h - desk_area.g_h;
		inc_xy   = max (widget_w, widget_h) - widget_b;
		if (desk_area.g_w < 800) {
			curr = desk_area;
			curr.g_w = (desk_area.g_w *2) /3;
		} else {
			curr.g_x = desk_area.g_x + inc_xy;
			curr.g_y = desk_area.g_y + inc_xy;
			curr.g_w = (desk_area.g_w *3) /4;
			curr.g_h = (desk_area.g_h *3) /4;
		}
		brws_gmsk = wgeo_calc (&brws_curr, &curr);
		
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
		
	} else if (ident == WIDENT_BMRK) {
		static BOOL __once = TRUE;
		if (__once) {
			__once = FALSE;
			curr.g_h = desk_area.g_h;
			curr.g_y = desk_area.g_y;
			curr.g_w = max (curr.g_h /2, 200);
			curr.g_x = desk_area.g_x + desk_area.g_w - curr.g_w;
			bmrk_gmsk = wgeo_calc (&bmrk_curr, &curr);
		} else {
			curr = bmrk_curr;
		}
	} else {
		brws_curr.g_x += inc_xy;
	  	if (brws_curr.g_x + brws_curr.g_w > desk_area.g_x + desk_area.g_w) {
  			brws_curr.g_x = desk_area.g_x;
  		}
		brws_curr.g_y += inc_xy;
	   if (brws_curr.g_y + brws_curr.g_h > desk_area.g_y + desk_area.g_h) {
	   	brws_curr.g_y = desk_area.g_y;
	   }
		curr = brws_curr;
	}
	
	window_ctor (This, wind_kind, ident,
	             (name && *name ? name : url), NULL, FALSE);
	This->Base.destruct  = vTab_destruct;
	This->Base.evMessage = vTab_evMessage;
	This->Base.evButton  = vTab_evButton;
	This->Base.evKeybrd  = vTab_evKeybrd;
	This->Base.drawWork  = vTab_drawWork;
	This->Base.drawIcon  = vTab_drawIcon;
	This->Base.close     = vTab_close;
	This->Base.raised    = vTab_raised;
	This->Base.moved     = vTab_moved;
	This->Base.sized     = vTab_sized;
	This->Base.iconified = vTab_iconified;
	
	This->shaded   = FALSE;
	This->isBusy   = 0;
	This->isDone   = 0;
	This->loading  = 0;
	This->Location = NULL;
	This->Pane     = new_containr (NULL);
	This->Active   = NULL;
	This->Input    = NULL;
	containr_register (This->Pane, wnd_hdlr, (long)This);
	
	This->HistUsed = 0;
	This->HistMenu = 0;
	for (i = 0; i <= HISTORY_LAST; This->History[i++] = NULL);
	
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
	This->TbarH    = (tbar_set > 0 && ident == WIDENT_BRWS ? tbar_set : 0);
	This->TbarMask = (url_hist ? TBAR_HIST_MASK : 0) | TBAR_OPEN_MASK;
	This->TbarActv = (This->TbarH && !url ? TBAR_EDIT : -1);
	for (i = 0; i < numberof(hw_buttons)-1; i++) {
		This->TbarElem[i].Offset = hw_buttons[i].Offset;
		This->TbarElem[i].Width  = 21;
	}
	This->TbarElem[TBAR_EDIT].Offset = This->TbarElem[TBAR_EDIT -1].Offset
	                                 + This->TbarElem[TBAR_EDIT -1].Width *3 /2;
	This->TbarElem[TBAR_HIST].Width  = 21;
	edit = TbarEdit (This);
	edit->Text[0] = '\0';
	edit->Length  = 0;
	edit->Shift   = 0;
	edit->Cursor  = 0;

	window_setBevent (&This->Base);
	
	if (info_bgnd & 0x0080) {
		wind_set(This->Base.Handle, WF_COLOR, W_HBAR,   info_bgnd, info_bgnd, -1);
		wind_set(This->Base.Handle, WF_COLOR, W_HSLIDE, info_bgnd, info_bgnd, -1);
	}
	This->Work.g_w = This->Work.g_h = 0; /* mark for inital resize */
	window_raise (&This->Base, topNbot, &curr);
	hwWind_redraw (This, NULL);

	if (url && *url) {
		start_page_load (This->Pane, url, NULL, TRUE, NULL);
	}
	
#ifdef GEM_MENU
	menu_history (NULL,0,0);
#endif

	if (cfg_AVWindow) {
		send_avwinopen(This->Base.Handle);
	}
	
	return This;
}

/*============================================================================*/
static WINDOW
vTab_destruct (HwWIND This)
{
	short i;
	
	if (hwWind_Focus == This) {
		hwWind_Focus = NULL;
	}
	
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
	
	free_location   (&This->Location);
	delete_containr ((CONTAINR*)&This->Pane);

	return window_dtor(This);
}


/*----------------------------------------------------------------------------*/
static void
draw_busybar (HwWIND This, const GRECT * area, const GRECT * clip)
{
	GRECT rect;
	PXY   p[3];
	WORD  width = area->g_w -4 - This->IbarH;
	p[2].p_x = 128       +2;
	p[2].p_y = area->g_h -4;
	p[1].p_x = area->g_x +2 + width - (p[2].p_x -1);
	p[1].p_y = area->g_y +2;
	rect = *(GRECT*)(p +1);
	if (rc_intersect (clip, &rect)) {
		p[0].p_x = p[1].p_x;
		p[0].p_y = p[1].p_y + p[2].p_y -2;
		p[2].p_x = p[1].p_x + p[2].p_x -2;
		p[2].p_y = p[1].p_y;
		vsl_color (vdi_handle, G_LBLACK);
		v_pline   (vdi_handle, 3, (short*)p);
		p[1].p_x = ++p[2].p_x;   p[0].p_x++;
		p[1].p_y = ++p[0].p_y;   p[2].p_y++;
		vsl_color (vdi_handle, G_WHITE);
		v_pline   (vdi_handle, 3, (short*)p);
		if (This->isBusy > 1) {
			width = ((This->isDone +1) *128) / This->isBusy;
			if      (width >128) width = 128;
			else if (width < 1)  width = 1;
			p[1].p_x--;
			p[1].p_y--;
			p[0].p_x = p[1].p_x - width +1;
			p[0].p_y = p[2].p_y;
			vsf_color (vdi_handle, G_BLUE);
			v_bar     (vdi_handle, (short*)p);
		}
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
	vst_map_mode  (vdi_handle, MAP_UNICODE);
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
	vst_map_mode  (vdi_handle, MAP_UNICODE);
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

	if (This->Base.Ident == WIDENT_BRWS) {
		brws_curr.g_x = This->Curr.g_x;
		brws_curr.g_y = This->Curr.g_y;
		brws_gmsk    |= GEO_USR_XY;
	} else { /* WIDENT_BMRK */
		bmrk_curr.g_x = This->Curr.g_x;
		bmrk_curr.g_y = This->Curr.g_y;
		bmrk_gmsk    |= GEO_USR_XY;
	}
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
vTab_sized (HwWIND This)
{
	TBAREDIT * edit = TbarEdit (This);
	GRECT      work;
	
	if (This->Base.isScrn) {
		if (This->Base.isFull) { /* set to mode */
			This->IbarH = 0;
		
		} else {                 /* back to normal mode */
			char * info = (This->Info[0] ? This->Info : This->Stat);
			if (wind_kind & INFO) {
				wind_set_str (This->Base.Handle, WF_INFO, info);
			}
			if (wind_kind & (LFARROW|HSLIDE)) {
				hwWind_setHSInfo(This, info);
			} else {
				This->IbarH = widget_h - widget_b -1;
			}
			wind_set_str (This->Base.Handle, WF_NAME, This->Base.Name);
			window_setBevent (&This->Base);
		}
		
	} else if (!This->Base.isFull) {
		BOOL mask = (This->Work.g_w > 0 && This->Work.g_h > 0 ? GEO_USR_WH : 0);
		if (This->Base.Ident == WIDENT_BRWS) {
			brws_curr.g_w = This->Curr.g_w;
			brws_curr.g_h = This->Curr.g_h;
			brws_gmsk    |= mask;
		} else if (This->Base.Ident == WIDENT_BMRK) {
			bmrk_curr.g_w = This->Curr.g_w;
			bmrk_curr.g_h = This->Curr.g_h;
			bmrk_gmsk    |= mask;
		}
	}
	wind_get_grect (This->Base.Handle, WF_WORKXYWH, &This->Work);
	work = This->Work;
	work.g_y += This->TbarH;
	work.g_h -= This->TbarH + This->IbarH;
	containr_calculate (This->Pane, &work);
	
	work.g_w -= This->TbarElem[TBAR_EDIT].Offset +6
	          + This->TbarElem[TBAR_HIST].Width;
	if ((edit->Visible = work.g_w /8) < 7) {
		edit->Visible = 7;
	}
	This->TbarElem[TBAR_EDIT].Width  = edit->Visible *8 +6;
	This->TbarElem[TBAR_HIST].Offset = This->TbarElem[TBAR_EDIT].Offset
	                                 + This->TbarElem[TBAR_EDIT].Width -1;
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


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
vTab_close (HwWIND This, UWORD state)
{
	if (!(state & K_ALT) || This->Base.Ident != WIDENT_BRWS) {
		return TRUE; /* to be deleted */
	
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
	return FALSE;
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
		This->History[menu] = history_create (This->Pane, This->Base.Name, NULL);
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

/*============================================================================*/
HwWIND
hwWind_byType (WORD val)
{
	HwWIND wind = hwWind_Top;
	ULONG  ident;
	
	if (val == 0) {
		ident = WIDENT_BRWS;
	} else {
		ident = WIDENT_BMRK;
	}
	while (wind && wind->Base.Ident != ident) {
		wind = hwWind_Next (wind);
	}

	if (wind->Base.Ident != ident) {
		wind = NULL;
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
			for (i = 0; i < numberof(hw_buttons)+1; i++) if (i != TBAR_EDIT) {
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
				btn++;
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
	if (on | off) for (i = 0; i < numberof(This->TbarElem); i++, mask <<= 1) {
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


/*----------------------------------------------------------------------------*/
static void
update_urlhist (void)
{
	char    k[] = "URL_00";
	URLHIST ent = url_hist;
	short   i   = 0;
	while (ent && ++i <= URL_HIST_MAX) {
		if (++k[5] > '9') {
			k[5] = '0';
			k[4]++;
		}
		save_config (k, ent->Link);
		ent = ent->Next;
	}
}

/*----------------------------------------------------------------------------*/
static void
select_urlhist (HwWIND This)
{
	char  * array[URL_HIST_MAX +1];
	URLHIST ent = url_hist;
	short   n   = 0;
	WORD    x   = This->Work.g_x + This->TbarElem[TBAR_HIST].Offset
	                             + This->TbarElem[TBAR_HIST].Width +1;
	WORD    y   = This->Work.g_y + This->TbarElem[TBAR_HIST].Width -1;
	while (ent) {
		array[n] = ent->Menu;
		if (++n == URL_HIST_MAX) break;
		ent = ent->Next;
	}
	array[n] = NULL;
	if ((n = HW_form_popup (array, -x, y, FALSE)) >= 0) {
		ent = url_hist;
		if (n) {
			URLHIST * pptr = &ent->Next;
			while (--n) pptr = &(*pptr)->Next;
			ent       = *pptr;
			*pptr     = ent->Next;
			ent->Next = url_hist;
			url_hist  = ent;
			update_urlhist();
		}
		start_page_load (This->Pane, ent->Link, NULL, TRUE, NULL);
	} else {
		chng_toolbar (This, 0, 0, -1);
	}
}

/*============================================================================*/
void
hwWind_urlhist (HwWIND This, const char * link)
{
	size_t  len = strlen (link);
	URLHIST ent = (This ? url_hist : NULL);
	
	if (ent && strcmp (ent->Link, link) != 0) {
		URLHIST * pptr = &ent->Next;
		while ((ent = *pptr) != NULL) {
			if (strcmp (ent->Link, link) == 0) {
				*pptr = ent->Next;
				break;
			}
			pptr = &ent->Next;
		}
	}
	if (!ent && (ent = malloc (sizeof (struct s_url_hist) + len)) != NULL) {
		ent->Menu[0] = ' ';
		if (len <= sizeof(ent->Menu) -3) {
			strcpy (ent->Menu +1, link);
		} else {
			memcpy (ent->Menu +1, link, sizeof(ent->Menu) -3);
			ent->Menu[sizeof(ent->Menu) -2] = '¯';
			ent->Menu[sizeof(ent->Menu) -1] = '\0';
		}
		memcpy (ent->Link, link, len +1);
	}
	if (ent) {
		if (!This) {
			URLHIST * pptr = &url_hist;
			while (*pptr) pptr = &(*pptr)->Next;
			*pptr     = ent;
			ent->Next = NULL;
			return;
		}
		if (!url_hist) {
			HwWIND wind = hwWind_Top;
			while (wind) {
				chng_toolbar (wind, TBAR_HIST_MASK, 0, -1);
				wind = hwWind_Next (wind);
			}
		}
		if (ent != url_hist) {
			ent->Next = url_hist;
			url_hist  = ent;
			update_urlhist();
		}
	}
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

/*----------------------------------------------------------------------------*/
/* This is the old icon */
#if 0
static UWORD oldlogo_data[] = {
	0x000c,0x0000, 0x0003,0xef00, 0x001f,0x7600, 0x0031,0xfb80,
	0x0031,0xf080, 0x0006,0x70c0, 0x00ff,0xec00, 0x0087,0x7fc0,
	0x0181,0xe0e0, 0x0001,0xf020, 0x005f,0xd860, 0x00ef,0x7f00,
	0x00c3,0xff80, 0x00c7,0xf080, 0x0007,0xf180, 0x0007,0xf800,
	0x000f,0xf000, 0x001f,0x7800, 0x001e,0xf800, 0x001d,0xec00,
	0x003e,0x7c00, 0x003d,0x7c00, 0x0078,0x2400, 0x00f8,0xfc00,
	0x00d8,0x2600, 0x00d0,0x7600, 0x01b1,0x2600, 0x01ff,0xfe00,
	0x0362,0x3200, 0x0260,0x3300, 0x0660,0x3300, 0x0460,0x3300
};
#endif

static UWORD logo1_data[] = {
	0x0000,0x0000, 0x0000,0x7f00, 0x0000,0xffc0, 0x01ff,0xfffc,
	0x0100,0x9804, 0x0300,0xe804, 0x07c3,0xf61c, 0x0dc3,0x7a1c,
	0x1bc2,0xbe1e, 0x37c3,0x761e, 0x2fc2,0xae1e, 0x6fc3,0x5e1f,
	0x5f43,0xfe1f, 0x5ac0,0x001f, 0x5d40,0x001f, 0x5ec0,0x001f,
	0xf543,0xfe1f, 0xdac2,0x9e1f, 0xff43,0x7e1f, 0x5fc2,0xbe1e,
	0x5fc3,0x7e1e, 0x5fc2,0xbe1e, 0x6fc3,0x5e1c, 0x2fc3,0xbe1e,
	0x3700,0xf804, 0x1b00,0xe806, 0x0dff,0x9ffe, 0x0600,0x7fd4,
	0x03ff,0xfc00, 0x00ff,0x6000, 0x0000,0x0000, 0x0000,0x0000
};
MFDB logo1_icon = { logo1_data, 32,32, 2, 0, 1, 0,0,0 };

static UWORD logo4_data[] = {
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x7f00, 0x0000,0xffc0,
	0x0000,0xc000, 0x0080,0x8000, 0x0080,0xe000, 0x0381,0xb10c,
	0x0684,0xd80c, 0x0c80,0x7404, 0x1884,0x1604, 0x30a0,0x3206,
	0x2084,0x2246, 0x31a0,0x0003, 0x7b01,0x7401, 0xac20,0x0001,
	0xaa07,0xff01, 0xac20,0x7e01, 0xba04,0x4201, 0xb220,0xc201,
	0xb304,0xc241, 0x7180,0x7a01, 0x1084,0x0e03, 0x1080,0x0302,
	0x0c00,0x0e06, 0x0600,0x0802, 0x0200,0x3006, 0x0000,0xe800,
	0x0000,0x1ff0, 0x0000,0x0000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x7f00, 0x0000,0xffc0,
	0x0000,0xc000, 0x0080,0xe000, 0x0280,0xf000, 0x0781,0xf90c,
	0x0f84,0xfc0c, 0x1f80,0x7e04, 0x3f84,0x1e04, 0x7fa1,0xbe06,
	0x7f85,0x3e46, 0x7fa0,0x0003, 0x7f81,0x7401, 0xaf20,0x0001,
	0xae07,0xff01, 0xac22,0x7e01, 0xbe06,0x7e01, 0xbe22,0xfe01,
	0xbf04,0xfe41, 0x7f80,0x7e01, 0x7f84,0x0e03, 0x3f80,0x0302,
	0x1e00,0x7e06, 0x0e00,0xf802, 0x0600,0xf806, 0x0200,0xf800,
	0x01ff,0xfff0, 0x007f,0x8000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x7f00, 0x0000,0xffc0,
	0x0000,0xf800, 0x0080,0x9800, 0x0080,0xe800, 0x0383,0xb70c,
	0x0687,0xda0c, 0x0c83,0xf40c, 0x1887,0xf60c, 0x30a3,0xf20e,
	0x2087,0xe24e, 0x31a0,0x000f, 0x7b81,0x740f, 0xffa0,0x000f,
	0xff87,0xff0f, 0xffa3,0xfe0f, 0xff87,0xc20f, 0xf3a3,0xc20f,
	0xf387,0xc24f, 0x7183,0xfa0f, 0x1087,0xfe0f, 0x1083,0xff0e,
	0x0c00,0xfe06, 0x0600,0xf802, 0x0200,0xf006, 0x0000,0xe800,
	0x0000,0x1ff0, 0x0000,0x0000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x7f00, 0x0000,0xffc0,
	0x01ff,0xfffc, 0x0101,0x9c04, 0x0101,0xec04, 0x03c5,0xf61c,
	0x06c0,0xfb1c, 0x0cc4,0x7d1c, 0x1dc0,0x1f1c, 0x3bc4,0x3b1e,
	0x3fc0,0x331e, 0x3fc7,0xff1f, 0x7f40,0x001f, 0xec40,0x001f,
	0xea40,0x001f, 0xec44,0x7f1f, 0xfa40,0x7f1f, 0xfe44,0xcf1f,
	0xff40,0xc71f, 0x77c4,0x7b1f, 0x13c0,0x0f1f, 0x1bc4,0x031e,
	0x0fc7,0x0f1e, 0x0701,0x0c06, 0x0301,0x3406, 0x01ff,0xeffc,
	0x007f,0x1ff0, 0x0000,0x0000, 0x0000,0x0000, 0x0000,0x0000
};
MFDB logo4_icon = { logo4_data, 32,32, 2, 1, 4, 0,0,0 };

static UWORD logo8_data[] = {
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0xfe80, 0x0004,0x7700,
	0x0154,0xee96, 0x00d5,0x1d14, 0x022a,0xa6ac, 0x02d7,0x1550,
	0x03ae,0x3aa8, 0x1b16,0x8548, 0x322c,0x77ac, 0x2056,0xff56,
	0x2d2c,0x6bee, 0x5cf6,0xd05f, 0x71eb,0xdeb2, 0x17f4,0x055b,
	0x20ea,0x88b7, 0x1937,0x345e, 0x88ad,0x23be, 0x5077,0x015b,
	0x08ad,0x83ee, 0x4417,0x8355, 0x0ead,0x26ae, 0x1617,0xea48,
	0x13eb,0x77b8, 0x0954,0xdb56, 0x04ab,0xaaa8, 0x013e,0x47fc,
	0x004a,0x7bf8, 0x0038,0x4000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x8080, 0x0004,0x4880,
	0x01ab,0x3f6a, 0x01d5,0x2910, 0x022a,0xcaa8, 0x0716,0x5058,
	0x096c,0xe2ac, 0x0a57,0xc240, 0x002f,0xfaaa, 0x0f97,0xf44a,
	0x02ef,0xe0e8, 0x0375,0xee49, 0x362b,0xdeab, 0x5c74,0x0545,
	0x27ea,0x00aa, 0x4bf5,0xc240, 0xcdef,0x0eaa, 0xfdb7,0x5e4c,
	0x146f,0x0eeb, 0x41d7,0x4c49, 0x22ef,0xaaaa, 0x05d5,0xfc40,
	0x00a9,0x7bb8, 0x0054,0xe752, 0x01ab,0xdaa8, 0x0141,0x0c00,
	0x00b4,0x17f8, 0x0008,0x0000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x4000, 0x0000,0x8000,
	0x0054,0x3e14, 0x0000,0xec04, 0x0000,0x9404, 0x0241,0x4800,
	0x0483,0xe408, 0x0d01,0x1104, 0x2e03,0xfb08, 0x36c1,0xf112,
	0x36c3,0xeb18, 0x6503,0x5018, 0x4200,0x201a, 0x3c40,0x001d,
	0x02c0,0x0819, 0x2241,0xd619, 0xf903,0x3119, 0x6341,0x2919,
	0xfd43,0x811a, 0x3b81,0x6912, 0x1207,0xaa0a, 0x1d81,0xd804,
	0x1cc3,0x7a08, 0x0e00,0x4004, 0x0600,0x8000, 0x013e,0xb7fc,
	0x0051,0xf800, 0x002f,0x4000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0xbe80, 0x0004,0x3f80,
	0x01ab,0xabea, 0x01d5,0x1910, 0x022a,0xeaa8, 0x0796,0x0458,
	0x0a6d,0x58ac, 0x0857,0x5648, 0x102d,0xfaae, 0x0917,0xf64e,
	0x0c2d,0xc0f6, 0x15f4,0xae4f, 0x0bab,0xdeab, 0x5bb4,0x0543,
	0x0caa,0x80a8, 0x5134,0xa042, 0xc4ed,0x92a8, 0xec37,0x504e,
	0x112d,0x02f7, 0x4457,0x024d, 0x2eed,0xa2ae, 0x0655,0xde48,
	0x03a8,0xf7b8, 0x0154,0xdf52, 0x01ab,0xf2a8, 0x0141,0x4c00,
	0x00be,0x17f8, 0x0018,0x0000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0xbf80, 0x0004,0x7fc0,
	0x01ab,0xadea, 0x01d5,0x6912, 0x02ab,0x12aa, 0x0695,0x495c,
	0x0cef,0xfdb4, 0x0dd7,0x5458, 0x1baf,0xf8b6, 0x1457,0xf64c,
	0x3eef,0xeeee, 0x2575,0xff4f, 0x2a2b,0xfea9, 0x7c74,0x0546,
	0x2eeb,0x76aa, 0x7175,0xf742, 0x2aef,0x82aa, 0xea77,0x204e,
	0xaa6f,0xf2ed, 0x7657,0x324d, 0x37ef,0xa9b4, 0x1e55,0xdf58,
	0x0f29,0xfdb4, 0x07d5,0xdf52, 0x03ab,0x66ae, 0x0041,0x3000,
	0x00c1,0xe7f8, 0x001f,0x0000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0xfe80, 0x0004,0xff80,
	0x01ff,0x91fe, 0x01d5,0x8514, 0x022a,0x6eac, 0x07d6,0x1458,
	0x0b6c,0x1aa4, 0x0ad6,0x474c, 0x35ac,0x01a6, 0x2936,0x035c,
	0x082c,0x05f6, 0x5076,0xae57, 0x502b,0xfeb1, 0x5034,0x055a,
	0x042a,0x08b3, 0x5034,0x205b, 0x166c,0x0db3, 0xfcb6,0x5757,
	0xc2ac,0x7df5, 0x4c56,0x175f, 0x2f68,0x20a4, 0x0654,0x064c,
	0x136a,0x05b0, 0x0954,0x9f56, 0x05ab,0x7aa8, 0x007f,0xcbfc,
	0x00ae,0x0ff8, 0x0038,0x4000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x4000, 0x0000,0x8000,
	0x0054,0x4014, 0x0000,0x9404, 0x0100,0x6c04, 0x0142,0xb600,
	0x0300,0x0200, 0x1280,0xab04, 0x25c0,0x0500, 0x2ba0,0x0910,
	0x0100,0x1510, 0x5a82,0x0010, 0x55c0,0x0014, 0x0380,0x0010,
	0x5100,0x0815, 0x0e82,0x0815, 0x1700,0x7115, 0x1500,0xd911,
	0x4700,0x7110, 0x0980,0xd912, 0x0904,0x5400, 0x0182,0x2004,
	0x1042,0x0000, 0x0800,0x2004, 0x0400,0x1800, 0x00be,0xcbfc,
	0x002e,0x0800, 0x0020,0xc000, 0x0000,0x0000, 0x0000,0x0000,
	0x0000,0x0000, 0x0000,0x0000, 0x0000,0x8180, 0x0004,0x0040,
	0x0100,0x0002, 0x00d5,0x6112, 0x02ab,0x12aa, 0x0614,0x0954,
	0x0c2c,0x05b4, 0x1c14,0x0050, 0x382c,0x00b2, 0x3034,0x0042,
	0x302c,0x00e0, 0x6034,0x0141, 0x602b,0xfea1, 0x6034,0x0541,
	0x602b,0xf6a0, 0x6034,0x0140, 0xe02c,0x00a0, 0x2234,0x2040,
	0xb02c,0x00e1, 0x7214,0x2041, 0x302c,0x01b2, 0x1814,0x0150,
	0x1c28,0x01b4, 0x0ed5,0x0352, 0x06ab,0x06ae, 0x0100,0x3000,
	0x00d1,0xf7f8, 0x003f,0xc000, 0x0000,0x0000, 0x0000,0x0000 
};
MFDB logo8_icon = { logo8_data, 32,32, 2, 1, 8, 0,0,0 };

static UWORD mask_data[] = {
	0x0000,0x0000, 0x0000,0x7f00, 0x0000,0xffc0, 0x01ff,0xfffc,
	0x01ff,0xfffe, 0x03ff,0xfffe, 0x07ff,0xfffe, 0x0fff,0xfffc,
	0x1fff,0xfffc, 0x3fff,0xfffe, 0x3fff,0xfffe, 0x7fff,0xffff,
	0x7fff,0xffff, 0x7fff,0xffff, 0x7fff,0xffff, 0xffff,0xffff,
	0xffff,0xffff, 0xffff,0xffff, 0xffff,0xffff, 0xffff,0xffff,
	0xffff,0xffff, 0x7fff,0xffff, 0x7fff,0xfffe, 0x3fff,0xfffe,
	0x3fff,0xfffe, 0x1fff,0xfffe, 0x0fff,0xfffe, 0x07ff,0xfffe,
	0x03ff,0xfffc, 0x00ff,0xf800, 0x001f,0x8000, 0x0000,0x0000
};
MFDB logo_mask = { mask_data, 32,32, 2, 0, 1, 0,0,0 };

MFDB logo_icon;

/*----------------------------------------------------------------------------*/
static WORD 
TC_trans (MFDB *src)
{
	WORD i, bit, color, mask;
	WORD first_plane[32], *plane, *idx, *new_addr;
	WORD *color_table, *bit_location, *temp_addr;
	WORD tot_colors = (1 << src->fd_nplanes);
	char used_colors[256];
	WORD x, y;
	MFDB tempscreen;
	MFDB temp;
	WORD pxy[8], colors[2];
	LONG temp_size, size;

	size = (LONG)(src->fd_wdwidth * src->fd_h);
	
	/* memory for the device raster */
	if( (new_addr = (short *) malloc( (size << 1) * planes )) == NULL )
		return( FALSE );

	memset(new_addr,0,((size << 1) * planes));	/*	-> fill with 0-planes */

	/* fill in the descriptions	 */
	src->fd_nplanes = planes;
	tempscreen = *src;		/* copy MFDB */
	src->fd_stand = 1;	/* standard format */
	tempscreen.fd_addr = new_addr;

	temp = *src;
	temp.fd_stand = 0;
	temp.fd_nplanes = 1;
	temp.fd_h = tot_colors;

	temp_size = tot_colors * temp.fd_wdwidth;
	temp_size <<= 1;

	if( (temp_addr = (short *) malloc(temp_size)) == NULL )
		return( FALSE );

	memset(temp_addr,255,(temp_size));	/*	-> fill with 0-planes */

	temp.fd_addr = temp_addr;

	pxy[0] = pxy[4] = 0;
	pxy[1] = pxy[5] = 0;
	pxy[2] = pxy[6] = src->fd_w-1;
	pxy[3] = pxy[7] = src->fd_h-1;

	/* If you don't do the following monochrome srcs are inverted */
	colors[1] = 0;

	mask = tot_colors - 1;

	idx = (short *)src->fd_addr;

	bit_location = color_table = (short *)temp_addr;

	for (y = 0; y < src->fd_h; y++)
	{
		memset(temp_addr,0,temp_size);
		memset( used_colors, 0, sizeof( used_colors ) );
	
		for (x = 0; x < src->fd_wdwidth; x++)
		{
			/* go through all bitplanes */
			plane = first_plane;

			/* get one word from a bitplane */
			for( i = 0; i < src->fd_nplanes; i ++ )
				*(plane ++) = *(idx + size * (long)i);

			/* go through one word */
			for( bit = 15; bit >= 0; bit -- )
			{
				color = 0;
				plane = first_plane;

				/* OR all 'bit' bits from all bitplanes together */
				for( i = 0; i < src->fd_nplanes; i ++ )
					color |= ((*(plane ++) >> bit) & 1) << i;

				color &= mask;

				used_colors[color] = 1;

				bit_location =  (short *)(color_table + (temp.fd_wdwidth * (long)color) + x);
	
				*bit_location |= 1 << bit;
			}

			idx++; /* increment idx */
		}

		/* if we've gone to the end of a row update now */

		pxy[5] = pxy[7] = y;

		for (i=0;i<tot_colors;i++)
		{
			if(used_colors[i])
			{
				colors[0] = i;
				pxy[1] = pxy[3] = i;
				vrt_cpyfm( vdi_handle, MD_TRANS, pxy, &temp, &tempscreen, colors );
			}
		}
	}

	free(temp_addr);
	src->fd_stand = 0;	/* standard format */
	src->fd_addr  = new_addr;
	
	return( TRUE );
}

/*----------------------------------------------------------------------------*/
void
init_icons(void)
{
	switch (planes)
	{
		case 1:
			logo_icon = logo1_icon;
			break;
		case 4:
			logo_icon = logo4_icon;
			break;
		case 8:
			logo_icon = logo8_icon;
			break;
		default:	
			logo_icon = logo8_icon;

			/* convert it to current bit planes */
			TC_trans((MFDB *)&logo_icon);
			break;
	}
	
	if (logo_icon.fd_stand) {
		WORD color[2] = { G_BLACK, G_WHITE };
		PXY  p[4];
		vr_trnfm (vdi_handle, &logo_icon, &logo_icon);
		p[0].p_x = p[0].p_y = p[2].p_x = p[2].p_y = 0;
		p[1].p_x = p[3].p_x = logo_mask.fd_w -1;
		p[1].p_y = p[3].p_y = logo_mask.fd_h -1;
		vrt_cpyfm(vdi_handle, MD_ERASE, (short*)p, &logo_mask, &logo_icon, color);
	}
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_drawIcon (HwWIND This, const GRECT * clip)
{
	MFDB   scrn     = { NULL, };
	MFDB * icon     = (This->isBusy ? &logo1_icon : &logo_icon);
	WORD   color[2] = { G_WHITE, G_LWHITE };
	PXY    lu ,p[4];
	GRECT  work;
	
	wind_get_grect (This->Base.Handle, WF_WORKXYWH, &work);
	lu.p_x = work.g_x + (work.g_w - icon->fd_w) /2;
	lu.p_y = work.g_y + (work.g_h - icon->fd_h) /2;
	vsf_color (vdi_handle, (planes >= 4 ? G_LWHITE : G_WHITE));
	p[1].p_x = (p[0].p_x = clip->g_x) + clip->g_w -1;
	p[1].p_y = (p[0].p_y = clip->g_y) + clip->g_h -1;
	v_hide_c (vdi_handle);
	v_bar (vdi_handle, (short*)p);
	p[2] = lu;
	p[3] = *(PXY*)&icon->fd_w;
	if (rc_intersect (clip, (GRECT*)(p +2))) {
		p[1].p_x = (p[0].p_x = p[2].p_x - lu.p_x) + p[3].p_x -1;
		p[1].p_y = (p[0].p_y = p[2].p_y - lu.p_y) + p[3].p_y -1;
		p[3].p_x += p[2].p_x -1;
		p[3].p_y += p[2].p_y -1;
		if (icon->fd_nplanes > 1) {
			vrt_cpyfm (vdi_handle, MD_TRANS, (short*)p, &logo_mask, &scrn, color);
/*			vro_cpyfm (vdi_handle, S_OR_D,   (short*)p, icon,       &scrn);*/

			/* for me this is a standard call in my code all true
			 * color blitting suffers from this
			 */
			if (planes > 8)
				vro_cpyfm(vdi_handle,S_AND_D,(short*)p,icon,&scrn);
			else
				vro_cpyfm(vdi_handle,S_OR_D,(short*)p,icon,&scrn);

		} else {
			if (!This->isBusy) {
				color[0] = G_BLACK;
			}
			vrt_cpyfm (vdi_handle, MD_TRANS, (short*)p, icon, &scrn, color);
		}
	}
	v_show_c (vdi_handle, 1);
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
wnd_hdlr (HW_EVENT event, long arg, CONTAINR cont, const void * gen_ptr)
{
	HwWIND wind     = hwWind_byValue (arg);
	BOOL   progress = FALSE;
	
	if (!wind) return;
	
	switch (event) {
		
		case HW_PageCleared:
			if (wind->Active == cont) {
				wind->Active = NULL;
				wind->Input  = NULL;
			}
			break;
		
		case HW_PageUpdated:
			if (!wind->Base.isIcon && wind->Base.Ident != WIDENT_BMRK) {
				hwWind_redraw (wind, gen_ptr);
			}
			break;
		
		case HW_PageStarted: {
			union { const void * cv; LOCATION loc; } u;
			char buf[1024];
			u.cv = gen_ptr;
			location_FullName (u.loc, buf, sizeof(buf));
			
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
					updt_toolbar (wind, buf);
				}
				chng_toolbar  (wind, 0, (UWORD)~TBAR_STOP_MASK, -1);
			}
			if (!cont->Parent) {
				free_location (&wind->Location);
				wind->Location = location_share (u.loc);
				hwWind_setName (wind, buf);
			} else {
				hwWind_setInfo (wind, buf, TRUE);
			}
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
			WORD bttn_on = TBAR_REDO_MASK|TBAR_OPEN_MASK
			             | (url_hist ? TBAR_HIST_MASK :0);
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
			hwWind_setInfo (wind, gen_ptr, TRUE);
			goto case_HW_ActivityBeg;
		
		case HW_ImgEndLoad:
			if (!wind->Base.isIcon && gen_ptr) {
				hwWind_redraw (wind, gen_ptr);
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
			progress = TRUE;
		}	break;
		
		case_HW_ActivityEnd:
			gen_ptr = NULL;
		case HW_ActivityEnd:
			wind->isDone += (gen_ptr && *(const long *)gen_ptr > 0
			                 ? *(const long *)gen_ptr : 1);
			if (wind->isDone >= wind->isBusy) {
				wind->isDone = wind->isBusy = 0;
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
			progress = TRUE;
			break;
		
		default:
			errprintf ("wind_handler (%i, %p)\n", event, cont);
	}
	
	if (progress && !wind->Base.isIcon && wind->IbarH) {
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

		WORD bstate;

		graf_mkstate (&mx, &my, &bstate, (WORD*)&kstate);
		if (bstate & 1) {   /* key still pressed? */
			WORD  d;         /* then is drag & drop */
			GRECT pos;
			WORD hdl;
			WINDOW wind;
			
			pos.g_x = x;
			pos.g_y = This->Work.g_y;
			pos.g_w = This->TbarElem[TBAR_EDIT].Width;
			pos.g_h = This->TbarH;
			wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &desk_area);
			graf_mouse ( FLAT_HAND, NULL);
			graf_dragbox (pos.g_w, pos.g_h, pos.g_x, pos.g_y, desk_area.g_x,
							  desk_area.g_y, desk_area.g_w, desk_area.g_h, &d, &d);
			graf_mouse ( ARROW, NULL );
			graf_mkstate(&mx, &my, &bstate, (WORD*)&kstate);
			hdl = wind_find ( mx, my );
			wind = window_byHandle ( hdl );
			if ( wind && wind->Ident == WIDENT_BMRK ) {  /* It is the bookmark window */
			  menu_bookmark_url ( NULL, NULL);
			} else {
			  send_ddmsg ( mx, my, kstate, "Text", ".TXT", 
			    				edit->Length, edit->Text );
			}
		} else {
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
		}
		This = NULL;
	
	} else if (tb_n >= 0) {
		short     event = wind_update (BEG_MCTRL);
		EVMULT_IN m_in  = { MU_BUTTON|MU_M1, 1, 0x03, 0x00, MO_LEAVE, };
		EVMULT_OUT out;
		short     hist  = This->HistMenu;
		short     shift = 0;
		chng_toolbar (This, 0, 0, tb_n);
		if (tb_n <= TBAR_RGHT) {
			m_in.emi_flags |= MU_TIMER;
			m_in.emi_tlow  =  500;
			m_in.emi_thigh =  0;
		}
		m_in.emi_m1.g_x = This->Work.g_x + This->TbarElem[tb_n].Offset;
		m_in.emi_m1.g_y = This->Work.g_y;
		m_in.emi_m1.g_w = m_in.emi_m1.g_h = This->TbarElem[tb_n].Width;
		if (tb_n != TBAR_HIST) do {
			WORD msg[8];
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
		if (tb_n == TBAR_REDO && (out.emo_kmeta & K_ALT)) {
			cache_clear (NULL);
			if (shift) tb_n = -1;
		}
		if (m_in.emi_m1leave) switch (tb_n) {
			case TBAR_LEFT: hwWind_undo     (This, FALSE);        break;
			case TBAR_RGHT: hwWind_undo     (This, TRUE);         break;
			case TBAR_HOME: hwWind_history  (This, 0,    FALSE);  break;
			case TBAR_REDO: hwWind_history  (This, hist, !shift); break;
			case TBAR_STOP: containr_escape (This->Pane);         break;
			case TBAR_HIST: select_urlhist  (This);               break;
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

/*
 ** Changes
 ** Author         Date           Desription
 ** P Slegg        14-Aug-2009    Remove the limit on paste buffer size by dynamically allocating space.
 **
 */
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
		WORDITEM word = NULL;
		
		if ((kstate & K_CTRL) && scan == 0x2F) { /* ^V -- copy from clipboard */
			FILE * file = open_scrap (TRUE);
			if (file)
			{
			  long filelength;
				char *buf, * p;

        /* find the size of the file */
        fseek (file, 0, SEEK_END);
        filelength = ftell (file);

        /* allocate enough memory to hold the file */
        buf = malloc (filelength);
        p = buf;

        /* read the entire file */
        fseek (file, 0, SEEK_SET);
        fread (buf, 1, filelength, file);

			/**	size_t len = fread (buf, 1, sizeof (buf), file);  **/
				while (filelength-- > 0)
				{
					if ((key = *(p++)) == '\n') key = '\r';
					else if (key < ' ')         key = '\0';
					if (key)
					{
						WORDITEM w = input_keybrd (This->Input, key, 0, &clip, &next);
						if (w) word = w;
						else   break;
					}
				}  /* while */
				fclose (file);
			}
			if (word) {
				clip.g_x = 0;
				clip.g_w = word->word_width;
				clip.g_y = -word->word_height;
				clip.g_h = word->word_height + word->word_tail_drop;
			}
			next = This->Input;
		
		} else {
			word = input_keybrd (This->Input, key, kstate, &clip, &next);
		}
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
						char buf[MAX_CLIPBOARD_LENGTH];
						size_t len = sizeof(edit->Text) - edit->Length -1;
						len = fread (buf, 1, min (len, sizeof (buf)), file);
						fclose (file);
						while (len > 0 && isspace (buf[len-1])) {
							len --;
						}
						if (len > 0 && isspace (buf[0])) {
							char * ptr = buf;
							while (len-- && isspace (*(++ptr)));
							memmove (buf, ptr, len);
						}
						/* [GS] Start patch */
						if (len > 0)
						{
							char ZStr[MAX_CLIPBOARD_LENGTH], * ptr = buf;
							WORD i;
							i = 0;
								while ( len-- > 0 )
								{
									if ( *ptr >= ' ' )
										ZStr[i++]=*ptr;
									++ptr;
								}
							len = i;
							memcpy (buf, ZStr, len);
						}
						/* End patch */
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
					LOCATION loc;
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
					loc = new_location (edit->Text, NULL);
					if (loc->Proto == PROT_ABOUT
					    && strncmp ("bookmarks", loc->File, 9) == 0) {
						menu_openbookmarks();
					} else {
						LOADER ldr = start_page_load (This->Pane,
						                              NULL, loc, TRUE, NULL);
						if (ldr) {
							char link[1024];
							location_FullName (ldr->Location, link, sizeof(link));
							hwWind_urlhist (This, link);
						}
						chng_toolbar (This, 0, 0, -1);
					}
					free_location (&loc);
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

