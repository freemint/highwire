#ifndef __FONTBASE_H__
#define __FONTBASE_H__


#define FNT_BOLD   0x0001 /* TXT_THICKENED */
#define FNT_ITALIC 0x0004 /* TXT_SKEWED    */
#define FNT_CONDNS 0x1000

typedef struct s_fontbase * FONTBASE;

struct s_fontbase {
	FONTBASE Next;
	FONT     List;
	WORD     Id;
	UWORD    Style;   /* FNT_BOLD|FNT_ITALIC */
	WORD     RealId;
	UWORD    Effects; /* FNT_BOLD|FNT_ITALIC */
	WORD     Mapping; /* BITSTREAM / ATARI / UNICODE */
	WCHAR    SpaceCode;
	WCHAR    NobrkCode;
};

struct s_font {
	FONTBASE Base;
	FONT     Next;
	WORD     Points;
	WORD     SpaceWidth;
	WORD     Ascend;
	WORD     Descend;
	LONG     Condensed;
};

FONT font_byType (WORD type, WORD style, WORD points, WORDITEM);
			/* Selects the appropriate font for FONTTYPE (= normal,header,pre) and
			 * 'style' (= bitmask of bold|italic|condensed) and stores it in the
			 * WIRDITEM.  If 'type', 'style' or 'points' are -1 this values will
			 * be extracted from the WORDITEM values.
			*/

#define font_Space(f)   f->Base->SpaceCode
#define font_Nobrk(f)   f->Base->NobrkCode

BOOL font_switch (FONT actual, FONT previous);
			/* Set the text attributes stored in 'actual'.  If 'previous' is not
			 * NULL only different values are set for speed reasons.
			 * If the function result is TRUE an additional call of vst_effects()
			 * with the fontbase->Effects attribute is necessary.
			*/

WORD font_step2size (WORD step);
			/* step >= 0: maps the font step of [0..7] to a font size in points
			 *            relative to the global font_size variable.
			 * step < 0:  interpretes step as a font size and returns the best
			 *            matching font step of [0..7].
			*/

FNTSTACK fontstack_setup (FNTSBASE *, WORD color);
FNTSTACK fontstack_clear (FNTSBASE *);
FNTSTACK fontstack_push     (TEXTBUFF, WORD step);
FNTSTACK fontstack_pop      (TEXTBUFF);
void     fontstack_setSize  (TEXTBUFF, WORD size);
#define                  _fstk_val(tb,f,s,v) {word_set_##f(tb,(tb)->font->s=v);}
#define                  _fstk_set(tb,f,s)  _fstk_val(tb,f,set##s,TRUE)
#define  fontstack_setType(  textbuff, t)   _fstk_val(textbuff,font,Type,t)
#define  fontstack_setColor( textbuff, c)   _fstk_val(textbuff,color,Color,c)
#define  fontstack_setBold(  textbuff)      _fstk_set(textbuff,bold,Bold)
#define  fontstack_setItalic(textbuff)      _fstk_set(textbuff,italic,Italic)
#define  fontstack_setUndrln(textbuff)      _fstk_set(textbuff,underline,Undrln)
#define  fontstack_setStrike(textbuff)      _fstk_set(textbuff,strike,Strike)
#define  fontstack_setNoWrap(textb)   {textb->nowrap+=textb->font->setNoWrap=1;}


#endif /*__FONTBASE_H__*/
