/* @(#)highwire/Variable.c
 */
#include <stddef.h> /* for size_t */

#include "global.h"


/* ******** Variable Definitions ******************* */

/* To be in-program settings.  Default for the user settings in highwire.cfg. */
WORD fonts[3][2][2] = {
	{ {5031, 5032}, {5033, 5034} },
	{ {5031, 5032}, {5033, 5034} },
	{ {5031, 5032}, {5033, 5034} }
};

/* ok there are 3 font families and these have 2 different effects

   What I know so far...

   [font family][bold face][italic face]

   font families
     0 - normal font
     1 - header font
     2 - pre font

    So you can have
   	header , bold, italic
   	header , bold, normal
   	header , normal, italic
   	...
*/
WORD font_size = 12;  /* points */

WORD link_colour = 12;  /* G_LBLUE */
WORD highlighted_link_colour = 10;  /* G_LRED */
WORD text_colour = 1;  /* G_BLACK */
WORD background_colour = 0;  /* G_WHITE, changed to G_LWHITE for � 2 colors */

WORD slider_bkg = 9;  /* G_LBLACK */
WORD slider_col = 8;  /* G_LWHITE */

WORD page_margin = 5; 

BOOL alternative_text_is_on = FALSE;


/* Globals */
WORD aes_max_window_title_length = 79;  /* default for original AES */
