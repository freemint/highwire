/* @(#)highwire/token.h
 */


#if defined (__TAG_ITEM)
# undef __TOKEN_TAG
# define __BEG_TAGS
# define __END_TAGS

#elif !defined (__TOKEN_TAG)
# define __BEG_TAGS   typedef enum { TAG_Unknown = 0,
# define __TAG_ITEM(t)   TAG_##t
# define __END_TAGS   , TAG_LastDefined } HTMLTAG;
#endif

#ifndef __TOKEN_TAG
#define __TOKEN_TAG
	__BEG_TAGS
		__TAG_ITEM (A),
		__TAG_ITEM (B),
		__TAG_ITEM (BASE),
		__TAG_ITEM (BASEFONT),
		__TAG_ITEM (BGSOUND),
		__TAG_ITEM (BIG),
		__TAG_ITEM (BLOCKQUOTE),
		__TAG_ITEM (BODY),
		__TAG_ITEM (BR),
		__TAG_ITEM (C),
		__TAG_ITEM (CENTER),
		__TAG_ITEM (CITE),
		__TAG_ITEM (CODE),
		__TAG_ITEM (DD),
		__TAG_ITEM (DEL),
		__TAG_ITEM (DFN),
		__TAG_ITEM (DIR),
		__TAG_ITEM (DIV),
		__TAG_ITEM (DL),
		__TAG_ITEM (DT),
		__TAG_ITEM (EM),
		__TAG_ITEM (EMBED),
		__TAG_ITEM (FONT),
#		undef       FORM
		__TAG_ITEM (FORM),
		__TAG_ITEM (FRAME),
		__TAG_ITEM (FRAMESET),
		__TAG_ITEM (H),   /* for <H1>..<H6> */
		__TAG_ITEM (HR),
		__TAG_ITEM (HTML),
		__TAG_ITEM (I),
		__TAG_ITEM (IMG),
#		undef       INPUT
		__TAG_ITEM (INPUT),
		__TAG_ITEM (INS),
		__TAG_ITEM (IOD),
		__TAG_ITEM (KBD),
		__TAG_ITEM (LI),
		__TAG_ITEM (LISTING),
		__TAG_ITEM (MENU),
		__TAG_ITEM (META),
		__TAG_ITEM (NOBR),
		__TAG_ITEM (NOSCRIPT),
		__TAG_ITEM (OL),
		__TAG_ITEM (OPTGROUP),
		__TAG_ITEM (OPTION),
		__TAG_ITEM (P),
		__TAG_ITEM (PLAINTEXT),
		__TAG_ITEM (PRE),
		__TAG_ITEM (Q),
		__TAG_ITEM (S),
		__TAG_ITEM (SAMP),
		__TAG_ITEM (SCRIPT),
		__TAG_ITEM (SELECT),
		__TAG_ITEM (SMALL),
		__TAG_ITEM (SPAN),
		__TAG_ITEM (STRIKE),
		__TAG_ITEM (STRONG),
		__TAG_ITEM (STYLE),
		__TAG_ITEM (SUB),
		__TAG_ITEM (SUP),
		__TAG_ITEM (TABLE),
		__TAG_ITEM (TD),
		__TAG_ITEM (TEXTAREA),
		__TAG_ITEM (TH),
		__TAG_ITEM (TITLE),
		__TAG_ITEM (TR),
		__TAG_ITEM (TT),
		__TAG_ITEM (U),
		__TAG_ITEM (UL),
		__TAG_ITEM (VAR),
		__TAG_ITEM (WBR),
		__TAG_ITEM (XMP)
	__END_TAGS

	#ifndef NumberofTAG
	# define NumberofTAG (TAG_LastDefined - TAG_Unknown -1)
	#endif
	
	#undef __BEG_TAGS
	#undef __TAG_ITEM
	#undef __END_TAGS
#endif /* __TOKEN_TAG */


#if defined (__KEY_ITEM)
# undef __TOKEN_KEY
# define __BEG_KEYS
# define __END_KEYS

#elif !defined (__TOKEN_KEY)
# define __BEG_KEYS   typedef enum { KEY_Unknown = 0, KEY_H_HEIGHT,
# define __KEY_ITEM(k)   KEY_##k
# define __END_KEYS   , KEY_LastDefined } HTMLKEY;
#endif

#ifndef __TOKEN_KEY
#define __TOKEN_KEY
	__BEG_KEYS
		__KEY_ITEM (ACTION),       /* FORM */
		__KEY_ITEM (ALIGN),        /* DIV,H#,HR,P,IMG,TABLE,TD,TH,TR */
		__KEY_ITEM (ALT),          /* IMG */
		__KEY_ITEM (BGCOLOR),      /* BODY,TABLE,TD,TH,TR */
		__KEY_ITEM (BORDER),       /* TABLE, FRAMESET   -- FRAMESET = NS propr. */
		__KEY_ITEM (BORDERCOLOR),  /* FRAMESET */
		__KEY_ITEM (CELLPADDING),  /* TABLE */
		__KEY_ITEM (CELLSPACING),  /* TABLE */
		__KEY_ITEM (CHARSET),      /* A */
		__KEY_ITEM (CHECKED),      /* INPUT */
		__KEY_ITEM (CLEAR),        /* BR */
		__KEY_ITEM (COLOR),        /* BASEFONT,FONT,HR,P */
		__KEY_ITEM (COLS),         /* FRAMESET */
		__KEY_ITEM (COLSPAN),      /* TD,TH */
		__KEY_ITEM (CONTENT),      /* META */
		__KEY_ITEM (DISABLED),     /* INPUT,SELECT,OPTION */
		__KEY_ITEM (FRAMEBORDER),  /* FRAMESET   -- IE proprietary */
		__KEY_ITEM (FRAMESPACING), /* FRAMESET   -- IE proprietary */
		__KEY_ITEM (HEIGHT),       /* HR,IMG,TABLE,TD,TH,TR */
		__KEY_ITEM (HREF),         /* A */
		__KEY_ITEM (HSPACE),       /* IMG */
		__KEY_ITEM (HTTP_EQUIV),   /* META */
		__KEY_ITEM (ID),           /* A */
		__KEY_ITEM (LABEL),        /* OPTGROUP */
		__KEY_ITEM (LANG),         /* HTML,Q */
		__KEY_ITEM (LEFTMARGIN),   /* BODY   -- IE proprietary */
		__KEY_ITEM (LINK),         /* BODY */
		__KEY_ITEM (MARGINHEIGHT), /* BODY,FRAME   -- BODY = NS proprietary */
		__KEY_ITEM (MARGINWIDTH),  /* BODY,FRAME   -- BODY = NS proprietary */
		__KEY_ITEM (MAXLENGTH),    /* INPUT */
		__KEY_ITEM (METHOD),       /* FORM */
		__KEY_ITEM (NAME),         /* A,FRAME */
		__KEY_ITEM (NOWRAP),       /* TD,TH */
		__KEY_ITEM (NORESIZE),     /* FRAME */
		__KEY_ITEM (NOSHADE),      /* HR */
		__KEY_ITEM (ROWS),         /* FRAMESET */
		__KEY_ITEM (ROWSPAN),      /* TD,TH */
		__KEY_ITEM (SCROLLING),    /* FRAME */
		__KEY_ITEM (SELECTED),     /* OPTION */
		__KEY_ITEM (SHOUT),        /* IOD   -- IOD4 proprietary */
		__KEY_ITEM (SIZE),         /* BASEFONT,FONT,HR */
		__KEY_ITEM (SRC),          /* EMBED,FRAE,IMG  -- EMBED = NS proprietary */
		__KEY_ITEM (START),        /* OL */
		__KEY_ITEM (STYLE),        /* all, internal mapped */
		__KEY_ITEM (TARGET),       /* A,BASE,FORM */
		__KEY_ITEM (TEXT),         /* BODY */
		__KEY_ITEM (TITLE),        /* BLOCKQUOTE */
		__KEY_ITEM (TOPMARGIN),    /* BODY   -- IE proprietary */
		__KEY_ITEM (TYPE),         /* LI,OL,UL */
		__KEY_ITEM (VALIGN),       /* TD,TH,TR */
		__KEY_ITEM (VALUE),        /* LI */
		__KEY_ITEM (VSPACE),       /* IMG */
		__KEY_ITEM (WIDTH)         /* HR,IMG,PRE,TABLE,TD,TH */
	__END_KEYS
	
	#ifndef NumberofKEY
	# define NumberofKEY (KEY_LastDefined - KEY_Unknown -2)
	#endif

	#undef __BEG_KEYS
	#undef __KEY_ITEM
	#undef __END_KEYS
#endif /* __TOKEN_KEY */


#if defined (__CSS_ITEM)
# undef __TOKEN_CSS
# define __BEG_CSSS
# define __END_CSSS

#elif !defined (__TOKEN_CSS)
# define __BEG_CSSS   typedef enum { CSS_Unknown = KEY_LastDefined,
# define __CSS_ITEM(k,e)   CSS_##k
# define __END_CSSS   , CSS_LastDefined } HTMLCSS;
#endif

#ifndef __TOKEN_CSS
#define __TOKEN_CSS
	__BEG_CSSS
		__CSS_ITEM (BACKGROUND_COLOR, KEY_BGCOLOR),
		/*          BORDER_COLOR      */
		__CSS_ITEM (BORDER_SPACING,   KEY_CELLSPACING),
		/*          BORDER_STYLE      */
		__CSS_ITEM (BORDER_WIDTH,     KEY_BORDER),
		/*          CLEAR             */
		__CSS_ITEM (COLOR,            KEY_COLOR),
		/*          EMPTY_CELLS       */
		/*          FLOAT             */
		__CSS_ITEM (FONT_FAMILY,      0),
		__CSS_ITEM (FONT_SIZE,        0),
		/*          FONT_STRETCH      */
		__CSS_ITEM (FONT_STYLE,       0),
		__CSS_ITEM (FONT_WEIGHT,      0),
		__CSS_ITEM (HEIGHT,           KEY_HEIGHT),
		/*          LINE_HEIGHT       */
		__CSS_ITEM (LIST_STYLE_TYPE,  0),
		/*          MIN_HEIGHT        */
		/*          MIN_WIDTH         */
		__CSS_ITEM (MARGIN,           0),
		/*          MAX_HEIGHT        */
		/*          MAX_WIDTH         */
		__CSS_ITEM (PADDING,          KEY_CELLPADDING),
		__CSS_ITEM (TEXT_ALIGN,       KEY_ALIGN),
		__CSS_ITEM (TEXT_DECORATION,  0),
		__CSS_ITEM (VERTICAL_ALIGN,   KEY_VALIGN),
		__CSS_ITEM (WIDTH,            KEY_WIDTH)
	__END_CSSS
	
	#ifndef NumberofCSS
	# define NumberofCSS (CSS_LastDefined - CSS_Unknown -1)
	#endif
	
	#undef __BEG_CSSS
	#undef __CSS_ITEM
	#undef __END_CSSS
#endif /* __TOKEN_CSS */
