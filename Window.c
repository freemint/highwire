#include <stddef.h>
#include <stdlib.h>
#include <gem.h>

#include "global.h"

#define WINDOW_t WINDOW
#include "Window.h"
static void dummy_Void (void) {}
static BOOL dummy_False(void) { return FALSE; }
static BOOL vTab_evMessage (WINDOW, WORD msg[], PXY, UWORD);
static void vTab_evButton  (WINDOW, WORD bmask, PXY, UWORD, WORD clicks);
static void vTab_evKeybrd  (WINDOW, WORD scan, WORD ascii, UWORD kstate);
static void vTab_draw      (WINDOW, const GRECT *);
static BOOL vTab_close     (WINDOW, UWORD kstate);
static void vTab_raised    (WINDOW, BOOL topNbot);
#define     vTab_moved     ((void(*)(WINDOW))dummy_Void)
#define     vTab_sized     ((BOOL(*)(WINDOW))dummy_False)
#define     vTab_iconified ((void(*)(WINDOW))dummy_Void)


WINDOW window_Top = NULL;

static GRECT desk_area = { 0,0, 0,0 };


/*============================================================================*/
WINDOW
window_ctor (WINDOW This, WORD widgets, GRECT * curr, BOOL modal)
{
	if (!desk_area.g_w || !desk_area.g_h) {
		wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &desk_area);
	}
	
	if (This && (This->Handle = wind_create_grect (widgets, &desk_area)) < 0) {
		This = NULL;
	}
	if (This) {
		This->Widgets = widgets;
		This->isModal = modal;
		This->isIcon  = FALSE;
		This->isFull  = FALSE;
		This->isScrn  = FALSE;
		This->Next = This->Prev = NULL;
		
		This->destruct  = window_dtor;
		This->evMessage = vTab_evMessage;
		This->evButton  = vTab_evButton;
		This->evKeybrd  = vTab_evKeybrd;
		This->drawWork  = vTab_draw;
		This->drawIcon  = vTab_draw;
		This->close     = vTab_close;
		This->raised    = vTab_raised;
		This->moved     = vTab_moved;
		This->sized     = vTab_sized;
		This->iconified = vTab_iconified;
		
		if (widgets & NAME) {
			wind_set_str (This->Handle, WF_NAME, "");
		}
		if (widgets & INFO) {
			wind_set_str (This->Handle, WF_INFO, "");
		}
		if (curr) {
			window_raise (This, TRUE, curr);
		} else {
			This->Curr = desk_area;
		}
	}
	return This;
}

/*============================================================================*/
WINDOW
window_dtor (WINDOW This)
{
	if (This) {
		if (This->Prev) This->Prev->Next = This->Next;
		else            window_Top       = This->Next;
		if (This->Next) This->Next->Prev = This->Prev;
		This->Next = This->Prev = NULL;
		wind_close  (This->Handle);
		wind_delete (This->Handle);
		This->Handle  = -1;
	}
	return This;
}


/*============================================================================*/
static BOOL
vTab_evMessage (WINDOW This, WORD msg[], PXY mouse, UWORD kstate)
{
	(void)This;
	(void)msg;
	(void)mouse;
	(void)kstate;
	return FALSE;
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
BOOL
window_evMessage (WORD msg[], PXY mouse, UWORD kstate)
{
	WINDOW wind = window_Top;
	while (wind && wind->Handle != msg[3]) {
		wind = wind->Next;
	}
	if (!wind) {
		return FALSE;
	}
	switch (msg[0]) {
		case WM_REDRAW:   window_redraw (wind, (GRECT*)(msg + 4)); break;
		case WM_TOPPED:   window_raise  (wind, TRUE,  NULL);       break;
		case WM_BOTTOMED: window_raise  (wind, FALSE, NULL);       break;
		
		case WM_MOVED:
			if (wind->isIcon) {
				GRECT curr;
				wind_get_grect (wind->Handle, WF_CURRXYWH, &curr);
				curr.g_x = msg[4];
				curr.g_y = msg[5];
				wind_set_grect (wind->Handle, WF_CURRXYWH, &curr);
			} else {
				wind->Curr.g_x = msg[4];
				wind->Curr.g_y = msg[5];
				wind_set_grect (wind->Handle, WF_CURRXYWH, &wind->Curr);
				wind->isFull = FALSE;
				(*wind->moved)(wind);
			}
			break;
		
		case WM_FULLED:
			if (wind->isFull) {
				wind_get_grect (wind->Handle, WF_PREVXYWH, (GRECT*)(msg +4));
			} else {
				wind_get_grect (wind->Handle, WF_FULLXYWH, (GRECT*)(msg +4));
				msg[0] = 0;
			}
		case WM_SIZED:
			if (!wind->isIcon) {
				window_resize (wind, (GRECT*)(msg +4), (msg[0] == 0));
			}
			break;
		
		case WM_ICONIFY:
			if (wind->isFull) {
				wind_get_grect (wind->Handle, WF_PREVXYWH, &wind->Curr);
			}
			wind_set_grect (wind->Handle, WF_ICONIFY, (GRECT*)(msg + 4));
			wind->isIcon = TRUE;
			(*wind->iconified)(wind);
			break;
		
		case WM_UNICONIFY:
			wind_set_grect (wind->Handle, WF_UNICONIFY, &wind->Curr);
			if (wind->isFull) {
				wind_get_grect (wind->Handle, WF_FULLXYWH, &wind->Curr);
				wind_set_grect (wind->Handle, WF_CURRXYWH, &wind->Curr);
			}
			wind->isIcon = FALSE;
			(*wind->iconified)(wind);
			break;
		
		case WM_CLOSED:
			if ((*wind->close)(wind, kstate)) {
				delete_window (wind);
			}
			break;
		
		default: return (*wind->evMessage)(wind, msg, mouse, kstate);
	}
	return TRUE;
}


/*============================================================================*/
static void
vTab_evButton (WINDOW This, WORD bmask, PXY mouse, UWORD kstate, WORD clicks)
{
	window_raise (This, TRUE, NULL);
	(void)bmask;
	(void)mouse;
	(void)kstate;
	(void)clicks;
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void
window_evButton (WORD bmask, PXY mouse, UWORD kstate, WORD clicks)
{
	WINDOW wind = window_Top;
	WORD   hdl  = wind_find (mouse.p_x, mouse.p_y);
	while (wind && wind->Handle != hdl) {
		wind = wind->Next;
	}
	if (wind && (wind == window_Top || !window_Top->isModal)) {
		(*wind->evButton)(wind, bmask, mouse, kstate, clicks);
	}
}


/*============================================================================*/
#include <stdio.h>
static void
vTab_evKeybrd (WINDOW This, WORD scan, WORD ascii, UWORD kstate)
{
	(void)This;
	(void)scan;
	(void)ascii;
	(void)kstate;
	printf ("window::evKeybrd (0x%02X,0x%02X,0x%04X)\n", scan, ascii, kstate);
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdio.h>
void
window_evKeybrd (UWORD key, UWORD kstate)
{
	WORD   scan  = key >> 8;
	WORD   ascii = key & 0xFF;
	WINDOW wind  = window_Top;
	
	if (wind && !kstate && scan == 0x44) { /* F10: extreme full screen mode */
	
		if (wind->isScrn) {
			WORD  hdl = wind->Handle;
			if ((wind->Handle = wind_create_grect(wind->Widgets,&desk_area)) < 0) {
				wind->Handle = hdl;
				return;
			}
			wind_get_grect (hdl, WF_PREVXYWH, &wind->Curr);
			wind->Curr.g_x -= desk_area.g_w;
			wind->Curr.g_y -= desk_area.g_h;
			wind_set_grect (wind->Handle, WF_CURRXYWH, &wind->Curr);
			wind->isFull = FALSE;
			(*wind->sized)(wind);
			wind->isScrn = FALSE;
			wind_open_grect (wind->Handle, &wind->Curr);
			wind_close  (hdl);
			wind_delete (hdl);
			
		} else if (!wind->isIcon) {
			GRECT curr, prev;
			WORD  hdl = wind->Handle;
			wind_calc_grect (WC_BORDER, 0, &desk_area, &curr);
			if ((wind->Handle = wind_create_grect (0, &curr)) < 0) {
				wind->Handle = hdl;
				return;
			}
			if (wind->isFull) {
				wind_get_grect (hdl, WF_PREVXYWH, &prev);
			} else {
				prev         = wind->Curr;
				wind->isFull = TRUE;
			}
			wind_set_grect (wind->Handle, WF_CURRXYWH, &curr);
			wind->Curr   = curr;
			wind->isScrn = TRUE;
			(*wind->sized)(wind);
			prev.g_x += desk_area.g_w;
			prev.g_y += desk_area.g_h;
			wind_open_grect (wind->Handle, &prev);
			wind_set_grect (wind->Handle, WF_CURRXYWH, &curr);
			wind_close  (hdl);
			wind_delete (hdl);
		}
		
	} else switch (ascii) {
		case 0x0017: /* CTRL+W */ window_raise (NULL, TRUE, NULL); break;
		case 0x0011: /* CTRL+Q */ exit (0);
		default:     if (wind) (*wind->evKeybrd)(wind, scan, ascii, kstate);
	}
}


/*============================================================================*/
static void
vTab_draw (WINDOW This, const GRECT * clip)
{
	(void)This;
	(void)clip;
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void
window_redraw (WINDOW This, const GRECT * clip)
{
	void (*draw)(WINDOW, const GRECT *)
	                          = (!This->isIcon ?This->drawWork : This->drawIcon);
	GRECT rect = desk_area, work;
	
	wind_update (BEG_UPDATE);
	
	if (clip && !rc_intersect (clip, &rect)) {
		rect.g_w = rect.g_h = 0;
	
	} else {
		wind_get_grect (This->Handle, WF_WORKXYWH, &work);
		if (!rc_intersect (&rect, &work)) {
			rect.g_w = rect.g_h = 0;
		
		} else {
			wind_get_grect (This->Handle, WF_FIRSTXYWH, &rect);
		}
	}
	while (rect.g_w > 0 && rect.g_h > 0) {
		if (rc_intersect (&work, &rect)) {
			(*draw)(This, &rect);
		}
		wind_get_grect (This->Handle, WF_NEXTXYWH, &rect);
	}
	
	wind_update (END_UPDATE);
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
vTab_close (WINDOW This, UWORD kstate)
{
	(void)This;
	(void)kstate;
	return TRUE; /* to be deleted */
}


/*============================================================================*/
static void
vTab_raised (WINDOW This, BOOL topNbot)
{
	(void)This;
	(void)topNbot;
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void
window_raise (WINDOW This, BOOL topNbot, const GRECT * curr)
{
	BOOL done = FALSE;
	
	if (!This) {
		This = window_Top;
		if (!This || This->isModal) return;
		if (topNbot) {
			while (This->Next) This = This->Next;
		}
	
	} else if (curr && !This->Next && !This->Prev) {
		topNbot    = TRUE;
		done       = TRUE;
		wind_set_grect (This->Handle, WF_CURRXYWH, curr);
		if (!wind_get_grect (This->Handle, WF_CURRXYWH, &This->Curr)
		    || This->Curr.g_w <= 0 || This->Curr.g_h <= 0) {
			This->Curr = *curr; /* avoid a Geneva-bug where curr becomes 0,0,0,0 */
		}                      /* if the window was created but not opend       */
		(*This->sized)(This);
		wind_open_grect (This->Handle, curr);
	}
	
	if (topNbot) {
		if (This->Prev) {
			if ((This->Prev->Next = This->Next) != NULL) {
				This->Next->Prev = This->Prev;
				This->Next       = NULL;
			}
			This->Prev = NULL;
			done       = TRUE;
		}
		wind_set (This->Handle, WF_TOP, 0,0,0,0);
		if (done) {
			if (!window_Top) {
				window_Top = This;
			} else if (!window_Top->isModal || This->isModal) {
				window_Top->Prev = This;
				This->Next       = window_Top;
				window_Top       = This;
			} else {
				WINDOW wind = window_Top;
				while (wind->Next) {
					WINDOW next = wind->Next;
					if (!next->isModal) break;
					wind = next;
				}
				if ((This->Next = wind->Next) != NULL) {
					This->Next->Prev = This;
				}
				This->Prev = wind;
				wind->Next = This;
				do {
					wind_set (wind->Handle, WF_TOP, 0,0,0,0);
				} while ((wind = wind->Prev) != NULL);
			}
		}
	
	} else {
		if (This->Next) {
			WINDOW bot = This->Next;
			while (bot->Next) bot = bot->Next;
			if ((This->Next->Prev = This->Prev) != NULL) {
				This->Prev->Next = This->Next;
			} else {
				window_Top       = This->Next;
			}
			This->Next = NULL;
			This->Prev = bot;
			bot->Next  = This;
			done       = TRUE;
		}
		wind_set (This->Handle, WF_BOTTOM, 0,0,0,0);
	}
	
	if (done) {
		(*This->raised)(This, topNbot);
	}
}

/*============================================================================*/
void
window_resize (WINDOW This, const GRECT * curr, BOOL fulled)
{
	GRECT prev = This->Curr;
	wind_set_grect (This->Handle, WF_CURRXYWH, curr);
	wind_get_grect (This->Handle, WF_CURRXYWH, &This->Curr);
	This->isFull = fulled;
	if ((*This->sized)(This)) {
		if (sys_XAAES()) {
			WORD msg[8];
			msg[0] = WM_REDRAW;
			msg[1] = gl_apid;
			msg[2] = 0;
			msg[3] = This->Handle;
			*(GRECT*)(msg +4) = prev;
			appl_write (gl_apid, 16, msg);
		} else if (prev.g_x == This->Curr.g_x && prev.g_y == This->Curr.g_y) {
			window_redraw (This, &prev);
		}
	}
}
