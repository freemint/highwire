#ifdef IMG_XMP

#include <ctype.h>

static BOOL decXmp_start (const char * file, IMGINFO info);
static BOOL decXmp_r_bit (IMGINFO, char * buffer);
static BOOL decXmp_r_pix (IMGINFO, char * buffer);
static void decXmp_quit  (IMGINFO);

static DECODER _decoder_xmp = {
	DECODER_CHAIN,
	decXmp_start
};
#undef  DECODER_CHAIN
#define DECODER_CHAIN &_decoder_xmp


#define TYPE_XBM   ((((((long)'X' <<8) | 'B') <<8) | 'M') <<8)
#define TYPE_XPM   ((((((long)'X' <<8) | 'P') <<8) | 'M') <<8)
typedef struct s_xmap {
	ULONG   Type;
	ULONG * PixChrs;
	WORD    NumChrs;
	WORD    Colors;
	WORD    W, H;
	char    Buffer[1024];
} * XMAP;

/*----------------------------------------------------------------------------*/
static char *
xmap_read (FILE * file, XMAP xmap, BOOL all)
{
	char * beg;
	while ((beg = fgets (xmap->Buffer,(int)sizeof(xmap->Buffer),file)) != NULL) {
		while (isspace (*beg)) beg++;
		if (*beg) {
			char * end;
			if (!all && (beg[0] == '/' && beg[1] == '*')
			         && (end = strstr (beg +2, "*/")) != NULL) {
				beg = end +2;
				while (isspace (*(++beg)));
				if (!*beg) continue;
			}
			end = strchr (beg, '\0');
			while (isspace (end[-1])) end--;
			*end = '\0';
			break;
		}
	}
	return beg;
}


/*----------------------------------------------------------------------------*/
static BOOL
decXmp_start (const char * name, IMGINFO info)
{
	WORD   depth  = 1;
	WORD   transp = -1;
	XMAP   xmap;
	char * p;
	FILE * file = fopen (name, "rb");
	if (!file) {
		return TRUE; /* avoid further tries of decoding */
	
	} else if ((xmap = malloc (sizeof(struct s_xmap))) == NULL) {
		puts ("decXmp_start(): low memory.");
		fclose (file);
		return FALSE;
	}
	
	xmap->Type    = 0ul;
	xmap->PixChrs = NULL;
	xmap->Colors  = 2;
	xmap->W       = 0;
	xmap->H       = 0;
	
	while ((p = xmap_read (file, xmap, !xmap->Type)) != NULL) {
		if (strncmp (p, "#define ", 8) == 0 
		    && (p = strrchr (p +8, '_')) != NULL) {
			p++;
			if (sscanf (p, "width  %hi", &xmap->W) == 1 && xmap->W > 0) continue;
			if (sscanf (p, "height %hi", &xmap->H) == 1 && xmap->H > 0) continue;
		
		} else if (p[0] == '/' && p[1] == '*') {
			if (strcmp (p +2, " XPM */") == 0) {
				xmap->Type = TYPE_XPM;
			}
			continue;
		
		} else if (strncmp (p, "static char ", 12) == 0) {
			if (!xmap->Type) {
				char * q = strrchr (p +12, '_');
				if (q && strcmp (q +1, "bits[] = {") == 0) {
					xmap->Type = TYPE_XBM;
				}
			} else { /* XPM */
				char * q = p +12;
				if (*q != '*' || (q = strchr (++q, '[')) == NULL
				              || strcmp (++q, "] = {") != 0) {
					xmap->Type = 0ul;
				}
			}
		}
		break;
	}
	if (xmap->Type == TYPE_XPM) {
		ULONG * pix = NULL, * map = NULL;
		if ((p = xmap_read (file, xmap, FALSE)) != NULL) {
			int i = sscanf (p, "\"%hi %hi %hi %hi \",",
			                &xmap->W, &xmap->H, &xmap->Colors, &xmap->NumChrs);
			if (i == 4 && xmap->W > 0 && xmap->H > 0
			    && xmap->Colors >= 2 && xmap->Colors <= 255
			    && xmap->NumChrs >= 1 && xmap->NumChrs <= 3) {
				pix = malloc (sizeof(ULONG) *256 *2);
				map = pix +256;
			}
			if (pix) {
				char form[] = "\"%0[^\t] %1[cs] %19[^\"]\",", mod[2], val[20];
				form[2] += xmap->NumChrs;
				memset (pix, 0, (char*)map - (char*)pix);
				for (i = 0; i < xmap->Colors; i++) {
					BOOL ok = TRUE;
					if ((p = xmap_read (file, xmap, FALSE)) == NULL ||
					    sscanf (p, form, (char*)&pix[i], mod, val) != 3) {
						ok = FALSE;
					} else if (val[0] == '#' && isxdigit (val[1])) {
						map[i] = strtoul (val +1, NULL, 16);
					} else if (strnicmp ("None", val, 4) == 0) {
						map[i] = 0ul; /* transparency value */
						transp = i;
					} else {
						ok = FALSE;
					}
					if (!ok) {
						free (pix);
						pix = NULL;
						break;
					}
				}
			}
		}
		
		if (pix) {
			depth         = 8;
			xmap->PixChrs = pix;
			info->Palette = (char*)map;
			info->PalRpos = 1;
			info->PalGpos = 2;
			info->PalBpos = 3;
			info->PalStep = 4;
		} else {
			xmap->Type = 0ul;
		}
	}
	if (!xmap->Type) {
		free (xmap);
		fclose (file);
		return FALSE;
	}
	
	info->_priv_data = xmap;
	info->_priv_file = file;
	info->read       = (xmap->Type == TYPE_XBM ? decXmp_r_bit : decXmp_r_pix);
	info->quit       = decXmp_quit;
	
	info->ImgWidth   = xmap->W;
	info->ImgHeight  = xmap->H;
	info->NumComps   = 1;
	info->BitDepth   = depth;
	info->NumColors  = xmap->Colors;
	info->Transp     = transp;
	info->Interlace  = 0;
	
	xmap->Buffer[0]  = '\0';
	
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static BOOL
decXmp_r_bit (IMGINFO info, char * buffer)
{
	XMAP xmap = info->_priv_data;
	if (xmap->H-- >= 0) {
		UWORD  mask = 0;
		UWORD  val  = -1;
		WORD   x    = xmap->W;
		char * p    = xmap->Buffer;
		while (x--) {
			int n;
			if (!mask) {
				int v1, v2;
				if ((!*p && (p = xmap_read (info->_priv_file, xmap, FALSE)) == NULL)
				    || sscanf (p, "0x%x,%n", &v1, &n) != 1) {
					xmap->H = -1;
					break;
				}
				p += n;
				while (isspace (*p)) p++;
				if (x >= 8) {
					if ((!*p &&
					     (p = xmap_read (info->_priv_file, xmap, FALSE)) == NULL)
					    || sscanf (p, "0x%x,%n", &v2, &n) != 1) {
						xmap->H = -1;
						break;
					}
					p += n;
					while (isspace (*p)) p++;
				} else {
					v2 = 0;
				}
				val = (v2 <<8) | v1;
				mask = 0x0001;
			}
			*(buffer++) = (val & mask ? 1 : 0);
			mask      <<= 1;
		}
		val = 0;
		while ((xmap->Buffer[val++] = *(p++)) != '\0');
	}
	return (xmap->H >= 0);
}

/*----------------------------------------------------------------------------*/
static BOOL
decXmp_r_pix (IMGINFO info, char * buffer)
{
	XMAP xmap = info->_priv_data;
	if (xmap->H-- >= 0) {
		WORD   x = xmap->W;
		char * p = xmap_read (info->_priv_file, xmap, FALSE);
		if (!p || *p != '"') {
			xmap->H = -1;
			x       = 0;
		}
		while (x--) {
			WORD   num = xmap->NumChrs;
			ULONG  pix = 0uL;
			char * chr = (char*)&pix;
			while (num-- && (*(chr++) = *(++p)) != '\0');
			num = xmap->Colors;
			while (num-- && pix != xmap->PixChrs[num]);
			if (num < 0) {
				xmap->H = -1;
				break;
			}
			*(buffer++) = num;
		}
		if (!*p || *(++p) != '"') {
			xmap->H = -1;
		}
	}
	return (xmap->H >= 0);
}

/*----------------------------------------------------------------------------*/
static void
decXmp_quit (IMGINFO info)
{
	XMAP xmap = info->_priv_data;
	if (xmap) {
		if (xmap->PixChrs) free (xmap->PixChrs);
		free (xmap);
		fclose (info->_priv_file);
		info->_priv_file = NULL;
		info->_priv_data = NULL;
	}
}

#endif /*IMG_XMP*/
