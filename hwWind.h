#ifndef __HW_WIND_H__
#define __HW_WIND_H__


#ifndef HISTORY
#define HISTORY void*
#endif

typedef struct hw_window * HwWIND;
#include "Window.h" /* the base class */

struct hw_window {
	WINDOWBASE Base;
	BOOL     shaded;
	UWORD    isBusy;  /* number of loading jobs etc. ..*/
	UWORD    isDone;  /* .. and that many already done */
	UWORD    loading; /* number of frames to be loaded */
	GRECT    Work;
	WORD     IbarH;
	WORD     TbarH;
	UWORD    TbarMask;
	WORD     TbarActv;
	struct {
		WORD Offset;
		WORD Width;
	}        TbarElem[8];
	LOCATION Location;
	void   * Pane;
	void   * Active;
	void   * Input;
	char     Stat[128];
	char     Info[128];
	UWORD    HistUsed;
	UWORD    HistMenu;
	HISTORY  History[1];
};

#define WIDENT_BRWS   WINDOW_IDENT('B','R','W','S')
#define WIDENT_BMRK   WINDOW_IDENT('B','M','R','K')     /* Ident for bookmark window */


HwWIND  new_hwWind   (const char * name, const char * url, BOOL topNbot);
#define delete_hwWind(HwWIND)   delete_window (&(HwWIND)->Base)

#define hwWind_setName(HwWIND, name)    window_setName (&HwWIND->Base, name)
void    hwWind_setInfo(HwWIND, const char *, BOOL statNinfo);
#define hwWind_raise(  HwWIND, BOOL)    window_raise (&HwWIND->Base, BOOL, NULL)
void    hwWind_scroll (HwWIND, CONTAINR, long dx, long dy);
void    hwWind_history(HwWIND, UWORD menu, BOOL renew);
void    hwWind_undo   (HwWIND, BOOL redo);

extern WORD   hwWind_Mshape;
extern HwWIND hwWind_Focus;
#define  hwWind_Top       hwWind_Next(NULL)
HwWIND   hwWind_Next      (HwWIND);
HwWIND   hwWind_byHandle  (WORD);
HwWIND   hwWind_byValue   (long);
#define  hwWind_byCoord( x, y )   hwWind_byHandle (wind_find (x, y))
HwWIND   hwWind_byContainr(CONTAINR);
HwWIND   hwWind_byType    (WORD);

#define hwWind_redraw( HwWIND, GRECT)       window_redraw (&HwWIND->Base, GRECT)
HwWIND  hwWind_mouse  (WORD mx, WORD my, GRECT * watch);

FRAME hwWind_setActive   (HwWIND, CONTAINR, INPUT);
FRAME hwWind_ActiveFrame (HwWIND);


typedef enum {
	HWWS_INFOBAR = 0x6962u, /* 'ib' infobar appearance, arg means:
	                     * 0: no info output at all
	                     * 1: only AES infobar (traditional GEM)
	                     * 2: only output to the hslider area
	                     * 3: both methods (default setting)
	                     * 4: application widget infobar, no realtime resizing */
	HWWS_TOOLBAR = 0x7462u, /* 'tb' toolbar setting, arg means:
	                     * 0:  disabled
	                     * >0: enabled and visible
	                     * <0: enabled but invisible by default */
	HWWS_GEOMETRY = 0x7767u, /* 'wg' window geometry, arg points to a string
	                     * containing the values in the format <W>x<H>+<X>+<Y> */
	HWWS_BOOKMGEO = 0x6267u /* 'bg' bookmark window geometry, arg points to a string
	                     * containing the values in the format <W>x<H>+<X>+<Y> */
} HWWIND_SET;

void hwWind_setup   (HWWIND_SET, long arg);
void hwWind_store   (HWWIND_SET);
void hwWind_urlhist (HwWIND, const char *);

void init_icons(void);


/* service function, not really related here */
FILE * open_scrap (BOOL rdNwr);

char * read_scrap(void);

#endif /*__HW_WIND_H__*/