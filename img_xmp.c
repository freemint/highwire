#ifdef IMG_XMP

#include <ctype.h>

static BOOL decXmp_start (const char * file, IMGINFO info);
static BOOL decXmp_read  (IMGINFO, char * buffer);
static void decXmp_quit  (IMGINFO);

static DECODER _decoder_xmp = {
	DECODER_CHAIN,
	decXmp_start
};
#undef  DECODER_CHAIN
#define DECODER_CHAIN &_decoder_xmp


typedef struct s_xmap {
	WORD   W, H;
	char   Buffer[1024];
} * XMAP;

static char * xmap_read (FILE *, XMAP);


/*----------------------------------------------------------------------------*/
static BOOL
decXmp_start (const char * name, IMGINFO info)
{
	XMAP   xmap;
	char * p;
	BOOL   match = FALSE;
	FILE * file  = fopen (name, "rb");
	if (!file) {
		return TRUE; /* avoid further tries of decoding */
	
	} else if ((xmap = malloc (sizeof(struct s_xmap))) == NULL) {
		puts ("decXmp_start(): low memory.");
		fclose (file);
		return FALSE;
	}
	
	xmap->W = 0;
	xmap->H = 0;
	
	while ((p = xmap_read (file, xmap)) != NULL) {
		if (strncmp (p, "#define ", 8) == 0 
		    && (p = strrchr (p +8, '_')) != NULL) {
			p++;
			if (sscanf (p, "width  %hi", &xmap->W) == 1 && xmap->W > 0) continue;
			if (sscanf (p, "height %hi", &xmap->H) == 1 && xmap->H > 0) continue;
		
		} else if (strncmp (p, "static char ", 12) == 0) {
			char * q = strrchr (p +12, '_');
			if (q && strcmp (q +1, "bits[] = {") == 0) {
				match = TRUE;
			}
		}
		break;
	}
	if (!match) {
		free (xmap);
		fclose (file);
		return FALSE;
	}
	
	info->_priv_data = xmap;
	info->_priv_file = file;
	info->read       = decXmp_read;
	info->quit       = decXmp_quit;
	
	info->ImgWidth   = xmap->W;
	info->ImgHeight  = xmap->H;
	info->NumComps   = 1;
	info->BitDepth   = 1;
	info->NumColors  = 2;
	info->Transp     = -1;
	info->Interlace  = 0;
	
	xmap->Buffer[0]  = '\0';
	
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static BOOL
decXmp_read (IMGINFO info, char * buffer)
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
				if ((!*p && (p = xmap_read (info->_priv_file, xmap)) == NULL)
				    || sscanf (p, "0x%x,%n", &v1, &n) != 1) {
					xmap->H = -1;
					break;
				}
				p += n;
				while (isspace (*p)) p++;
				if (x >= 8) {
					if ((!*p && (p = xmap_read (info->_priv_file, xmap)) == NULL)
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
static void
decXmp_quit (IMGINFO info)
{
	XMAP xmap = info->_priv_data;
	if (xmap) {
		free (xmap);
		fclose (info->_priv_file);
		info->_priv_file = NULL;
		info->_priv_data = NULL;
	}
}


/******************************************************************************/


/*----------------------------------------------------------------------------*/
static char *
xmap_read (FILE * file, XMAP xmap)
{
	char * beg;
	while ((beg = fgets (xmap->Buffer,(int)sizeof(xmap->Buffer),file)) != NULL) {
		while (isspace (*beg)) beg++;
		if (*beg) {
			char * end = strchr (beg, '\0');
			while (isspace (end[-1])) end--;
			*end = '\0';
			break;
		}
	}
	return beg;
}


#endif /* IMG_XMP */
