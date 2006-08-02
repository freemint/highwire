/*******************************************************************************
 *
 * Private declarations for image.c and raster.c
 *
 *******************************************************************************
*/

typedef struct s_img_info * IMGINFO;
struct s_img_info {
	void  * _priv_data; /* decoder private data */
	void  * _priv_more;
	FILE  * _priv_file;
	BOOL   (*read)(IMGINFO, char * buffer);
	void   (*quit)(IMGINFO);
	UWORD    ImgWidth, ImgHeight; /* original size of the image */
	unsigned NumComps :8;  /* 3 = TrueColor, 1 = grayscale or palette mode */
	unsigned BitDepth :8;
	WORD     NumColors;
	char   * Palette;
	unsigned PalRpos :8;
	unsigned PalGpos :8;
	unsigned PalBpos :8;
	unsigned PalStep :8;
	WORD     Transp;
	WORD     Interlace;
	/* */
	void   (*raster)(IMGINFO, void * dst);
	void   * RowMem;
	char   * RowBuf;   /* initially RowMem but might be changed for interlaced */
	ULONG    RowBytes; /* >0: RowBuf will cover the whole ig, else one line    */
	void   * DthBuf;
	UWORD    DthWidth; /* calculated width of the image */
	UWORD    PixMask;
	size_t   PgSize;
	size_t   LnSize;
	size_t   IncXfx;
	size_t   IncYfx;
	ULONG    Pixel[256];
};

typedef struct s_rasterizer {
	const char * DispInfo;
	BOOL         StndBitmap;
	void       (*cnvpal)(IMGINFO, ULONG);
	void       (*functn)(IMGINFO, void *);
} * RASTERIZER;

RASTERIZER rasterizer (UWORD depth, UWORD comps);
	