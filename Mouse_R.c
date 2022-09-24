/* @(#)highwire/Mouse_R.c
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h> /* debug */

#include <gemx.h>

#include "token.h"

#ifdef __PUREC__
# define CONTAINR struct s_containr *
#endif
#include "global.h"

#include "bookmark.h"
#include "Containr.h"
#include "Loader.h"
#include "Location.h"
#include "Form.h"
#include "hwWind.h"


/*----------------------------------------------------------------------------*/
static DOMBOX * elem_box (UWORD elem, void * hash, WORDITEM * word)
{
	DOMBOX * box  = NULL;
	if (elem == PE_TLINK) {
		struct url_link * link = hash;
		*word = link->start;
	} else if (elem == PE_TEXT) {
		*word = hash;
	} else if (elem == PE_PARAGRPH) {
		PARAGRPH par = hash;
		if (par) {
			box = &par->Box;
		}
		*word = NULL;
	} else {
		*word = NULL;
	}
	if (*word) {
		WORDLINE line = (*word)->line;
		PARAGRPH par  = (line ? line->Paragraph : NULL);
		if (par) {
			box = &par->Box;
		}
	}
	return box;
}

/*----------------------------------------------------------------------------*/
/* Mouse click in bookmark window                                             */

static BOOL
bmrk_clicked (PXY mouse, WORD button, WORD clicks,
              HwWIND wind, FRAME frame,UWORD elem, void * hash)
{
	BOOL     done = FALSE;
	WORDITEM word = NULL;
	DOMBOX * box  = elem_box (elem, hash, &word);
	if (box && (!box->IdName || (strcmp (box->ClName, "LNK") != 0 &&
	                             strcmp (box->ClName, "GRP") != 0))) {
		box = NULL;
	}
	
	if (button & RIGHT_BUTTON) { /*............................................*/
		if (box || elem == PE_FRAME) {
			rpopbkm_open (mouse.p_x, mouse.p_y, box, word);
			done = TRUE;
		}
	
	} else if (clicks >= 2) { /*...............................................*/
		const char * lnk = NULL;
		const char * grp = NULL;
		if (box) {
			if (*box->ClName == 'L') lnk = box->IdName;
			else               /*G*/ grp = box->IdName;
			if (!grp && box->Parent) {
				box = box->Parent;
				grp = (box->ClName ? box->ClName : NULL);
			}
		}
		if (grp && !lnk) {
			DOMBOX * next = box->Sibling;
			while (next) {
				if (next->HtmlCode == TAG_DL && strcmp (next->ClName, grp) == 0) {
					BOOL  hidden = next->Hidden;
					next->Hidden = !hidden;
					if (containr_calculate (frame->Container, NULL)) {
						hwWind_redraw (wind, NULL);
						set_bookmark_group (grp, hidden);
					}
					done = TRUE;
					break;
				}
				next = next->Sibling;
			}
		}
	
	} else if (box && box->IdName) { /*........................................*/
		HwWIND     target = wind;
		BOOL      dragged = FALSE;
		const char * id   = box->IdName;
		WORD         bgnd = box->Backgnd;
		GRECT     rect = {0,0,0,0};
		WORD      dx = 0, dy = 0;
		PXY       p[5];
		DOMBOX *  hide  = NULL;
		short     event = 0;
		EVMULT_IN m_in  = { MU_TIMER|MU_BUTTON|MU_M2, 1, 0x03, 0x00,
		                    MO_LEAVE,{0,0,1,1}, MO_LEAVE,{0,0,0,0}, 200,0 };
		EVMULT_OUT out;
		wind_update (BEG_MCTRL);
		do {
			WORD msg[8];
			event = evnt_multi_fast (&m_in, msg, &out);
			
			if (event & MU_TIMER) {
				long x, y;
				dombox_Offset (box, &x, &y);
				rect.g_x = (WORD)(x + (long)frame->clip.g_x - frame->h_bar.scroll);
				rect.g_y = (WORD)(y + (long)frame->clip.g_y - frame->v_bar.scroll);
				rect.g_w = (WORD)box->Rect.W;
				rect.g_h = (WORD)box->Rect.H;
				
				box->Backgnd = G_YELLOW;
				if (*box->ClName == 'G') {
					hide = box->Sibling;
					while (hide) {
						if (hide->HtmlCode == TAG_DL
						    && strcmp (hide->ClName, id) == 0) {
							if (hide->Hidden) {
								hide = NULL;
							} else {
								hide->Hidden = TRUE;
								if (containr_calculate (frame->Container, NULL)) {
									hwWind_redraw (wind, NULL);
								} else {
									hide->Hidden = FALSE;
									hide = NULL;
								}
							}
							break;
						}
						hide = hide->Sibling;
					}
				}
				if (!hide) {
					hwWind_redraw (wind, &rect);
				}
				graf_mouse (FLAT_HAND, NULL);
				dx = rect.g_x - out.emo_mouse.p_x;
				dy = rect.g_y - out.emo_mouse.p_y;
				vswr_mode (vdi_handle, MD_XOR);
				vsl_type  (vdi_handle, USERLINE);
				vsl_udsty (vdi_handle, (short)0xAAAA);
				
				m_in.emi_flags &= ~MU_TIMER;
				event  |= MU_M1;
				dragged = TRUE;
			}
			
			if (event & (MU_M1|MU_BUTTON)) {
				v_hide_c (vdi_handle);
				if (!(m_in.emi_flags & MU_M1)) {
					m_in.emi_flags |= MU_M1;
				} else {
					v_pline (vdi_handle, 5, &p[0].p_x);
				}
				if (!(event & MU_BUTTON)) {
					p[0].p_x = p[3].p_x = out.emo_mouse.p_x + dx;
					p[1].p_x = p[2].p_x = p[0].p_x + rect.g_w -1;
					p[0].p_y = p[1].p_y = out.emo_mouse.p_y + dy;
					p[2].p_y = p[3].p_y = p[0].p_y + rect.g_h -1;
					p[4]     = p[0];
					v_pline (vdi_handle, 5, &p[0].p_x);
				}
				v_show_c (vdi_handle, 1);
				m_in.emi_m1.g_x = out.emo_mouse.p_x;
				m_in.emi_m1.g_y = out.emo_mouse.p_y;
			}
			
			if (event & (MU_M2)) {
				static const MFORM mform = {
					7,7, 1, G_WHITE, G_RED,
					{	0x03C0,0x0FF0,0x1FF8,0x3FFC, 0x7C7E,0x78FE,0xF1FF,0xF3EF,
						0xF7CF,0xFF8F,0x7F1E,0x7E3E, 0x3FFC,0x1FF8,0x0FF0,0x03C0	},
					{	0x0000,0x03C0,0x0FF0,0x1C38, 0x303C,0x307C,0x60E6,0x61C6,
						0x6386,0x6706,0x3E0C,0x3C0C, 0x1C38,0x0FF0,0x03C0,0x0000	}
				/*	7,7, 1, G_WHITE, G_BLACK,
					{	0xC007,0xF00F,0xF81F,0x7C3E, 0x3E7C,0x1FF8,0x0FF0,0x07E0,
						0x07E0,0x0FF0,0x1FF8,0x3E7C, 0x7C3E,0xF81F,0xF00F,0xC007	},
					{	0x0000,0x6006,0x700E,0x381C, 0x1C38,0x0E70,0x07E0,0x03C0,
						0x03C0,0x07E0,0x0E70,0x1C38, 0x381C,0x700E,0x6006,0x0000	}*/
				};
				WORD hdl = wind_find (out.emo_mouse.p_x, out.emo_mouse.p_y);
				BOOL flt;
				wind_get_grect (hdl, WF_FIRSTXYWH, &m_in.emi_m2);
				while (m_in.emi_m2.g_w > 0 && m_in.emi_m2.g_h > 0) {
					if (out.emo_mouse.p_x >= m_in.emi_m2.g_x                   &&
					    out.emo_mouse.p_x <  m_in.emi_m2.g_x + m_in.emi_m2.g_w &&
					    out.emo_mouse.p_y >= m_in.emi_m2.g_y                   &&
					    out.emo_mouse.p_y <  m_in.emi_m2.g_y + m_in.emi_m2.g_h) {
						break;
					}
					wind_get_grect (hdl, WF_NEXTXYWH, &m_in.emi_m2);
				}
				if (m_in.emi_m2.g_w <= 0 || m_in.emi_m2.g_h <= 0) {
					m_in.emi_m2.g_x = out.emo_mouse.p_x;
					m_in.emi_m2.g_y = out.emo_mouse.p_y;
					m_in.emi_m2.g_w = m_in.emi_m2.g_h = 1;
					flt    = FALSE;
					target = NULL;
				} else if ((target = hwWind_byHandle (hdl)) == NULL) {
					flt    = FALSE;
				} else if (target != wind
				           && (*id != 'L' || target->Base.Ident != WIDENT_BRWS)) {
					flt    = FALSE;
					target = NULL;
				} else {
					flt    = TRUE;
				}
				graf_mouse ((flt ? FLAT_HAND : USER_DEF), &mform);
/*printf ("%i,%i/%i,%i\n",
        m_in.emi_m2.g_x,m_in.emi_m2.g_y,m_in.emi_m2.g_w,m_in.emi_m2.g_h);*/
			}
		} while (!(event & MU_BUTTON));
		wind_update (END_MCTRL);
		
		if (dragged) {
			vsl_type  (vdi_handle, SOLID);
			vswr_mode (vdi_handle, MD_TRANS);
			box->Backgnd = bgnd;
			if (target == wind) {
				CONTAINR     cont  = frame->Container;
				DOMBOX     * other = NULL;
				const char * where = NULL;
				GRECT        watch;
				elem  = containr_Element (&cont,
				                          out.emo_mouse.p_x, out.emo_mouse.p_y,
				                          &watch, NULL, &hash);
				other = elem_box (elem, hash, &word);
				if (other && (!other->IdName
				              || (strcmp (other->ClName, "LNK") != 0 &&
				                  strcmp (other->ClName, "GRP") != 0))) {
					other = NULL;
				}
				if (other) {
					where = other->IdName;
				} else if (elem == PE_FRAME) {
					long my = out.emo_mouse.p_y - frame->clip.g_y;
					if (my >= frame->clip.g_h - dombox_BotDist (&frame->Page)) {
						where = ">";
					} else if (my < dombox_TopDist (&frame->Page)) {
						where = "<";
					}
				}
				if (pos_bookmark_entry (id, where)) {
					hwWind_history (wind, wind->HistMenu, TRUE);
					done = TRUE;
				}
			} else if (target) { /* WIDENT_BRWS */
				const char * url = (word && word->link
				                    ? word->link->address : NULL);
				if (url) {
					start_page_load (target->Pane, url, NULL, TRUE, NULL);
				}
			}
			if (!done) {
				if (hide) {
					hide->Hidden = FALSE;
					containr_calculate (frame->Container, NULL);
					hwWind_redraw (wind, NULL);
				} else {
					hwWind_redraw (wind, &rect);
				}
				graf_mouse (ARROW, NULL); /* shouldn't be necessary... */
				check_mouse_position (out.emo_mouse.p_x, out.emo_mouse.p_y);
				done = TRUE;
			}
		}
	}
	
	return done;
}

/*==============================================================================
 *
 * handles mouse interaction with a frame
*/
void
button_clicked (CONTAINR cont, WORD button, WORD clicks, UWORD state, PXY mouse)
{
	void * hash  = NULL;
	void * txt_o;
	GRECT  watch;
	WORD   decx = 0, decy = 0;
	
	EVMULT_IN multi_in = { 0, };
	
	HwWIND  wind = hwWind_byContainr (cont);
	void * txt_i = wind->Input;
	UWORD  elem  = containr_Element (&cont, mouse.p_x, mouse.p_y,
	                                 &watch, NULL, &hash);
	FRAME  frame = hwWind_setActive (wind, cont, NULL);
	
#if defined(GEM_MENU) && (_HIGHWIRE_ENCMENU_ == 1)
	if (frame) {
		update_menu (frame->Encoding, (frame->MimeType == MIME_TXT_PLAIN));
	}
#endif
	if (wind->Base.Ident == WIDENT_BMRK          /* bookmark window  */
	    && bmrk_clicked (mouse, button, clicks, wind, frame, elem, hash)	) {
		return;
	}
	
	txt_o = txt_i;
	
	if ((button & RIGHT_BUTTON) &&
	    (elem >= PE_PARAGRPH || elem == PE_EMPTY || elem == PE_FRAME)) {
#if (_HIGHWIRE_RPOP_ == 1)
		if (elem == PE_ILINK) {
			rpopilink_open (mouse.p_x, mouse.p_y, cont, hash);			
		} else if (elem == PE_IMAGE) {
			rpopimg_open (mouse.p_x, mouse.p_y, cont);			
		} else if (elem > PE_IMAGE && elem < PE_INPUT) {
			rpoplink_open (mouse.p_x, mouse.p_y, cont, hash);	
		} else {
			rpopup_open (mouse.p_x, mouse.p_y);
		}
#endif
	
	} else switch (PE_Type (elem)) {
		
		case PE_VBAR: {
			BOOL drag = FALSE;
			long step = 0;
		
			WORD y = mouse.p_y - frame->clip.g_y;
			
			if (y >= frame->v_bar.pos + frame->v_bar.size) {
				
				if (y >= frame->v_bar.rd) {                         /* down arrow */
					step = (clicks == 1 ? scroll_step : frame->Page.Rect.H);
					step = (button & LEFT_BUTTON ? +step : -step);
				
				} else if (button & LEFT_BUTTON) {                   /* down page */
					step = +(frame->clip.g_h - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decy = frame->v_bar.size /2;
				}
			
			} else if (y <= frame->v_bar.pos) {
				
				if (y <= frame->v_bar.lu) {                           /* up arrow */
					step = (clicks == 1 ? scroll_step : frame->Page.Rect.H);
					step = (button & LEFT_BUTTON ? -step : +step);
				
				} else if (button & LEFT_BUTTON) {                     /* up page */
					step = -(frame->clip.g_h - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decy = frame->v_bar.size /2;
				}
			
			} else if (button & LEFT_BUTTON) {                       /* realtime */
				drag = TRUE;
				decy = mouse.p_y - frame->clip.g_y - frame->v_bar.pos;
		
			} else {                                                   /* slider */
				WORD s_x  = frame->clip.g_x + frame->clip.g_w;
				WORD s_y  = 0;
				WORD sldr = frame->v_bar.rd - frame->v_bar.lu +1;
				graf_dragbox (scroll_bar_width, frame->v_bar.size,
				              s_x, frame->clip.g_y + frame->v_bar.pos,
				              s_x, frame->clip.g_y + frame->v_bar.lu,
				              scroll_bar_width, sldr,
				              &s_x, &s_y);
				sldr -= frame->v_bar.size;
				step  = ((frame->Page.Rect.H - frame->clip.g_h)
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
		
			WORD x = mouse.p_x - frame->clip.g_x;
			
			if (x >= frame->h_bar.pos + frame->h_bar.size) {
				
				if (x >= frame->h_bar.rd) {                        /* right arrow */
					step = (clicks == 1 ? scroll_step : frame->Page.Rect.W);
					step = (button & LEFT_BUTTON ? +step : -step);
				
				} else if (button & LEFT_BUTTON) {                  /* right page */
					step = +(frame->clip.g_w - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decx = frame->h_bar.size /2;
				}
			
			} else if (x <= frame->h_bar.pos) {
				
				if (x <= frame->h_bar.lu) {                         /* left arrow */
					step = (clicks == 1 ? scroll_step : frame->Page.Rect.W);
					step = (button & LEFT_BUTTON ? -step : +step);
				
				} else if (button & LEFT_BUTTON) {                   /* left page */
					step = -(frame->clip.g_w - scroll_step);
			
				} else {                                          /* absolute set */
					drag = TRUE;
					decx = frame->h_bar.size /2;
				}
			
			} else if (button & LEFT_BUTTON) {                       /* realtime */
				drag = TRUE;
				decx = mouse.p_x - frame->clip.g_x - frame->h_bar.pos;
		
			} else {                                                   /* slider */
				WORD s_x  = 0;
				WORD s_y  = frame->clip.g_y + frame->clip.g_h;
				WORD sldr = frame->h_bar.rd - frame->h_bar.lu +1;
				graf_dragbox (frame->h_bar.size, scroll_bar_width,
				              frame->clip.g_x + frame->h_bar.pos, s_y,
				              frame->clip.g_x + frame->h_bar.lu,  s_y,
				              sldr, scroll_bar_width,
				              &s_x, &s_y);
				sldr -= frame->h_bar.size;
				step  = ((frame->Page.Rect.W - frame->clip.g_w)
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
			hwWind_raise (wind, TRUE);
			if (!wind->Base.Prev) {
				WORD    value = 0;
				char ** popup = NULL;
				GRECT   radio;
				PXY     pxy;
				pxy.p_x = mouse.p_x - watch.g_x;
				pxy.p_y = mouse.p_y - watch.g_y;
				switch (input_handle (hash, pxy, &radio, &popup)) {
					case 2: radio.g_x += watch.g_x;
					        radio.g_y += watch.g_y;
					        hwWind_redraw (wind, &radio);
					case 1: if (popup) {
					           long * xy = (long*)&radio; /* CHECKME */
					           xy[0] += frame->clip.g_x - frame->h_bar.scroll;
					           xy[1] += frame->clip.g_y - frame->v_bar.scroll;
					           value = HW_form_popup (popup, (WORD)xy[0], (WORD)xy[1], FALSE);
					        } else {
					           WORD dmy;
					           if (input_isEdit (hash)) {
					              txt_i = txt_o = NULL;
					              hwWind_setActive (wind, cont, hash);
					           }
					           hwWind_redraw (wind, &watch);
					           evnt_button (1, 0x03, 0x00, &dmy,&dmy,&dmy,&dmy);
					        }
					        if (input_activate (hash, value)) {
					           hwWind_redraw (wind, &watch);
					        }
				}
			}
		}	break;
		
		case PE_TLINK: case PE_ILINK: {
			WORD dmy;
			struct url_link * link = hash;
			const char      * addr = link->address;
			CONTAINR target = NULL;

			if (link->u.target && stricmp(link->u.target, "_hw_top") == 0) {
				HwWIND this = hwWind_byType (0);

				if (this != NULL) {
					hwWind_raise (this, TRUE);

					cont = this->Pane;
					target = containr_byName (cont, "_top");
				} else {
					target = NULL;
				}
				
			} else {
				target = (link->u.target &&
			                   stricmp (link->u.target, "_blank") != 0
			                   ? containr_byName (cont, link->u.target) : NULL);
			}
			
			if (target) {
				const char * p = strchr (addr, '#');
				FRAME  t_frame = containr_Frame (target);
				if (p && t_frame) {
					const char * file = t_frame->Location->File;
					if (strncmp (file, addr, (p - addr)) == 0 && !file[p - addr]) {
						addr = p;
						cont = target;   /* content matches */
					}
				}
			}
			if (*addr == '#') {
				long dx, dy;
				if (containr_Anchor (cont, addr, &dx, &dy)) {
					hwWind_scroll (wind, cont, dx, dy);
					check_mouse_position (mouse.p_x, mouse.p_y);
				}
			} else {
				char buf[2 * HW_PATH_MAX];

				if (state & K_ALT) {
					cont = NULL;
				} else if (link->u.target) {
					cont = target;
				}

				if (*addr == '?') {
					char * p;
					size_t buf_len, p_len;

					location_FullName (frame->Location, buf, sizeof(buf));

					buf_len = strlen(buf);

					if ((p = strchr (buf, '?')) != NULL) {
						p_len = strlen(p);
						buf[(buf_len - p_len)] = '\0';
					}
					strcat(buf, addr);

					addr = buf;
				}
				
				if (!cont) {
					BOOL topNbot = !(state & (K_RSHIFT|K_LSHIFT));
					if ((wind = new_hwWind (addr, NULL, topNbot)) != NULL) {
						cont = wind->Pane;
					}
				}

				if (cont) {
					LOCATION ref = (PROTO_isRemote (frame->Location->Proto)
					                ? location_share (frame->Location) : NULL);
					LOADER   ldr = start_page_load (cont, addr, frame->BaseHref,
					                                TRUE, NULL);
					if (ldr) {
						if (location_equalHost (ref, ldr->Location)
					       && frame->AuthRealm && frame->AuthBasic) {
							ldr->AuthRealm = strdup (frame->AuthRealm);
							ldr->AuthBasic = strdup (frame->AuthBasic);
						}
						ldr->Referer  = location_share (ref);
						ldr->Encoding = link->encoding;
					}
					free_location (&ref);
				}
			}
			txt_i = NULL;
			evnt_button (1, 0x03, 0x00, &dmy,&dmy,&dmy,&dmy);
		} break;
		
		case PE_BORDER: {
			CONTAINR sibl  = cont->Sibling;
			GRECT    bound = cont->Area;
			WORD dx, dy;
			
			if (elem == PE_BORDER_RT) {
				bound.g_w += sibl->Area.g_w - (sibl->Sibling ? sibl->BorderSize :0);
			} else { /* PE_BORDER_DN */
				bound.g_h += sibl->Area.g_h - (sibl->Sibling ? sibl->BorderSize :0);
			}
			graf_dragbox (watch.g_w, watch.g_h, watch.g_x, watch.g_y,
			              bound.g_x, bound.g_y, bound.g_w, bound.g_h, &dx, &dy);
			if ((dx -= watch.g_x) | (dy -= watch.g_y)) {
				cont->Area.g_w += dx;
				cont->Area.g_h += dy;
				containr_calculate (cont, NULL);
				sibl->Area.g_x += dx;
				sibl->Area.g_y += dy;
				sibl->Area.g_w -= dx;
				sibl->Area.g_h -= dy;
				containr_calculate (sibl, NULL);
				hwWind_redraw (wind, &bound);
			}
		}	break;
		
		default:
			if (wind) {
				hwWind_raise (wind, TRUE);
			}
			txt_i = NULL;
	}
	
	if (multi_in.emi_flags) {
		long pg_h = frame->Page.Rect.H - frame->clip.g_h;
		WORD sl_h = frame->v_bar.rd - frame->v_bar.lu - frame->v_bar.size;
		long top  = frame->clip.g_y + frame->v_bar.lu +1;
		long bot  = frame->clip.g_y + frame->v_bar.rd -1;
		long pg_w = frame->Page.Rect.W - frame->clip.g_w;
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
	
	if (txt_i) {   /* restore previous active text input field */
		hwWind_setActive (wind, cont, txt_i);
	
	} else if (txt_o) {   /* deactivate previous active text input field */
		WORDITEM word = input_activate (txt_o, -1);
		if (word) {
			long  x, y;
			frame     = (FRAME)dombox_Offset (&word->line->Paragraph->Box, &x, &y);
			watch.g_x = (WORD)(x + frame->clip.g_x - frame->h_bar.scroll
			              + word->h_offset);
			watch.g_y = (WORD)(y + frame->clip.g_y - frame->v_bar.scroll
			              + word->line->OffsetY - word->word_height);
			watch.g_w = word->word_width;
			watch.g_h = word->word_height + word->word_tail_drop;
			hwWind_redraw (wind, &watch);
		}
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
	HwWIND   wind = hwWind_mouse  (mx, my, &watch);
	
	if (wind) {
		GRECT rect;
		clip = wind->Work;
		cont = wind->Pane;
		elem = containr_Element (&cont, mx, my, &rect, &clip, &hash);
		rc_intersect (&rect, &watch);
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
	    && hwWind_Focus->Base.isIcon) {
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
