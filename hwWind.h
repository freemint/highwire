#ifndef __HW_WIND_H__
#define __HW_WIND_H__


#ifndef HISTORY
#define HISTORY void*
#endif

typedef struct hw_window * HwWIND;
struct hw_window {
	WORD    Handle;
	BOOL    isFull;
	BOOL    isIcon;
	UWORD   isBusy;
	UWORD   loading;
	GRECT   Curr;
	GRECT   Work;
	HwWIND  Next;
	void  * Pane;
	void  * Active;
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
void hwWind_raise   (HwWIND, BOOL topNbot);
void hwWind_scroll  (HwWIND, CONTAINR, long dx, long dy);
void hwWind_history (HwWIND, UWORD menu);
void hwWind_undo    (HwWIND, BOOL redo);

extern WORD   hwWind_Mshape;
extern HwWIND hwWind_Top;
extern HwWIND hwWind_Focus;
HwWIND  hwWind_byValue  (long);
HwWIND  hwWind_byHandle (WORD);
#define hwWind_byCoord( x, y )   hwWind_byHandle (wind_find (x, y))

void hwWind_redraw (HwWIND, const GRECT *);

FRAME hwWind_setActive   (HwWIND, CONTAINR);
FRAME hwWind_ActiveFrame (HwWIND);


#endif /*__HW_WIND_H__*/
