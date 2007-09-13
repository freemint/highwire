
/*#define ICO_DBG
*/

/*static*/ BOOL decIco_start (const char * name, IMGINFO info);
static BOOL decIco_rd_4  (IMGINFO, char * buffer);
static BOOL decIco_rd_8  (IMGINFO, char * buffer);
static void decIco_quit  (IMGINFO);

static DECODER _decoder_ico = {
	DECODER_CHAIN,
	decIco_start
};
#undef  DECODER_CHAIN
#define DECODER_CHAIN &_decoder_ico


/* The maximum number of icons we'll try to handle in any one file */
#define MAXICONS 32

/* Header of a single icon image in an icon file */
typedef struct s_icon_dir_entry {
	CHAR  bWidth;
	CHAR  bHeight;
	CHAR  bColorCount;
	CHAR  bReserved;
#define  bHasMask bReserved /* used for our own purpose */
	UWORD wPlanes;
	UWORD wBitCount;
	ULONG dwBytesInRes;
	ULONG dwImageOffset;
	/*  */
	UWORD depth;
} ICONDIRENTRY;

/* Header of an icon file */
typedef struct s_icon_header {
	UWORD idReserved;
	UWORD idType;
	UWORD idCount;
/*	ICONDIRENTRY  idEntries[MAXICONS];*/
} ICONHEADER;

/* Bitmap header - this is on the images themselves */
typedef struct s_bitmap_info_header {
	ULONG biSize;
	LONG  biWidth;
	LONG  biHeight;
	UWORD biPlanes;
	UWORD biBitCount;
	ULONG biCompression;
	ULONG biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	ULONG biClrUsed;
	ULONG biClrImportant;
} BITMAPINFOHEADER;


typedef struct s_icon {
	WORD   Inc; /* width, 4byte aligned */
	WORD   Cnt; /* height */
	char * Ptr;
	/* image data follow here */
} * ICON;


/*---------------------------------------------------------------------------*/
static char fread_char (FILE * file)
{
	unsigned char b[1];
	fread (b, 1, 1, file);
	return (b[0]);
}

/*---------------------------------------------------------------------------*/
static short fread_word (FILE * file)
{
	unsigned char b[2];
	fread (b, 1, 2, file);
	return (((unsigned short)b[1] <<8) | b[0]);
}

/*---------------------------------------------------------------------------*/
static long fread_long (FILE * file)
{
	return (fread_word (file) | ((long)fread_word (file) <<16));
}


/*----------------------------------------------------------------------------*/
/*static*/ BOOL
decIco_start (const char * name, IMGINFO info)
{
	ICONDIRENTRY * found   = NULL;
	ICONDIRENTRY * entries = NULL;
	UWORD          count   = 0;
	
	FILE * file = fopen (name, "rb");
	if (!file) {
		return TRUE; /* avoid further tries of decoding */
	
	} else {
		/* Magic number for an icon file */
		/* This is the the idReserved and idType fields of the header */
		const char magic_number[4] = { 0, 0, 1, 0 };

		ICONHEADER iconheader;
		
		fread (&iconheader, 1, 4, file);
/*		iconheader.idReserved = fread_word (file);
		iconheader.idType     = fread_word (file);
*/		iconheader.idCount    = fread_word (file);
		if (memcmp (magic_number, &iconheader, sizeof(magic_number)) != 0) {
			/* not an icon file */
			fclose (file);
			return FALSE;
		
		} else if ((count = iconheader.idCount) == 0 || count >= MAXICONS) {
			printf ("decIco_start(): wrong icon count %i.\n", count);
			/*fclose (file);
			return FALSE;*/
		
		} else if ((entries = malloc (sizeof(ICONDIRENTRY) * count)) == NULL) {
			puts ("decXmp_start(): low memory.");
			/*fclose (file);
			return FALSE;*/
		}
#ifdef ICO_DBG
		printf ("______________________________________________________________\n"
		        "ICO: '%s'\nICO: %i icon%s\n",
		        name, count, (count == 1 ? "" : "s"));
#endif
	}
	
	if (!entries) {
		fclose (file);
		return FALSE;
	
	} else {
		ICONDIRENTRY * entry;
		int            i;
		for (i = 0, entry = entries; i < count; i++, entry++) {
			entry->bWidth        = fread_char (file);
			entry->bHeight       = fread_char (file);
			entry->bColorCount   = fread_char (file);
			entry->bReserved     = fread_char (file);
			entry->wPlanes       = fread_word (file);
			entry->wBitCount     = fread_word (file);
			entry->dwBytesInRes  = fread_long (file);
			entry->dwImageOffset = fread_long (file);
		}
		for (i = 0, entry = entries; i < count; i++, entry++) {
			BITMAPINFOHEADER header;
			short            depth;
			long             colors;
#ifdef ICO_DBG
			printf ("ICO: %i/%i = %i*%i *%i / %i / %i / %li\n", i+1, count,
			        entry->bWidth, entry->bHeight, entry->bColorCount,
			        entry->wPlanes, entry->wBitCount, entry->dwBytesInRes);
#endif
			fseek (file, entry->dwImageOffset, SEEK_SET);
			header.biSize          = fread_long (file);
			header.biWidth         = fread_long (file);
			header.biHeight        = fread_long (file);
			header.biPlanes        = fread_word (file);
			header.biBitCount      = fread_word (file);
			header.biCompression   = fread_long (file);
			header.biSizeImage     = fread_long (file);
			header.biXPelsPerMeter = fread_long (file);
			header.biYPelsPerMeter = fread_long (file);
			header.biClrUsed       = fread_long (file);
			header.biClrImportant  = fread_long (file);
			
			depth  = header.biPlanes * header.biBitCount;
			colors = (header.biClrImportant != 0 ? header.biClrImportant :
			          header.biClrUsed      != 0 ? header.biClrUsed      :
			                                       1 << depth);
#ifdef ICO_DBG
			printf ("           %li*%li  %ix%i %li/%li  -> %i, %li\n",
			        header.biWidth, header.biHeight,
			        header.biPlanes, header.biBitCount,
			        header.biClrImportant, header.biClrUsed, depth, colors);
#endif	
			if (depth != 4 && depth != 8) {
#ifdef ICO_DBG
				BOOL verbose = FALSE;
#else
				BOOL verbose = (depth != 1 && depth != 24);
#endif
				if (verbose) {
					printf ("decIco_start(): unsupported color depth %i, skipped.\n",
					        depth);
				}
			} else if (!found || (found->bWidth  < entry->bWidth &&
			                      found->bHeight < entry->bHeight)) {
				entry->dwImageOffset += header.biSize;
				entry->bColorCount   =  colors -1; /* take care for 256 */
				entry->wBitCount     =  depth;
				entry->bHasMask      = (header.biHeight == entry->bHeight *2);
				found                =  entry;
			}
		}
	}
	
	if (found){
		WORD   inc  = ((found->bWidth +3) & ~3) / (8 / found->wBitCount);
		size_t b_sz = inc * found->bHeight;
		ICON   icon = malloc (sizeof (struct s_icon) + b_sz);
		size_t c_sz = sizeof(long) * (found->bColorCount +1);
		long * cmap = (icon ? malloc (c_sz) : NULL);
		if (!cmap) {
			if (icon) free (icon);
			found = NULL;
			puts ("decXmp_start(): low memory.");
		
		} else {
			fseek (file, found->dwImageOffset, SEEK_SET);
			/* Read the colormap */
			fread (cmap,    1, c_sz, file);
			/* Read the image */
			fread (icon +1, 1, b_sz, file);
#ifdef ICO_DBG
			printf ("ICO: found %i*%i *%i / %i   %i %li\n",
			        found->bWidth, found->bHeight,
			        found->bColorCount +1, found->wBitCount, inc, b_sz);
#endif	
			icon->Inc = inc;
			icon->Cnt = found->bHeight;
			icon->Ptr = (char*)(icon +1) + inc * found->bHeight;
			
			info->_priv_data = icon;
			info->_priv_file = file;
			info->read       = (found->wBitCount == 8 ? decIco_rd_8 : decIco_rd_4);
			info->quit       = decIco_quit;
			
			info->ImgWidth   = found->bWidth;
			info->ImgHeight  = found->bHeight;
			info->NumComps   = 1; /* not supported */
			info->BitDepth   = found->wBitCount;
			info->NumColors  = found->bColorCount +1;
			info->Palette = (char*)cmap;
			info->PalRpos = 2;
			info->PalGpos = 1;
			info->PalBpos = 0;
			info->PalStep = 4;
			info->Transp     = -1;
			info->Interlace  = 0;
		}
	}
	
	free (entries);
	
	if (!found) {
		fclose (file);
		return FALSE;
		
	} else if (found->bHasMask) {
		/* Check for at least one transparent pixel */
		WORD    transp = -1;
		long    fsave  = ftell (file);
		ICON    icon   = info->_priv_data;
		int     nlongs = (found->bWidth +31) /32;
		int     nbytes = nlongs * 4;
		ULONG   rmask  = ~((1uL << (32 - (found->bWidth & 0x1F))) -1);
		CHAR    cbuf[32]; /* == 256 bits */
		ULONG * lbuf   = (ULONG*)cbuf;
		int     y, x;
		for (y = 0; y < found->bHeight && transp < 0; y++) {
			fread (cbuf, 1, nbytes, file);
			lbuf[nlongs -1] &= rmask;
			for (x = 0; x < nlongs; x++) {
				if (lbuf[x]) {
					transp = 1 << found->wBitCount;
#ifdef ICO_DBG
					printf ("ICO: transparency detected.\n");
#endif	
					break;
				}
				/*printf("%08lX", lbuf[x]);*/
			}
			/*printf("\n");*/
		}
		if (transp >= 0) {
			/* search for an available color index */
			CHAR   bit = 0;
			CHAR * ptr = icon->Ptr;
			memset (cbuf, 0, sizeof(cbuf));
			for (y = 0; y < found->bHeight; y++) {
				CHAR * p = ptr -= icon->Inc;
				for (x = 0; x < found->bWidth; x++) {
					CHAR pix = *(p++);
					cbuf[pix >>3] |= 1 << (7 - (pix & 7));
				}
			}
			ptr = cbuf + sizeof(cbuf);
			while (--transp >= 0) {
				if ((bit <<= 1) == 0) {
					bit = 0x01u;
					ptr--;
				}
				if (!(*ptr & bit)) {
#ifdef ICO_DBG
					printf ("ICO: transparency index %i %08lX.\n",
					        transp, ((ULONG*)info->Palette)[transp]);
#endif	
					break;
				}
			}
		}
		if (transp >= 0) {
			CHAR * ptr = (CHAR*)(icon +1);
			fseek (file, fsave, SEEK_SET);
			for (y = 0; y < found->bHeight; y++, ptr += icon->Inc) {
				CHAR * p   = ptr;
				CHAR   bit = 0;
				CHAR * msk = cbuf -1;
				fread (cbuf, 1, nbytes, file);
				for (x = 0; x < found->bWidth; x++, p++) {
					if ((bit >>= 1) == 0) {
						if (!(*(++msk))) {
							x += 7;
							p += 7;
							continue;
						}
						bit = 0x80u;
					}
					if (*msk & bit) {
						*p = transp;
					}
				}
			}
			info->Transp = transp;
		}
	}
	
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static BOOL
decIco_rd_4 (IMGINFO info, char * buffer)
{
	ICON icon = info->_priv_data;
	if (icon->Cnt > 0) {
		WORD   x = info->ImgWidth / 2;
		WORD   o = info->ImgWidth & 1;
		char * p = icon->Ptr -= icon->Inc;
		while (x--) {
			CHAR pix = *(p++);
			*(buffer++) = pix >> 4;
			*(buffer++) = pix & 0x0F;
		}
		if (o) {
			*(buffer)   = *(p) >> 4;
		}
	}
	return (icon->Cnt-- > 0);
}

/*----------------------------------------------------------------------------*/
static BOOL
decIco_rd_8 (IMGINFO info, char * buffer)
{
	ICON icon = info->_priv_data;
	if (icon->Cnt > 0) {
		WORD   x = info->ImgWidth;
		char * p = icon->Ptr -= icon->Inc;
		while (x--) {
			*(buffer++) = *(p++);
		}
	}
	return (icon->Cnt-- > 0);
}

/*----------------------------------------------------------------------------*/
static void
decIco_quit (IMGINFO info)
{
	ICON icon = info->_priv_data;
	if (icon) {
		free (info->_priv_data);
		fclose (info->_priv_file);
		info->_priv_file = NULL;
		info->_priv_data = NULL;
	}
}
