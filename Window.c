#include <stddef.h>
#include <stdlib.h>
#include <gem.h>

#include "hw-types.h"

#define WINDOW_t WINDOW
#include "Window.h"
static BOOL vTab_evMessage (WINDOW, WORD msg[], PXY, UWORD);
static void vTab_evButton  (WINDOW, WORD bmask, PXY, UWORD, WORD clicks);
static void vTab_evKeybrd  (WINDOW, WORD scan, WORD ascii, UWORD kstate);
static void vTab_draw      (WINDOW, const GRECT *);
static void vTab_raised    (WINDOW, BOOL topNbot);


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
		This->Next = This->Prev = NULL;
		
		This->evMessage = vTab_evMessage;
		This->evButton  = vTab_evButton;
		This->evKeybrd  = vTab_evKeybrd;
		This->drawWork  = vTab_draw;
		This->drawIcon  = vTab_draw;
		This->raised    = vTab_raised;
		
		if (widgets & NAME) {
			wind_set_str (This->Handle, WF_NAME, "");
		}
		if (widgets & INFO) {
			wind_set_str (This->Handle, WF_INFO, "");
		}
		if (curr) {
			window_raise (This, TRUE, curr);
			This->Curr = *curr;
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
	window_raise (This, TRUE, NULL);
	(void)scan;
	(void)ascii;
	(void)kstate;
	printf ("window::evKeybrd (0x%02X,0x%02X,0x%04X)\n", scan, ascii, kstate);
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void
window_evKeybrd (UWORD key, UWORD kstate)
{
	WORD scan  = key >> 8;
	WORD ascii = key & 0xFF;
	
	switch (ascii) {
		case 0x0017: /* CTRL+W */ window_raise (NULL, TRUE, NULL); break;
		case 0x0011: /* CTRL+Q */ exit (0);
		default:     (*window_Top->evKeybrd)(window_Top, scan, ascii, kstate);
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
		This->Curr = *curr;
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
