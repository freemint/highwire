/* @(#)highwire/HighWire.c
 */
#include <stdlib.h>
#include <stdio.h>

#ifdef __PUREC__
#include <tos.h>

#else /* LATTICE || __GNUC__ */
#include <mintbind.h>
#endif

#include <gemx.h>

#include "global.h"
#include "Logging.h"
#include "schedule.h"
#include "hwWind.h"
#ifdef GEM_MENU
# include "highwire.h"
#endif


extern char *gslongname;
extern char *gsanswer;
extern char *gsi;  /* extern GS_INFO *gsi; */
extern char *va_helpbuf;


static void highwire_ex(void);


VDI_Workstation vdi_dev;

#ifdef GEM_MENU
OBJECT *about;
OBJECT *rpopup;
OBJECT *menutree;
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
	WORD AES_type;
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

	if ((gdostype != GDOS_FSM)&&(gdostype != -65536L)) {
		form_alert(1, _ERROR_SPEEDO_);
		exit(EXIT_FAILURE);
	}

	/* Allocate all GLOBAL memory merged to one block.  The reason for using one
	 * block is, that MiNT with memory protection internally allocates 8 KiB.
	 * GLOBAL is needed for inter process communication under memory protection.
	 * 'Sysconf()' is from Frank Naumann in article
	 * <199904241138.a34259@l2.maus.de> in Maus.Computer.Atari.Programmieren.
     *
     * here is the old routine in case the new one is problematic
	 *	if (can_extended_mxalloc()) gslongname = (char *)Mxalloc(128 + HW_PATH_MAX, ALLOCMODE);
	 */
	if (Sysconf(-1) != EINVFN)
		gslongname = (char *)Mxalloc(32 + 16 + 2 * HW_PATH_MAX, ALLOCMODE);
	else
		gslongname = (char *)Malloc(32 + 16 + 2 * HW_PATH_MAX);
	if (!gslongname) {
		form_alert(1, _ERROR_NOMEM_);
		exit(EXIT_FAILURE);
	}
	gsi        = gslongname + 32;
	gsanswer   = gsi + 16/*sizeof(GS_INFO)*/;
	va_helpbuf = gsanswer + HW_PATH_MAX;
	Init_AV_Protocol();

	if (ignore_colours) /* monochrome ? */
	{
		/* set the scroller widget colors for monochrome */
		slider_bkg = G_WHITE;
		slider_col = G_WHITE;
	}

	/* identify the AES and issue appropriate initialization calls
	 */

	AES_type = identify_AES();

	/* If you know a documented length greater than 79 characters, add it. */
	if (AES_type == AES_nAES)
		aes_max_window_title_length = 127;

	/* BUG: This should fix single TOS but Geneva will probably still fail. */
	if (AES_type != AES_single)
		/* our name in the application menu */
		menu_register(gl_apid, "  HighWire HTML");

	(void)Pdomain(1);  /* we know about long file names */

#ifdef GEM_MENU
	if (!rsrc_load(_HIGHWIRE_RSC_)) {
		form_alert(1, _ERROR_NORSC_);
		exit(EXIT_FAILURE);
	}
	rsrc_gaddr(R_TREE, MENUTREE, &menutree);
	rsrc_gaddr(R_TREE, ABOUT, &about);
	rsrc_gaddr(R_TREE, RPOPUP, &rpopup);

	menu_bar(menutree, MENU_INSTALL);
#endif

	if (appl_xgetinfo(12, &info, &u, &u, &u) && (info & 8))
/*	if (appl_xgetinfo(AES_MESSAGE, &info, &u, &u, &u) && (info & 8))*/
		/* we know about AP_TERM message */
		shel_write(SWM_NEWMSG, 1, 0, NULL, NULL);

	/* grab the screen colors for later use */
	save_colors();

	if (AES_type == AES_single)
		init_logging();

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
			if (argv[i][0] != '-' && new_hwWind (argv[i], "", argv[i])) {
				u++;
			}
		} while (++i < argc);
	}
	if (!u && !new_hwWind ("", "", def_address)) {
		exit (EXIT_FAILURE);
	}
	set_mouse_watch (MO_ENTER, &hwWind_Top->Work);

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
			button_clicked (out.emo_mbutton, out.emo_mouse.p_x, out.emo_mouse.p_y);
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
