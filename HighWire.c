/* @(#)highwire/HighWire.c
 */
#include <stdlib.h>
#include <stdio.h>

#ifdef __PUREC__
#include <tos.h>
#include <string.h> /* for memcmp() */

#else /* LATTICE || __GNUC__ */
#include <mintbind.h>
#include <string.h> /* for memcmp() */
#endif

#include <gemx.h>

#include "global.h"
#include "Logging.h"
#include "schedule.h"
#define WINDOW_t WINDOW
#include "hwWind.h"
#ifdef GEM_MENU
# include "highwire.h"
#endif
#include "ovl_sys.h"


extern char *gslongname;
extern char *gsanswer;
extern char *gsi;  /* extern GS_INFO *gsi; */
extern char *va_helpbuf;


static void highwire_ex(void);
static void open_splash(void);


VDI_Workstation vdi_dev;

#ifdef GEM_MENU
OBJECT *about;
OBJECT *rpopup;
OBJECT *menutree;
OBJECT *rpoplink;
#endif

static EVMULT_IN multi_in = {
	MU_MESAG|MU_BUTTON|MU_TIMER|MU_KEYBD /*|MU_M1 */,

	0x102, 3, 0,			/* mouses */

	MO_LEAVE, { -1,-1, 1, 1 },  /* M1 */
	0,        {  0, 0, 0, 0 },  /* M2 */
	1, 0                        /* Timer */
};


/*============================================================================*/
int
main (int argc, char **argv)
{
	WORD info, u;
	BOOL quit = FALSE;
	long gdostype;
	char def_address[] = "html\\highwire.htm";

	if (V_Opnvwk (&vdi_dev) <= 0) {
		exit (EXIT_FAILURE);
	}
	atexit (highwire_ex);

	if (appl_init() < 0) {
		exit (EXIT_FAILURE);
	}

	/* HighWire needs a Speedo fonts GDOS, that is SpeedoGDOS or NVDI ò 3.
	 */
	gdostype = vq_vgdos();

/*  This next test is a potential candidate if the -65536 test doesn't work */
/*	if ((gdostype == GDOS_FNT)||(gdostype == -2)) {*/

	if ((gdostype != GDOS_FSM) && (gdostype != -65536L)
	    && (memcmp (&gdostype, "fVDI", 4) != 0)) {
		hwUi_fatal (NULL, _ERROR_SPEEDO_);
	}
	
	open_splash();
	
	/* Allocate all GLOBAL memory merged to one block.  The reason for using one
	 * block is, that MiNT with memory protection internally allocates 8 KiB.
	 * GLOBAL is needed for inter process communication under memory protection.
	 * 'Sysconf()' is from Frank Naumann in article
	 * <199904241138.a34259@l2.maus.de> in Maus.Computer.Atari.Programmieren.
	 */
	if (Sysconf(-1) != -EINVFN) {
		short mode = 0x0003  /* prefer TT ram    */
		           | 0x0020; /* global accesible */
		gslongname = (char *)Mxalloc (32 + 16 + 2 * HW_PATH_MAX, mode);
	} else {
		gslongname = (char *)Malloc (32 + 16 + 2 * HW_PATH_MAX);
	}
	if (!gslongname) {
		hwUi_fatal (NULL, _ERROR_NOMEM_);
	}
	gsi        = gslongname + 32;
	gsanswer   = gsi + 16/*sizeof(GS_INFO)*/;
	va_helpbuf = gsanswer + HW_PATH_MAX;
	Init_AV_Protocol();

	if (ignore_colours) {
		/* set the scroller widget colours for monochrome */
		slider_bkg = G_WHITE;
		slider_col = G_WHITE;
	} else {
		/* set default colour for page background */
		background_colour = G_LWHITE;
	}

	/* identify the AES and issue appropriate initialization calls
	 */
	if (sys_type() & (SYS_MAGIC|SYS_NAES|SYS_XAAES)) {
	/* If you know a documented length greater than 79 characters, add it. */
		if (sys_NAES()) {
			aes_max_window_title_length = 127;
		}
		/* our name in the application menu */
		menu_register(gl_apid, "  HighWire " _HIGHWIRE_VERSION_);
	}
	(void)Pdomain(1);  /* we know about long file names */

#ifdef GEM_MENU
	if (!rsrc_load(_HIGHWIRE_RSC_)) {
		hwUi_fatal (NULL, _ERROR_NORSC_);
	}
	rsrc_gaddr(R_TREE, MENUTREE, &menutree);
	rsrc_gaddr(R_TREE, ABOUT, &about);
	rsrc_gaddr(R_TREE, RPOPUP, &rpopup);
	rsrc_gaddr(R_TREE, RLINKPOP, &rpoplink);

	menu_bar(menutree, MENU_INSTALL);
#endif

	if (appl_xgetinfo(12, &info, &u, &u, &u) && (info & 8))
/*	if (appl_xgetinfo(AES_MESSAGE, &info, &u, &u, &u) && (info & 8))*/
		/* we know about AP_TERM message */
		shel_write(SWM_NEWMSG, 1, 0, NULL, NULL);

	/* grab the screen colors for later use */
	save_colors();

	if (sys_type() & SYS_TOS) {
		init_logging();
	}
	
	/* init paths and load config */
	init_paths();

	vst_load_fonts (vdi_handle, 0);

	vst_scratch (vdi_handle, SCRATCH_BOTH);
	vst_kern (vdi_handle, TRACK_NORMAL, PAIR_ON, &u, &u);
	vswr_mode (vdi_handle, MD_TRANS);

	u = 0;
	if (argc > 1) {
		short i = 1;
		do {
			if (argv[i][0] != '-' && new_hwWind ("", argv[i], NULL)) {
				u++;
			}
		} while (++i < argc);
	}
	if (!u && !new_hwWind ("", def_address, NULL)) {
		hwUi_fatal (NULL, "Can't open main window.");
	}
	set_mouse_watch (MO_ENTER, &hwWind_Top->Work);

#if 0
load_sampleovl();
#endif

	while (!quit)
	{
		WORD       msg[8];
		EVMULT_OUT out;
		short      event = evnt_multi_fast (&multi_in, msg, &out);

		if (event & MU_M1) {
			check_mouse_position (out.emo_mouse.p_x, out.emo_mouse.p_y);
		}
		if (event & MU_MESAG) {
			quit = process_messages (msg, out.emo_mouse, out.emo_kmeta);
		}
		if (event & MU_BUTTON) {
			button_clicked (out.emo_mbutton, out.emo_mclicks, out.emo_kmeta,
			                out.emo_mouse.p_x, out.emo_mouse.p_y);
		}
		if (event & MU_KEYBD) {
			key_pressed (out.emo_kreturn, out.emo_kmeta);
		}
		schedule (1);
	}

	/* Now the C library calls highwire_ex(). */
	return EXIT_SUCCESS;
}  /* main() */


/*============================================================================*/
void
set_mouse_watch (WORD leaveNenter, const GRECT * watch)
{
	if (!watch) {
		multi_in.emi_flags &= ~MU_M1;
	
	} else {
		multi_in.emi_flags  |= MU_M1;
		multi_in.emi_m1leave = leaveNenter;
		multi_in.emi_m1      = *watch;
	}
}

const GRECT * mouse_watch = &multi_in.emi_m1;


/*----------------------------------------------------------------------------*/
/* The HighWire exit code called by the C library.
 * Frees resources other than memory and files.
 */
static void
highwire_ex (void)
{
	if (gl_apid > 0) {
		
		kill_ovl (NULL);
		
		Exit_AV_Protocol();

	#ifdef GEM_MENU
		menu_bar(menutree, MENU_REMOVE);
		rsrc_free();
	#endif

		appl_exit ();
	}
	vst_unload_fonts (vdi_handle, 0);
	v_clsvwk         (vdi_handle);
}


/******************************************************************************/

extern MFDB       logo_icon;
static WINDOW     splash     = NULL;
static const char spl_name[] = _HIGHWIRE_FULLNAME_;
static const char spl_vers[] = _HIGHWIRE_VERSION_" "_HIGHWIRE_BETATAG_;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
close_job (void * arg, long invalidated)
{
	(void)invalidated;
	if (arg == splash) {
		free (window_dtor(splash));
		splash = NULL;
	}
	return FALSE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static void
vTab_draw (WINDOW This, const GRECT * clip)
{
	MFDB  scrn     = { NULL, };
	WORD  color[2] = { G_BLACK, G_LBLACK }, dmy;
	PXY   p[4];
	p[1].p_x = (p[0].p_x = clip->g_x) + clip->g_w -1;
	p[1].p_y = (p[0].p_y = clip->g_y) + clip->g_h -1;
	vs_clip_pxy (vdi_handle, p);
	wind_get_grect (This->Handle, WF_CURRXYWH, (GRECT*)(p +2));
	p[3].p_x = (p[2].p_x += 8) + logo_icon.fd_w -1;
	p[3].p_y = (p[2].p_y += 8) + logo_icon.fd_h -1;
	vswr_mode     (vdi_handle, MD_TRANS);
	vsf_color     (vdi_handle, G_LWHITE);
	vst_alignment (vdi_handle, TA_LEFT, TA_TOP, &dmy,&dmy);
	vst_color     (vdi_handle, G_BLACK);
	vst_effects   (vdi_handle, TXT_NORMAL);
	vst_font      (vdi_handle, 1);
	v_hide_c (vdi_handle);
	v_bar (vdi_handle, (short*)p);
	p[0].p_x = p[0].p_y = 0;
	p[1] = *(PXY*)&logo_icon.fd_w;
	vrt_cpyfm (vdi_handle, MD_TRANS, (short*)p, &logo_icon, &scrn, color);
	v_gtext (vdi_handle, p[3].p_x +8, p[2].p_y,     spl_name);
	v_gtext (vdi_handle, p[3].p_x +8, p[2].p_y +16, spl_vers);
	v_show_c (vdi_handle, 1);
	vst_alignment (vdi_handle, TA_LEFT, TA_BASE, &dmy,&dmy);
	vs_clip_off (vdi_handle);
}

/*----------------------------------------------------------------------------*/
static void
open_splash (void)
{
	GRECT curr;
	WORD w = 8 + logo_icon.fd_w + 8 + (sizeof(spl_vers) -1) *8 + 8;
	WORD h = 8 + logo_icon.fd_h + 8;
	wind_get_grect (DESKTOP_HANDLE, WF_WORKXYWH, &curr);
	curr.g_x += (curr.g_w - w) /2;
	curr.g_y += (curr.g_h - h) /2;
	curr.g_w =  w;
	curr.g_h =  h;
	splash = window_ctor (malloc (sizeof (WINDOWBASE)), 0, &curr, TRUE);
	splash->drawWork = vTab_draw;
	window_redraw (splash, NULL);
	sched_insert (close_job, splash, 0l, 20);
}


