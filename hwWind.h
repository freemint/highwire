#ifndef __HW_WIND_H__
#define __HW_WIND_H__


#ifndef HISTORY
#define HISTORY void*
#endif

typedef struct hw_window * HwWIND;
#include "Window.h" /* the base class */

struct hw_window {
	WINDOWBASE Base;
	BOOL    isFull;
	BOOL    shaded;
	UWORD   isBusy;
	UWORD   loading;
	GRECT   Curr;
	GRECT   Work;
	WORD    IbarH;
	WORD    TbarH;
	UWORD   TbarMask;
	WORD    TbarActv;
	struct {
		WORD Offset;
		WORD Width;    
	}       TbarElem[7];
	void  * Pane;
	void  * Active;
	void  * Input;
	char    Name[128];
	char    Stat[128];
	char    Info[128];
	UWORD   HistUsed;
	UWORD   HistMenu;
	HISTORY History[1];
};


HwWIND new_hwWind    (const char * name, const char * url, LOCATION);
void   delete_hwWind (HwWIND);

void hwWind_setName (HwWIND, const char *);
void hwWind_setInfo (HwWIND, const char *, BOOL statNinfo);
void hwWind_move    (HwWIND, PXY);
void hwWind_resize  (HwWIND, const GRECT *);
void hwWind_full    (HwWIND);
void hwWind_iconify (HwWIND, const GRECT *);
#define hwWind_raise(HwWIND, BOOL)      window_raise (&HwWIND->Base, BOOL, NULL)
void hwWind_close   (HwWIND, UWORD state);
void hwWind_scroll  (HwWIND, CONTAINR, long dx, long dy);
void hwWind_history (HwWIND, UWORD menu, BOOL renew);
void hwWind_undo    (HwWIND, BOOL redo);

extern WORD   hwWind_Mshape;
extern HwWIND hwWind_Focus;
#define hwWind_Top      hwWind_Next(NULL)
HwWIND  hwWind_Next     (HwWIND);
HwWIND  hwWind_byValue  (long);
HwWIND  hwWind_byHandle (WORD);
#define hwWind_byCoord( x, y )   hwWind_byHandle (wind_find (x, y))
HwWIND  hwWind_byContainr (CONTAINR);

#define hwWind_redraw( HwWIND, GRECT)       window_redraw (&HwWIND->Base, GRECT)
HwWIND  hwWind_mouse  (WORD mx, WORD my, GRECT * watch);
HwWIND  hwWind_keybrd (WORD key, UWORD state);

FRAME hwWind_setActive   (HwWIND, CONTAINR, INPUT);
FRAME hwWind_ActiveFrame (HwWIND);


typedef enum {
	HWWS_INFOBAR = 0x6962u, /* 'ib' infobar appearance, arg means:
	                     * 0: no info output at all
	                     * 1: only AES infobar (traditional GEM)
	                     * 2: only output to the hslider area
	                     * 3: both methods (default setting)
	                     * 4: application widget infobar, no realtime resizing */
	HWWS_TOOLBAR = 0x7462u  /* 'tb' toolbar setting, arg means:
	                     * 0:  disabled
	                     * >0: enabled and visible
	                     * <0: enabled but invisible by default */
} HWWIND_SET;

void hwWind_setup (HWWIND_SET, long arg);


/* service function, not really releated here */
FILE * open_scrap (BOOL rdNwr);


#endif /*__HW_WIND_H__*/
