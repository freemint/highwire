#include <stdlib.h>
#include <stdio.h>
#include <gem.h>

#include "global.h"


typedef struct form_window * FORMWIND;
#define WINDOW_t             FORMWIND
#include "Window.h"

#define IDENT_FORM   ((((((((ULONG)'F')<<8)|'O')<<8)|'R')<<8)|'M')
struct form_window {
	WINDOWBASE Base;
	OBJECT * Tree;
	GRECT  * Work;
	WORD     EditObj;
	WORD     EditIdx;
	/**/
	BOOL (*handler)(OBJECT *, WORD obj);
};
static WINDOW vTab_destruct(FORMWIND);
static void vTab_evButton (FORMWIND, WORD bmask, PXY mouse, UWORD kstate, WORD);
static void vTab_evKeybrd (FORMWIND, WORD scan, WORD ascii, UWORD kstate);
static void vTab_drawWork (FORMWIND, const GRECT *);
static BOOL vTab_close    (FORMWIND, UWORD kstate);
static void vTab_moved    (FORMWIND);

static BOOL dummy_handler (OBJECT *, WORD obj);

/*============================================================================*/
static FORMWIND
new_formWind (OBJECT * tree, const char * title, BOOL modal)
{
	FORMWIND This = (tree ? malloc (sizeof (struct form_window)) : NULL);
	if (This) {
		(void)title;
		
		tree->ob_flags |= OF_FLAG15;
		window_ctor (This, MOVER|CLOSER, IDENT_FORM, title, NULL, modal);
		This->Tree = tree;
		This->Work = (GRECT*)&tree->ob_x;
		wind_calc_grect (WC_BORDER, This->Base.Widgets,
		                 This->Work, &This->Base.Curr);
		This->Base.destruct = vTab_destruct;
		This->Base.evButton = vTab_evButton;
		This->Base.evKeybrd = vTab_evKeybrd;
		This->Base.drawWork = vTab_drawWork;
		This->Base.close    = vTab_close;
		This->Base.moved    = vTab_moved;
		
		This->EditObj = 0;
		This->EditIdx = 0;
		This->handler = dummy_handler;
	}
	return This;
}

/*============================================================================*/
#define delete_formWind(FORMWIND)   free((*(FORMWIND)->Base.destruct)(FORMWIND))

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static WINDOW
vTab_destruct (FORMWIND This)
{
	This->Tree->ob_flags &= ~OF_FLAG15;
	
	return window_dtor(This);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_evButton (FORMWIND This, WORD bmask, PXY mouse, UWORD kstate, WORD clicks)
{
	(void)kstate;
	if (bmask == 0x01) {
		WORD obj = objc_find (This->Tree, ROOT, MAX_DEPTH, mouse.p_x, mouse.p_y);
		if (obj >= 0) {
			WORD nxt = 0;
			WORD ret = form_button (This->Tree, obj, clicks, &nxt);
			if (nxt > 0 && nxt != This->EditObj
			            && (This->Tree[nxt].ob_flags & OF_EDITABLE)) {
				/* edit object selected */
				if (This->EditObj > 0) {
					objc_edit (This->Tree, This->EditObj, 0, &This->EditIdx, 3);
				}
				if (objc_edit (This->Tree, nxt, 0, &This->EditIdx, 1)) {
					This->EditObj = nxt;
				} else {
					This->EditObj = 0;
				}
			}
			if (ret == 0) {
				/* exit object selected */
				if ((*This->handler)(This->Tree, nxt)) {
					delete_formWind (This);
				}
			}
		}
	}
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_evKeybrd (FORMWIND This, WORD scan, WORD ascii, UWORD kstate)
{
	(void)kstate;
	
	if (This->EditObj > 0) {
		WORD nxt = 0;
		WORD chr = (scan<<8)|ascii;
		if (form_keybd (This->Tree, This->EditObj, 0, chr, &nxt, &chr) == 0) {
			/* return key: default object selected */
			if ((*This->handler)(This->Tree, nxt)) {
				delete_formWind (This);
			}
		
		} else if (nxt > 0) {
			/* cursor or tab: next edit object selected */
			objc_edit (This->Tree, This->EditObj, 0, &This->EditIdx, 3);
			if (objc_edit (This->Tree, nxt, 0, &This->EditIdx, 1)) {
				This->EditObj = nxt;
			} else {
				This->EditObj = 0;
			}
		
		} else {
			/* normal key hit */
			objc_edit (This->Tree, This->EditObj, chr, &This->EditIdx, 2);
		}
	}
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_drawWork (FORMWIND This, const GRECT * clip)
{
	objc_draw (This->Tree, ROOT, MAX_DEPTH,
	           clip->g_x, clip->g_y, clip->g_w, clip->g_h);
	if (This->EditObj > 0) {
		GRECT crsr;
		objc_offset (This->Tree, This->EditObj, &crsr.g_x, &crsr.g_y);
		crsr.g_x += This->EditIdx *8;
		crsr.g_y -= 3;
		crsr.g_w =  1;
		crsr.g_h =  This->Tree[This->EditObj].ob_height +6;
		if (rc_intersect (clip, &crsr)) {
			crsr.g_w += crsr.g_x -1;
			crsr.g_h += crsr.g_y -1; 
			vs_clip_pxy (vdi_handle, (PXY*)&crsr);
			vswr_mode (vdi_handle, MD_XOR);
			v_hide_c  (vdi_handle);
			v_pline   (vdi_handle, 2, (short*)&crsr);
			v_show_c  (vdi_handle, 1);
			vswr_mode (vdi_handle, MD_TRANS);
			vs_clip_off (vdi_handle);
		}
	}
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
vTab_close (FORMWIND This, UWORD kstate)
{
	(void)kstate;
	if ((*This->handler)(This->Tree, -1)) {
		This->Tree->ob_flags &= ~OF_FLAG15;
		return TRUE; /* to be deleted */
	}
	return FALSE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_moved (FORMWIND This)
{
	wind_get_grect (This->Base.Handle, WF_WORKXYWH, This->Work);
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
dummy_handler (OBJECT * tree, WORD obj)
{
	printf ("dummy_handler(%i)\n", obj);
	return (obj < 0 || (tree[obj].ob_flags & OF_DEFAULT));
}


/*============================================================================*/
WORD
formwind_do (OBJECT * tree, WORD start, const char * title,
             BOOL modal, BOOL(*handler)(OBJECT*,WORD))
{
	FORMWIND wind = NULL;
	
	if (!tree || (tree->ob_flags & OF_FLAG15)
	          || (wind = new_formWind (tree, title, modal)) == NULL) return -1;
	
	if (handler) {
		wind->handler = handler;
	}
	window_raise (&wind->Base, TRUE, &wind->Base.Curr);
	if (start > 0 && objc_edit (wind->Tree, start, 0, &wind->EditIdx, 1)) {
		wind->EditObj = start;
	}
	
	return wind->Base.Handle;
}
