#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gemx.h>

#include "global.h"
#include "hwWind.h"
#include "highwire.h"


static WORD type2obj[] = { FNT_RG_NAME, FNT_BD_NAME, FNT_IT_NAME, FNT_BI_NAME };

static OBJECT * fnt_form = NULL;
static WINDOW   fnt_wind = NULL;

static TEDINFO * fnt_name[4];
static char    * fnt_indx[4];

static WORD actv_font = normal_font; /* normal_font, header_font, pre_font */
static WORD actv_type = -1;
static WORD tab_actv = 0x0000;
static WORD tab_pasv = 0x0000;

static WORD saved_font[3][2][2];
static WORD local_font[3][2][2];

static WORD selector = -1;


/*----------------------------------------------------------------------------*/
static BOOL
select_type (WORD type, BOOL draw)
{
	if (type != actv_type) {
		if (actv_type >= 0) {
			fnt_name[actv_type]->te_thickness = 0;
			if (draw) {
				WORD  obj = type2obj[actv_type];
				GRECT rect;
				objc_offset (fnt_form, obj, &rect.g_x, &rect.g_y);
				rect.g_x -= 3;
				rect.g_y -= 3;
				rect.g_w =  fnt_form[obj].ob_width  +6;
				rect.g_h =  fnt_form[obj].ob_height +6;
				window_redraw (fnt_wind, &rect);
			}
		}
		actv_type = type;
		
		if (actv_type >= 0) {
			fnt_name[actv_type]->te_thickness = -1;
			if (draw) {
				WORD  obj = type2obj[actv_type];
				GRECT rect;
				objc_offset (fnt_form, obj, &rect.g_x, &rect.g_y);
				rect.g_x -= 3;
				rect.g_y -= 3;
				rect.g_w =  fnt_form[obj].ob_width  +6;
				rect.g_h =  fnt_form[obj].ob_height +6;
				window_redraw (fnt_wind, &rect);
			}
		}
	}
	return (selector >= 0 && actv_type >= 0);
}

/*----------------------------------------------------------------------------*/
static void
set_entry (short entry, short id)
{
	XFNT_INFO info = { sizeof(XFNT_INFO), };
	const char * text = (vqt_xfntinfo (vdi_handle, 0x0001, id, 0, &info)
	                     ? info.font_name : "???");
	strcpy  (fnt_name[entry]->te_ptext, text);
	sprintf (fnt_indx[entry], "%hu", id);
}

/*----------------------------------------------------------------------------*/
static void
setup_font (WORD font)
{
	short i;
	for (i = 0; i < 4; i++) {
		set_entry (i, local_font[font][i&1][i>>1]);
	}
	actv_font = font;
}

/*----------------------------------------------------------------------------*/
static void
select_font (WORD font)
{
	if (font != actv_font) {
		static WORD font2tab[] = { FNT_SERF, FNT_SANS, FNT_MONO };
		WORD  obj;
		GRECT rect;
		
		obj = font2tab[actv_font];
		fnt_form[obj].ob_spec.tedinfo->te_color = tab_pasv;
		objc_offset (fnt_form, obj, &rect.g_x, &rect.g_y);
		rect.g_w = fnt_form[obj].ob_width;
		rect.g_h = fnt_form[obj].ob_height;
		window_redraw (fnt_wind, &rect);
		
		obj = font2tab[font];
		fnt_form[obj].ob_spec.tedinfo->te_color = tab_actv;
		objc_offset (fnt_form, obj, &rect.g_x, &rect.g_y);
		rect.g_w = fnt_form[obj].ob_width;
		rect.g_h = fnt_form[obj].ob_height;
		window_redraw (fnt_wind, &rect);
		
		select_type (-1, FALSE);
		setup_font (font);
		objc_offset (fnt_form, FNT_LIST, &rect.g_x, &rect.g_y);
		rect.g_w = fnt_form[FNT_LIST].ob_width;
		rect.g_h = fnt_form[FNT_LIST].ob_height;
		window_redraw (fnt_wind, &rect);
	}
}

/*----------------------------------------------------------------------------*/
static BOOL
fonts_apply (BOOL final)
{
	static const char * keyword[3][2][2] = {
		{{"NORMAL",  "ITALIC_NORMAL"},  {"BOLD_NORMAL",  "BOLD_ITALIC_NORMAL"}  },
		{{"HEADER",  "ITALIC_HEADER"},  {"BOLD_HEADER",  "BOLD_ITALIC_HEADER"}  },
		{{"TELETYPE","ITALIC_TELETYPE"},{"BOLD_TELETYPE","BOLD_ITALIC_TELETYPE"}},
	};
	BOOL updt = FALSE;
	WORD n, i, j;
	for (n = 0; n < 3; n++) {
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				if (fonts[n][i][j] != local_font[n][i][j]) {
					fonts[n][i][j] = local_font[n][i][j];
					updt           = TRUE;
				}
				if (final && fonts[n][i][j] != saved_font[n][i][j]) {
					char buff[8];
					sprintf (buff, "%hu", fonts[n][i][j]);
					save_config (keyword[n][i][j], buff);
				}
			}
		}
	}
	if (updt) {
		HwWIND wind = hwWind_Top;
		while (wind) {
			hwWind_history (wind, wind->HistMenu, FALSE);
			wind = hwWind_Next (wind);
		}
	}
	return final;
}

/*----------------------------------------------------------------------------*/
static BOOL
fonts_reset (BOOL final)
{
	BOOL draw = FALSE;
	BOOL updt = FALSE;
	WORD n, i, j;
	for (n = 0; n < 3; n++) {
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				if (local_font[n][i][j] != saved_font[n][i][j]) {
					local_font[n][i][j] = saved_font[n][i][j];
					if (!final) draw   |= (n == actv_font);
				}
				if (fonts[n][i][j] != saved_font[n][i][j]) {
					fonts[n][i][j] = saved_font[n][i][j];
					updt           = TRUE;
				}
			}
		}
	}
	if (draw) {
		GRECT rect;
		setup_font (actv_font);
		objc_offset (fnt_form, FNT_LIST, &rect.g_x, &rect.g_y);
		rect.g_w = fnt_form[FNT_LIST].ob_width;
		rect.g_h = fnt_form[FNT_LIST].ob_height;
		window_redraw (fnt_wind, &rect);
	}
	if (updt) {
		HwWIND wind = hwWind_Top;
		while (wind) {
			hwWind_history (wind, wind->HistMenu, FALSE);
			wind = hwWind_Next (wind);
		}
	}
	return final;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
fontsel_handler (OBJECT * tree, WORD obj)
{
	BOOL ret = FALSE;
	BOOL sel = FALSE;
	
	(void)tree;
	
	if (obj < 0) { /* window close notification from parent class */
		fonts_reset (TRUE);
		ret = TRUE; /* to be deleted */
	
	} else switch (obj) { /* some of the elements got pressed */
		
		case FNT_SERF: select_font (normal_font); break;
		case FNT_SANS: select_font (header_font); break;
		case FNT_MONO: select_font (pre_font);    break;
		
		case FNT_RG_NAME: sel = select_type (0, TRUE); break;
		case FNT_BD_NAME: sel = select_type (1, TRUE); break;
		case FNT_IT_NAME: sel = select_type (2, TRUE); break;
		case FNT_BI_NAME: sel = select_type (3, TRUE); break;
		
		case FNT_OK:    ret = fonts_apply (TRUE);  break;
		case FNT_PREVW: ret = fonts_apply (FALSE); break;
		case FNT_RESET: ret = fonts_reset (FALSE); break;
		case FNT_CANCL: ret = fonts_reset (TRUE);  break;
	}
	
	if (obj > 0 && fnt_form[obj].ob_type == G_BUTTON) {
		GRECT rect;
		fnt_form[obj].ob_state &= ~OS_SELECTED;
		objc_offset (fnt_form, obj, &rect.g_x, &rect.g_y);
		rect.g_x -= 2;
		rect.g_y -= 2;
		rect.g_w =  fnt_form[obj].ob_width  +4;
		rect.g_h =  fnt_form[obj].ob_height +4;
		window_redraw (fnt_wind, &rect);
	}
	
	if (sel) {
		WORD msg[8] = { 0x7A19, 0, 0, -1, 0, 0, G_BLACK, 0x0000 };
		msg[1] = gl_apid;
/*		msg[3] = fnt_wind->Handle;*/
		msg[4] = local_font[actv_font][actv_type&1][actv_type>>1];
		msg[5] = font_size;
		appl_write (selector, 16, msg);
	}
	
	if (ret) {
		fnt_wind = NULL;
	}
	return ret;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
vTab_evMessage (WINDOW This, WORD msg[], PXY mouse, UWORD kstate)
{
	(void)kstate;
	
	if (msg[0] == 0x7A18) {
		WORD obj;
		WORD type;
		if (!This) { /* else internal called */
			obj  = (actv_type < 0 ? -1 : type2obj[actv_type]);
			type = actv_type;
		} else { /* normal event */
			obj = objc_find (fnt_form, ROOT, MAX_DEPTH, mouse.p_x, mouse.p_y);
			switch (obj) {
				case FNT_RG_NAME: type = 0; break;
				case FNT_BD_NAME: type = 1; break;
				case FNT_IT_NAME: type = 2; break;
				case FNT_BI_NAME: type = 3; break;
				default:          type = -1;
			}
		}
/*		printf("obj = %i   %i %i %i %i %i %i 0x%02X\n",
		       obj, msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);*/
		if (type >= 0) {
			GRECT rect;
			if (type != actv_type) {
				select_type (-1, TRUE);
				select_type (type, FALSE);
			}
			local_font[actv_font][type&1][type>>1] = msg[4];
			set_entry (type, msg[4]);
			objc_offset (fnt_form, obj, &rect.g_x, &rect.g_y);
			rect.g_x -= 3;
			rect.g_y -= 3;
			rect.g_w =  fnt_form[obj].ob_width  +6;
			rect.g_h =  fnt_form[obj].ob_height +3 +16;
			window_redraw (fnt_wind, &rect);
		}
		return TRUE;
	}
/*	printf("msg = 0x%04X\n", msg[0]);*/
	return FALSE;
}

/*============================================================================*/
void
fonts_setup (WORD msg[])
{
	if (fnt_wind) {
		window_raise (fnt_wind, TRUE, NULL);
	
	} else {
		short n;
		if (!fnt_form) {
			if (!rsrc_gaddr (R_TREE, FONTSLCT, &fnt_form)) {
				puts ("font_setup(): rsrc_gaddr() failed!");
				exit(1);
			}
			form_center (fnt_form, &n,&n,&n,&n);
			
			fnt_name[0] = fnt_form[FNT_RG_NAME].ob_spec.tedinfo;
			fnt_indx[0] = fnt_form[FNT_RG_INDX].ob_spec.tedinfo->te_ptext;
			fnt_name[1] = fnt_form[FNT_BD_NAME].ob_spec.tedinfo;
			fnt_indx[1] = fnt_form[FNT_BD_INDX].ob_spec.tedinfo->te_ptext;
			fnt_name[2] = fnt_form[FNT_IT_NAME].ob_spec.tedinfo;
			fnt_indx[2] = fnt_form[FNT_IT_INDX].ob_spec.tedinfo->te_ptext;
			fnt_name[3] = fnt_form[FNT_BI_NAME].ob_spec.tedinfo;
			fnt_indx[3] = fnt_form[FNT_BI_INDX].ob_spec.tedinfo->te_ptext;
			
			tab_actv = fnt_form[FNT_SERF].ob_spec.tedinfo->te_color;
			tab_pasv = fnt_form[FNT_SANS].ob_spec.tedinfo->te_color;
		}
		
		for (n = 0; n < 3; n++) {
			saved_font[n][0][0] = local_font[n][0][0] = fonts[n][0][0];
			saved_font[n][0][1] = local_font[n][0][1] = fonts[n][0][1];
			saved_font[n][1][0] = local_font[n][1][0] = fonts[n][1][0];
			saved_font[n][1][1] = local_font[n][1][1] = fonts[n][1][1];
		}
		setup_font  (actv_font);
		select_type (-1, FALSE);
		
		if (selector == -1) {
			const char * env = getenv ("FONTSELECT");
			if (!env || !*env) {
				selector = -2;
			} else {
				char buff[9];
				strncpy (buff, env, 8);
				buff[8] = '\0';
				while (strlen (buff) < 8) {
					strcat (buff, " ");
				}
				if ((n = appl_find (buff)) >= 0) {
					selector = n;
				}
			}
		}
		
		n = formwind_do (fnt_form, 0, "Font Setup", FALSE, fontsel_handler);
		fnt_wind = window_byHandle (n);
		if (!fnt_wind) return;  /* something went wrong */
		
		fnt_wind->evMessage = vTab_evMessage;
	}
	
	if (msg) {
		 PXY mouse = { 0, 0 };
		 (*fnt_wind->evMessage)(NULL, msg, mouse, 0x0000);
	}
}
