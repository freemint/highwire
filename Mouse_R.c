/* @(#)highwire/Mouse_R.c
 */
#include <stdlib.h>
#include <string.h>

#include <gemx.h>

#include "global.h"
#include "Containr.h"
#include "Loader.h"
#include "Form.h"
#include "hwWind.h"


/*==============================================================================
 *
 * handles mouse interaction with a frame
*/
void
button_clicked (WORD button, WORD mx, WORD my)
{
	void   * hash = NULL;
	GRECT    watch;
	WORD     decx = 0, decy = 0;
	
	EVMULT_IN multi_in = { 0, };
	
	HwWIND   wind = hwWind_byCoord (mx, my);
	CONTAINR cont = (wind ? wind->Pane : NULL);
	UWORD    elem = containr_Element (&cont, mx, my, &watch, NULL, &hash);
	FRAME   frame = hwWind_setActive (wind, cont);
	
#ifdef GEM_MENU
#if (_HIGHWIRE_ENCMENU_==TRUE)
	if (frame) {
		update_menu (frame->Encoding, (frame->MimeType == MIME_TXT_PLAIN));
	}
#endif
#endif
	
	switch (PE_Type (elem)) {
		
		case PE_VBAR: {
			BOOL drag = FALSE;
			long step = 0;
		
			WORD y = my - frame->clip.g_y;
			
			if (y >= frame->v_bar.pos + frame->v_bar.size) {
				
				if (y >= frame->v_bar.rd) {                         /* down arrow */
					step = (button & LEFT_BUTTON ? +scroll_step : -scroll_step);
				
				} else if (button & LEFT_BUTTON) {                   /* down page */
					step = +(frame->clip.g_h - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decy = frame->v_bar.size /2;
				}
			
			} else if (y <= frame->v_bar.pos) {
				
				if (y <= frame->v_bar.lu) {                           /* up arrow */
					step = (button & LEFT_BUTTON ? -scroll_step : +scroll_step);
				
				} else if (button & LEFT_BUTTON) {                     /* up page */
					step = -(frame->clip.g_h - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decy = frame->v_bar.size /2;
				}
			
			} else if (button & LEFT_BUTTON) {                       /* realtime */
				drag = TRUE;
				decy = my - frame->clip.g_y - frame->v_bar.pos;
		
			} else {                                                   /* slider */
				short s_x  = frame->clip.g_x + frame->clip.g_w;
				short s_y  = 0;
				short sldr = frame->v_bar.rd - frame->v_bar.lu +1;
				graf_dragbox (scroll_bar_width, frame->v_bar.size,
				              s_x, frame->clip.g_y + frame->v_bar.pos,
				              s_x, frame->clip.g_y + frame->v_bar.lu,
				              scroll_bar_width, sldr,
				              &s_x, &s_y);
				sldr -= frame->v_bar.size;
				step  = ((frame->Page.Height - frame->clip.g_h)
				         * ((s_y - frame->clip.g_y) - frame->v_bar.lu) + (sldr /2))
				         / sldr
				      - frame->v_bar.scroll;
			}
			
			if (drag) {
				multi_in.emi_flags   = MU_BUTTON|MU_M1;
				multi_in.emi_m1leave = MO_LEAVE;
				multi_in.emi_m1.g_x  = 0;
				multi_in.emi_m1.g_y  = frame->clip.g_y
				                     + frame->v_bar.lu + frame->v_bar.pos;
				multi_in.emi_m1.g_w  = 10000;
				multi_in.emi_m1.g_h  = 1;
				
			} else {
				hwWind_scroll (wind, cont, 0, step);
			}
		} break;
		
		case PE_HBAR: {
			BOOL drag = FALSE;
			long step = 0;
		
			WORD x = mx - frame->clip.g_x;
			
			if (x >= frame->h_bar.pos + frame->h_bar.size) {
				
				if (x >= frame->h_bar.rd) {                        /* right arrow */
					step = (button & LEFT_BUTTON ? +scroll_step : -scroll_step);
				
				} else if (button & LEFT_BUTTON) {                  /* right page */
					step = +(frame->clip.g_w - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decx = frame->h_bar.size /2;
				}
			
			} else if (x <= frame->h_bar.pos) {
				
				if (x <= frame->h_bar.lu) {                         /* left arrow */
					step = (button & LEFT_BUTTON ? -scroll_step : +scroll_step);
				
				} else if (button & LEFT_BUTTON) {                   /* left page */
					step = -(frame->clip.g_w - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decx = frame->h_bar.size /2;
				}
			
			} else if (button & LEFT_BUTTON) {                       /* realtime */
				drag = TRUE;
				decx = mx - frame->clip.g_x - frame->h_bar.pos;
		
			} else {                                                   /* slider */
				short s_x  = 0;
				short s_y  = frame->clip.g_y + frame->clip.g_h;
				short sldr = frame->h_bar.rd - frame->h_bar.lu +1;
				graf_dragbox (frame->h_bar.size, scroll_bar_width,
				              frame->clip.g_x + frame->h_bar.pos, s_y,
				              frame->clip.g_x + frame->h_bar.lu,  s_y,
				              sldr, scroll_bar_width,
				              &s_x, &s_y);
				sldr -= frame->h_bar.size;
				step  = ((frame->Page.Width - frame->clip.g_w)
				         * ((s_x - frame->clip.g_x) - frame->h_bar.lu) + (sldr /2))
				         / sldr
				      - frame->h_bar.scroll;
			}
			
			if (drag) {
				multi_in.emi_flags   = MU_BUTTON|MU_M2;
				multi_in.emi_m2leave = MO_LEAVE;
				multi_in.emi_m2.g_x  = frame->clip.g_x
				                     + frame->h_bar.lu + frame->h_bar.pos;
				multi_in.emi_m2.g_y  = 0;
				multi_in.emi_m2.g_w  = 1;
				multi_in.emi_m2.g_h  = 10000;
				
			} else {
				hwWind_scroll (wind, cont, step, 0);
			}
		} break;
		
		case PE_INPUT: {
			GRECT radio;
			switch (input_handle (hash, &radio)) {
				case 2: radio.g_x += watch.g_x;
				        radio.g_y += watch.g_y;
				        hwWind_redraw (wind, &radio);
				case 1: hwWind_redraw (wind, &watch);
				        if (input_activate (hash)) {
				           hwWind_redraw (wind, &watch);
				        }
			}
		}	break;
		
		case PE_TLINK: case PE_ILINK: {
			struct url_link * link = hash;
			if (*link->address == '#') {
				long dx, dy;
				if (containr_Anchor (cont, link->address, &dx, &dy)) {
					hwWind_scroll (wind, cont, dx, dy);
					check_mouse_position (mx, my);
				}
			} else {
				LOCATION loc = frame->Location;
				WORD meta, u;
				graf_mkstate (&u,&u,&u, &meta);
				if (meta & K_ALT) {
					cont = new_hwWind (link->address, "", NULL)->Pane;
				} else if (link->u.target) {
					if (stricmp (link->u.target, "_blank") == 0) {
						cont = new_hwWind (link->address, "", NULL)->Pane;
					} else {
						CONTAINR target = containr_byName (cont, link->u.target);
						if (target) cont = target;
					}
				}
				loader_setParams (new_loader_job (link->address, loc, cont),
				                  link->encoding, -1,-1);
			}
		} break;
		
		default:
			if (elem >= PE_FRAME && (button & RIGHT_BUTTON)) {
#if (_HIGHWIRE_RPOP_==TRUE)
				rpopup_open (mx, my);
#endif
			} else if (wind) {
				hwWind_raise (wind, TRUE);
			}
	}
	
	if (multi_in.emi_flags) {
		long pg_h = frame->Page.Height - frame->clip.g_h;
		WORD sl_h = frame->v_bar.rd - frame->v_bar.lu - frame->v_bar.size;
		long top  = frame->clip.g_y + frame->v_bar.lu +1;
		long bot  = frame->clip.g_y + frame->v_bar.rd -1;
		long pg_w = frame->Page.Width - frame->clip.g_w;
		WORD sl_w = frame->h_bar.rd - frame->h_bar.lu - frame->h_bar.size;
		long lft  = frame->clip.g_x + frame->h_bar.lu +1;
		long rgt  = frame->clip.g_x + frame->h_bar.rd -1;
		long dx = 0, dy = 0;
		
		wind_update (BEG_MCTRL);
		
		multi_in.emi_bclicks = 1;
		multi_in.emi_bmask   = button;
		multi_in.emi_bstate  = 0x00;
		while (1) {
			WORD       msg[8];
			EVMULT_OUT out;
			short      event = evnt_multi_fast (&multi_in, msg, &out);
			
			if (event & MU_M1) {
				long y = out.emo_mouse.p_y - decy;
				if      (y >= bot) dy =  pg_h;
				else if (y >  top) dy = (pg_h * (y - top) + (top /2)) / sl_h;
				dy -= frame->v_bar.scroll;
				multi_in.emi_m1.g_y = out.emo_mouse.p_y;
			}
			if (event & MU_M2) {
				long x = out.emo_mouse.p_x - decx;
				if      (x >= rgt) dx =  pg_w;
				else if (x >  lft) dx = (pg_w * (x - lft) + (lft /2)) / sl_w;
				dx -= frame->h_bar.scroll;
				multi_in.emi_m2.g_x = out.emo_mouse.p_x;
			}
			if (dx || dy) {
				hwWind_scroll (wind, cont, dx, dy);
				dx = dy = 0;
			}
			if (event & MU_BUTTON) break;
		}
		wind_update (END_MCTRL);
	}
}


/*==============================================================================
 *
 * find the actual clickable to be highlighted.
 *
 * AltF4 - Feb. 17, 2002:  reworked to make use of container funtions and avoid
 *                         access of the global the_firt_frame and frame_next().
*/
void
check_mouse_position (WORD mx, WORD my)
{
#if 0 /* debugging */
	static GRECT WATCH = {0,0,-1,-1};
#	define WATCH WATCH
#endif
	
	static GRECT focus_rect;
	
	CONTAINR cont = NULL;
	void   * hash = NULL;
	UWORD    elem = PE_NONE;
	GRECT    clip, watch;
	WORD     whdl = wind_find (mx, my);
	HwWIND   wind = hwWind_byHandle (whdl);
	
	if (wind && !wind->isIcon) {
		clip = wind->Work;
	} else {
		wind_get_grect (whdl, WF_WORKXYWH, &clip);
	}
	if (mx < clip.g_x || mx >= clip.g_x + clip.g_w ||
	    my < clip.g_y || my >= clip.g_y + clip.g_h) {
		watch.g_x = mx;
		watch.g_y = my;
		watch.g_w = watch.g_h = 1;
	
	} else {
		wind_get_grect (whdl, WF_FIRSTXYWH, &watch);
		while (watch.g_w > 0 && watch.g_h > 0 &&
			    (mx < watch.g_x || mx >= watch.g_x + watch.g_w ||
			     my < watch.g_y || my >= watch.g_y + watch.g_h)) {
			wind_get_grect (whdl, WF_NEXTXYWH, &watch);
		}
		if (watch.g_w <= 0 || watch.g_h <= 0) {
			watch.g_x = mx;
			watch.g_y = my;
			watch.g_w = watch.g_h = 1;
			wind = NULL;
		
		} else if (wind && !wind->isIcon) {
			GRECT rect;
			cont = wind->Pane;
			elem = containr_Element (&cont, mx, my, &rect, &clip, &hash);
			rc_intersect (&rect, &watch);
		}
	}
	set_mouse_watch (MO_LEAVE, &watch);
	
#ifdef WATCH
	if (WATCH.g_w > 0 && WATCH.g_h > 0) {
		PXY p[5];
		p[1].p_x = p[2].p_x = (p[0].p_x = p[3].p_x = WATCH.g_x) + WATCH.g_w -1;
		p[2].p_y = p[3].p_y = (p[0].p_y = p[1].p_y = WATCH.g_y) + WATCH.g_h -1;
		p[4] = p[0];
		vswr_mode (vdi_handle, MD_XOR);
		vsl_color (vdi_handle, G_BLACK);
		v_hide_c (vdi_handle);
		v_pline (vdi_handle, 5, (short*)p);
		v_show_c (vdi_handle, 1);
		vswr_mode (vdi_handle, MD_TRANS);
	}
#endif
	
	if (wind) {
		if (PE_isLink (elem)) {
			hwWind_setInfo (wind, ((struct url_link*)hash)->address, FALSE);
		} else {
			hwWind_setInfo (wind, NULL, FALSE);
		}
	}
	if (elem != PE_TLINK) {
		hash = NULL;
	}

	if (hwWind_Focus
	    && (hwWind_Focus = hwWind_byValue ((long)hwWind_Focus)) != NULL
	    && hwWind_Focus->isIcon) {
			containr_highlight (hwWind_Focus->Pane, NULL);
	    	hwWind_Focus = NULL;
	
	} else if (hwWind_Focus) {
		BOOL draw;
		if (hwWind_Focus != wind) {
			hwWind_setInfo (hwWind_Focus, NULL, FALSE);
			containr_highlight (hwWind_Focus->Pane, NULL);
			draw = TRUE;
		} else if (containr_highlight (hwWind_Focus->Pane, hash) != hash) {
			draw = TRUE;
		} else {
			hash = NULL;
			draw = FALSE;
		}
		if (draw) {
			hwWind_redraw (hwWind_Focus, &focus_rect);
			hwWind_Focus = NULL;
		}
	}
	if (hash) {
		containr_highlight (cont, hash);
		hwWind_redraw (wind, &clip);
		hwWind_Focus = wind;
		focus_rect   = clip;
	}

	if ((PE_Type (elem) == PE_BORDER) && (cont->Resize == TRUE))
	                              graf_mouse (hwWind_Mshape = THICK_CROSS, NULL);
	else if (elem == PE_EDITABLE) graf_mouse (hwWind_Mshape = TEXT_CRSR,   NULL);
	else if (PE_isActive (elem))  graf_mouse (hwWind_Mshape = POINT_HAND,  NULL);
	else if (hwWind_Mshape)       graf_mouse (hwWind_Mshape = ARROW,       NULL);

#ifdef WATCH
	WATCH = watch;
	if (WATCH.g_w > 0 && WATCH.g_h > 0) {
		PXY p[5];
		p[1].p_x = p[2].p_x = (p[0].p_x = p[3].p_x = WATCH.g_x) + WATCH.g_w -1;
		p[2].p_y = p[3].p_y = (p[0].p_y = p[1].p_y = WATCH.g_y) + WATCH.g_h -1;
		p[4] = p[0];
		vswr_mode (vdi_handle, MD_XOR);
		vsl_color (vdi_handle, G_BLACK);
		v_hide_c (vdi_handle);
		v_pline (vdi_handle, 5, (short*)p);
		v_show_c (vdi_handle, 1);
		vswr_mode (vdi_handle, MD_TRANS);
	}
#endif
}
