
typedef struct s_window * WINDOW;
#ifndef WINDOW_t
#define WINDOW_t WINDOW

#else
WINDOW_t window_ctor (WINDOW_t, WORD widgets, GRECT *, BOOL modal);
WINDOW_t window_dtor (WINDOW_t);
#endif
typedef struct s_window {   /* all of the following are private attributes, */
	WORD   Handle;           /* only to be read, by foreign modules          */
	WORD   Widgets;    /* as in wind_create (kind, ...):  mover,  sizer, etc. */
	WINDOW Prev, Next; /* still private, keep away */
	BOOL   isModal;
	BOOL   isIcon;
	BOOL   isFull;
	GRECT  Curr;       /* outer extents of the window */
	/***/
	BOOL (*evMessage)(WINDOW_t, WORD msg[], PXY, UWORD kstate);
	void (*evButton) (WINDOW_t, WORD bmask, PXY, UWORD kstate, WORD clicks);
	void (*evKeybrd) (WINDOW_t, WORD scan, WORD ascii, UWORD kstate);
	void (*drawWork)(WINDOW_t, const GRECT *);
	void (*drawIcon)(WINDOW_t, const GRECT *);
	void (*raised)   (WINDOW_t, BOOL topNbot);
	void (*moved)    (WINDOW_t);
	BOOL (*sized)    (WINDOW_t);
	void (*iconified)(WINDOW_t);
} WINDOWBASE;

#undef WINDOW_t


BOOL window_evMessage (WORD msg[], PXY mouse, UWORD kstate);
void window_evButton  (WORD bmask, PXY mouse, UWORD kstate, WORD clicks);
void window_evKeybrd  (UWORD key, UWORD kstate);

void window_redraw (WINDOW, const GRECT *);

void window_raise  (WINDOW, BOOL topNbot, const GRECT *);
void window_resize (WINDOW, const GRECT *, BOOL fulled);
