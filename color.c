/* @(#)highwire/color.c
 */
#include <stddef.h> /* for size_t */
#include <gem.h>

#include "global.h"


typedef struct {
	CHAR satur;
	CHAR red, green, blue;
} SRGB;
static SRGB screen_colortab[256]; /* used to save colors */


/*----------------------------------------------------------------------------*/
static CHAR
saturation (CHAR * rgb)
{
	CHAR satr;
	#define rgb_red   rgb[0]
	#define rgb_green rgb[1]
	#define rgb_blue  rgb[2]
	
	if (rgb_red >= rgb_green) {
		if (rgb_green >= rgb_blue)   satr = rgb_red   - rgb_blue;
		else if (rgb_red > rgb_blue) satr = rgb_red   - rgb_green;
		else                         satr = rgb_blue  - rgb_green;
	} else if (rgb_green >= rgb_blue) {
		if (rgb_red >= rgb_blue)     satr = rgb_green - rgb_blue;
		else                         satr = rgb_green - rgb_red;
	} else {
		                             satr = rgb_blue  - rgb_red;
	}
	#undef rgb_red
	#undef rgb_green
	#undef rgb_blue
	
	return satr;
}


/*==============================================================================
 * Saves colors in global array screen_colortab[]
 *
 * Really for us this should be loading the standard palette
 * I'm just grabbing whatever the system has set
 */
void
save_colors(void)
{
	WORD  res_colors = (planes >= 8 ? 256 : 1 << planes);
	int i;

	for (i = 0; i < res_colors; i++) {
		WORD coltab[3];
		vq_color (vdi_handle, i, 1, coltab);

		screen_colortab[i].red   = ((long)coltab[0] *255 +500) /1000;
		screen_colortab[i].green = ((long)coltab[1] *255 +500) /1000;
		screen_colortab[i].blue  = ((long)coltab[2] *255 +500) /1000;
		screen_colortab[i].satur = saturation (&screen_colortab[i].red);
	}
}


/*============================================================================*/
WORD
remap_color (long value)
{
	short red   = ((char*)&value)[1];
	short green = ((char*)&value)[2];
	short blue  = ((char*)&value)[3];
	short satur = saturation (((CHAR*)&value) +1);
	
	WORD  best_fit  = 0;
	UWORD best_err  = 0xFFFFu;
	WORD  max_color = (planes >= 8 ? 256 : 1 << planes);
	
	WORD i = 0;
	
	value = (value & 0x00FFFFFFl) | ((long)satur <<24);
	
	do {
		if (*(long*)&screen_colortab[i] == value) {
			/* gotcha! */
			best_fit = i;
			break;
		} else {
			UWORD err = (red   > screen_colortab[i].red ?
			             red   - screen_colortab[i].red :
			                     screen_colortab[i].red - red)
			          + (green > screen_colortab[i].green ?
			             green - screen_colortab[i].green :
			                     screen_colortab[i].green - green)
			          + (blue  > screen_colortab[i].blue ?
			             blue  - screen_colortab[i].blue :
			                     screen_colortab[i].blue - blue)
			          + (satur > screen_colortab[i].satur ?
			             satur - screen_colortab[i].satur :
			                     screen_colortab[i].satur - satur);
			if (err <= best_err) {
				best_err = err;
				best_fit = i;
			}
		}
	} while (++i < max_color);
	
	return best_fit;
}


/*==============================================================================
 * Returns the the VDI color of 'rgb' as a iiRRGGBB value.  If 'trans' is NULL
 * ii is the VDI palette index of RRGGBB, else it contains the ii-th element of
 * the array 'trans'.
 * The argument 'rgb' has normally a value of 00RRGGBB;  if it is 0xFFFFFFnn,
 * the nn part will be taken directly as a VDI palette index and no color match
 * search will be performed.
 */
ULONG
color_lookup (ULONG rgb, WORD * trans)
{
	CHAR idx = ((rgb & ~0xFFuL) == ~0xFFuL ? rgb : remap_color (rgb));
	return ( ((long)(trans ? trans[idx] : idx) <<24)
	       | (*(long*)&screen_colortab[idx] & 0x00FFFFFFuL) );
}
