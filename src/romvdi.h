/*
 * romvdi.h
 *
 * Compatibility layer letting HighWire run on the plain ROM VDI, ie. on an ST
 * or Mega ST(E) with no SpeedoGDOS, NVDI or fVDI installed.
 *
 * HighWire was written against a Speedo/FSM capable GDOS and uses a number of
 * VDI calls that only exist there: scalable point sizes, 16-bit (Unicode) text
 * output, character mapping modes and extended font enquiry.  On the ROM VDI
 * those calls are simply absent - and worse than absent, vqt_xfntinfo() failing
 * used to leave font_base() returning NULL for every id including the system
 * font, which then got dereferenced and bombed.
 *
 * Every extended call HighWire makes is wrapped here.  With a capable GDOS the
 * wrapper is a straight pass-through and behaviour is unchanged.  Without one
 * it falls back to something GEM 1.0 understands:
 *
 *   vqt_xfntinfo    -> synthesised (Atari mapped bitmap font)
 *   vst_arbpt       -> vst_point
 *   vst_setsize32   -> ignored (no condensed text)
 *   vst_map_mode    -> remembered, so text conversion knows what it is holding
 *   vqt_advance     -> vqt_width
 *   v_ftext         -> v_gtext
 *   v_ftext16/16n   -> converted to 8-bit, then v_gtext
 *   vqt_f_extent16n -> converted to 8-bit, then vqt_extent
 *   vst_scratch/vst_kern/vst_(un)load_fonts -> ignored
 *   vq_scrninfo     -> synthesised (interleaved bitplanes, ST palette order)
 *
 * The result is single-size, single-face text: the ROM carries only the fixed
 * system font, so there is no bold, no italic and no scaling.  That is a large
 * step down from SpeedoGDOS, but it is what the machine can do, and it is the
 * same output HighWire already produces when a GDOS is present but has no
 * fonts loaded.
 */
#ifndef __ROMVDI_H__
#define __ROMVDI_H__

#include "fontbase.h"

/* TRUE when a Speedo/FSM capable GDOS is present (SpeedoGDOS, NVDI >= 3 or
 * fVDI).  FALSE means we are on the plain ROM VDI and running in fallback.
 */
extern BOOL has_fsm_gdos;

/* Call once, straight after the workstation is open, with the result of
 * vq_vgdos().  Decides which mode we are in for the rest of the session.
 */
void romvdi_init (long gdostype);

WORD hw_vst_font        (WORD handle, FONTBASE base);
WORD hw_vqt_xfntinfo    (WORD handle, WORD flags, WORD id, WORD fnt_index,
                         XFNT_INFO * info);
void hw_vst_map_mode    (WORD handle, WORD mode);
WORD hw_vst_arbpt       (WORD handle, WORD point,
                         WORD * wc, WORD * hc, WORD * wb, WORD * hb);
void hw_vst_setsize32   (WORD handle, LONG point,
                         WORD * wc, WORD * hc, WORD * wb, WORD * hb);
void hw_vqt_advance     (WORD handle, WORD ch,
                         WORD * advx, WORD * advy, WORD * remx, WORD * remy);
void hw_v_ftext         (WORD handle, WORD x, WORD y, const char * str);
void hw_v_ftext16       (WORD handle, WORD x, WORD y, const WCHAR * str);
void hw_v_ftext16n      (WORD handle, WORD x, WORD y, const WCHAR * str,
                         WORD num);
void hw_vqt_f_extent16n (WORD handle, const WCHAR * str, WORD num,
                         WORD * extent);
void hw_vst_scratch     (WORD handle, WORD mode);
void hw_vst_kern        (WORD handle, WORD tmode, WORD pmode,
                         WORD * tracks, WORD * pairs);
void hw_vst_load_fonts  (WORD handle, WORD sel);
void hw_vst_unload_fonts(WORD handle, WORD sel);
void hw_vq_scrninfo     (WORD handle, WORD * out);

#endif /* __ROMVDI_H__ */
