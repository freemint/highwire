#include <stddef.h>
#include <gem.h>

#include "hw-types.h"

#define WINDOW_t WINDOW
#include "Window.h"
static void vTab_draw   (WINDOW, const GRECT *);
static void vTab_raised (WINDOW, BOOL topNbot);


WINDOW window_Top = NULL;

static GRECT desk_area = { 0,0, 0,0 };


/*============================================================================*/
WINDOW
window_ctor (WINDOW This, WORD widgets, GRECT * curr)
{
	if (!desk_area.g_w || !desk_area.g_h) {
		wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &desk_area);
	}
	
	if (This && (This->Handle = wind_create_grect (widgets, &desk_area)) < 0) {
		This = NULL;
	}
	if (This) {
		This->Widgets = widgets;
		if ((This->Next = window_Top) != NULL) {
			window_Top->Prev = This;
		}
		This->Prev = NULL;
		window_Top = This;
		
		This->isIcon = FALSE;
		
		This->drawWork = vTab_draw;
		This->drawIcon = vTab_draw;
		This->raised   = vTab_raised;
		
		if (widgets & NAME) {
			wind_set_str (This->Handle, WF_NAME, "");
		}
		if (widgets & INFO) {
			wind_set_str (This->Handle, WF_INFO, "");
		}
		if (curr) {
			wind_open_grect (This->Handle, curr);
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
		wind_close  (This->Handle);
		wind_delete (This->Handle);
	}
	return This;
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
window_raise (WINDOW This, BOOL topNbot)
{
	BOOL done = FALSE;
	
	if (!This) {
		This = window_Top;
		if (!This) return;
		if (topNbot) {
			while (This->Next) This = This->Next;
		}
	}
	
	if (topNbot) {
		if (This->Prev) {
			if ((This->Prev->Next = This->Next) != NULL) {
				This->Next->Prev = This->Prev;
			}
			window_Top->Prev = This;
			This->Next = window_Top;
			This->Prev = NULL;
			window_Top = This;
			done       = TRUE;
		}
		wind_set (This->Handle, WF_TOP, 0,0,0,0);
	
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















