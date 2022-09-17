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

/* tables predefined for fixed color map enabled, otherwise overwritten by the
 * values of the color map at start time
*/
static CHAR color_Cube216[216] = {
	G_BLACK, 16, 17, 18, 19, 20,  21, 22, 23, 24, 25, 26,
	     27, 28, 29, 30, 31, 32,  33, 34, 35, 36, 37, 38,
	     39, 40, 41, 42, 43, 44,  45, 46, 47, 48, 49, 50,
	/**/ 51, 52, 53, 54, 55, 56,  57, 58, 59, 60, 61, 62,     
	     63, 64, 65, 66, 67, 68,  69, 70, 71, 72, 73, 74,
	     75, 76, 77, 78, 79, 80,  81, 82, 83, 84, 85, 86,
	/**/ 87, 88, 89, 90, 91, 92,  93, 94, 95, 96, 97, 98,
	     99,100,101,102,103,104, 105,106,107,108,109,110,
	    111,112,113,114,115,116, 117,118,119,120,121,122,
	/**/123,124,125,126,127,128, 129,130,131,132,133,134,
	    135,136,137,138,139,140, 141,142,143,144,145,146,
	    147,148,149,150,151,152, 153,154,155,156,157,158,
	/**/159,160,161,162,163,164, 165,166,167,168,169,170,
	    171,172,173,174,175,176, 177,178,179,180,181,182,
	    183,184,185,186,187,188, 189,190,191,192,193,194,
	/**/195,196,197,198,199,200, 201,202,203,204,205,206,
	    207,208,209,210,211,212, 213,214,215,216,217,218,
	    219,220,221,222,223,224, 225,226,227,228,229,G_WHITE
};
static CHAR color_GrayMap[32] = {
	G_BLACK,244,243,255,245,236, 58,246, 242,254,247,249,101,241,235,240,
	    239,248,238,144,234,237,253,252, 233,187,232,231,251,250,230,G_WHITE
};

BOOL color_FixedMap = FALSE;


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


/*============================================================================*/
void
color_mapsetup(void)
{
	WORD coltab[3];

	/* setup the rgb 6*6*6=216 color cube */
	WORD i = 15;
	for (coltab[0] = 0; coltab[0] <= 1000; coltab[0] += 200) {
		for (coltab[1] = 0; coltab[1] <= 1000; coltab[1] += 200) {
			for (coltab[2] = 0; coltab[2] <= 1000; coltab[2] += 200) {
				if (coltab[0] != coltab[1] || coltab[1] != coltab[2]) {
					vs_color (vdi_handle, i, coltab);
				}
				i++;
			}
		}
	}
	
	/* setup 32 gray values */
	for (i = 0; i < numberof(color_GrayMap); i++) {
		LONG gray = (i <<3) | (i >>2); /* 000abcde -> abcdeabc */
		coltab[0] = coltab[1] = coltab[2] = (gray *1000 +127) /255;
		vs_color (vdi_handle, color_GrayMap[i], coltab);
	}
	
	if (planes == 8) {
		WORD mbuf[8] = { COLORS_CHANGED, 0, 0,0,0,0,0,0 };
		mbuf[1] = gl_apid;
		shel_write (SWM_BROADCAST, 0, 0, (void*)mbuf, NULL);
	}
}


/*==============================================================================
 * Saves colors in global array screen_colortab[]
*/
void
save_colors(void)
{
	WORD res_colors = (planes >= 8 ? 256 : 1 << planes);
	int  i;
	
	static BOOL _once = TRUE;
	if (!_once) return;
	_once = FALSE;
	
	if ((cfg_FixedCmap && planes == 8) || planes > 8) {
		color_mapsetup();
		color_FixedMap = TRUE;
	}
	/* else:  tables will be setup on request by color_tables() */
	
	for (i = 0; i < res_colors; i++) {
		WORD coltab[3];
		vq_color (vdi_handle, i, 1, coltab);
		screen_colortab[i].red   = ((long)coltab[0] *255 +500) /1000;
		screen_colortab[i].green = ((long)coltab[1] *255 +500) /1000;
		screen_colortab[i].blue  = ((long)coltab[2] *255 +500) /1000;
		screen_colortab[i].satur = saturation (&screen_colortab[i].red);
	}
/*	if (TRUE) {
		FILE * f = fopen ("pal.txt", "w");
		if ((i & 7) == 0) fprintf (f, "%02X:", i);
		fprintf (f, " %02X:%02X:%02X", screen_colortab[i].red,
		                       screen_colortab[i].green, screen_colortab[i].blue);
		if ((i & 7) == 7) fprintf (f, "\n");
		fclose (f);
	}*/
}


/*============================================================================*/
WORD
remap_color (long value)
{
	short red   = ((char*)&value)[1];
	short green = ((char*)&value)[2];
	short blue  = ((char*)&value)[3];
	short satur = saturation (((CHAR*)&value) +1);
	
	WORD best_fit = 0;
	
	if (color_FixedMap) {
		WORD r = (red   *5 +127) /255;
		WORD g = (green *5 +127) /255;
		WORD b = (blue  *5 +127) /255;
		best_fit = (r == g && g == b ? color_GrayMap[(red + green + blue) /24]
		                             : color_Cube216[(r *6 + g) *6 +b]);
	
	} else {
		UWORD best_err  = 0xFFFFu;
		WORD  max_color = (planes >= 8 ? 256 : 1 << planes);
		WORD  i         = 0;
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
	}
	return best_fit;
}


/*==============================================================================
 * Returns the stored VDI screen color for 'idx' as a 00RRGGBB value.
*/
ULONG
color_lookup (WORD idx)
{
	return *(long*)&screen_colortab[idx] & 0x00FFFFFFuL;
}


/*============================================================================*/
void
color_tables (ULONG cube[216], ULONG gray[32], WORD pixel_val[256])
{
	WORD i;
	
	if (!color_FixedMap) {
		CHAR * dst;
		ULONG  r, g, b;
		/* setup the rgb 6*6*6=216 color cube */
		dst = color_Cube216;
		for (r = 0x000000uL; r <= 0xFF0000uL; r += 0x330000uL) {
			for (g = 0x000000uL; g <= 0x00FF00uL; g += 0x003300uL) {
				for (b = 0x000000uL; b <= 0x0000FFuL; b += 0x000033uL) {
					*(dst++) = remap_color (r | g | b);
				}
			}
		}
		/* setup 32 gray values */
		dst = color_GrayMap;
		for (g = 0x000000uL; g <= 0xF8F8F8uL; g += 0x080808uL) {
			*(dst++) = remap_color (g | ((g >>5) & 0x030303uL));
		}
	}
	
	/* copy the rgb 6*6*6=216 color cube */
	for (i = 0; i < numberof(color_Cube216); i++) {
		WORD  idx = color_Cube216[i];
		ULONG rgb = *(long*)&screen_colortab[idx];
		cube[i]   = ((long)pixel_val[idx] <<24) | (rgb & 0x00FFFFFFuL);
	}
	
	/* copy 32 gray values */
	for (i = 0; i < numberof(color_GrayMap); i++) {
		WORD idx = color_GrayMap[i];
		ULONG rgb = *(long*)&screen_colortab[idx];
		gray[i]   = ((long)pixel_val[idx] <<24) | (rgb & 0x00FFFFFFuL);
	}
}
