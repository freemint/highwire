/* @(#)highwire/image.c
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h> /* memcpy */
#include <time.h>
#include <gemx.h>

#ifdef __PUREC__
# define CONTAINR struct s_containr *
#endif

#include "global.h"
#include "Location.h"
#include "Containr.h"
#include "schedule.h"
#include "cache.h"


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
	char   * RowBuf;
	void   * DthBuf;
	UWORD    DthWidth; /* calculated width of the image */
	UWORD    PixMask;
	size_t   PgSize;
	size_t   LnSize;
	size_t   IncXfx;
	size_t   IncYfx;
	ULONG    Pixel[256];
};

static void invalid_cnvpal(void) { puts("invalid cnvpal"); exit(1); }
static void invalid_raster(void) { puts("invalid raster"); exit(1); }
static void invalid_gscale(void) { puts("invalid gscale"); exit(1); }
static void invalid_dither(void) { puts("invalid dither"); exit(1); }

static void   cnvpal_mono  (IMGINFO, ULONG);
static void (*cnvpal_color)(IMGINFO, ULONG)  = (void*)invalid_cnvpal;
static void   raster_mono  (IMGINFO, void *);
static void (*raster_cmap) (IMGINFO, void *) = (void*)invalid_raster;
static void (*raster_gray) (IMGINFO, void *) = (void*)invalid_gscale;
static void (*raster_true) (IMGINFO, void *) = (void*)invalid_dither;
static BOOL   stnd_bitmap  = FALSE;

static char  disp_info[10] = "";
static short pixel_val[256];
static ULONG cube216[216];
static ULONG graymap[32];


static void init_display (void);
static IMGINFO  get_decoder (const char * file);
static pIMGDATA setup       (IMAGE, IMGINFO);
static void     read_img    (IMAGE, IMGINFO, pIMGDATA);
static BOOL     image_job   (void *, long);

#define img_hash(w,h,c) (((((long)((char)c)<<12) ^ w)<<12) ^ h)


/*============================================================================*/
const char *
image_dispinfo(void)
{
	if (!disp_info[0]) {
		init_display();
	}
	return disp_info;
}


/*----------------------------------------------------------------------------*/
static void
set_word (IMAGE img)
{
	struct word_item * word = img->word;
	short h = img->disp_h + img->vspace;
	
	switch (word->vertical_align) {
		case ALN_TOP:
			word->word_height = (word->line
			                     ? word->line->Ascend : img->alt_h + img->vspace);
			break;
		case ALN_MIDDLE:
			word->word_height = (h + img->vspace) /2;
			break;
		default:
			word->word_height = h;
	}
	word->word_tail_drop = max (0, h - word->word_height + img->vspace);
	word->word_width = img->disp_w + (img->hspace * 2);
}


/*============================================================================*/
IMAGE
new_image (FRAME frame, TEXTBUFF current, const char * file, LOCATION base,
           short w, short h, short vspace, short hspace)
{
	LOCATION loc = new_location (file, base);
	IMAGE    img = malloc (sizeof(struct s_image));
	long     hash;
	CACHED   cached;
	
	if (!disp_info[0]) {
		init_display();
	}
	
	img->vspace = vspace;
	img->hspace = hspace;

	img->set_w = w;
	img->set_h = h;
	
	img->source    = loc;
	img->u.Data    = NULL;
	img->frame     = frame;
	img->paragraph = current->paragraph;
	img->word      = current->word;
	img->backgnd   = current->backgnd;
	img->alt_w     = 0;
	img->alt_h     = img->word->word_height;
	img->offset.Origin = &img->paragraph->Offset;
	img->word->image = img;
	
	hash   = img_hash ((w > 0 ? w : 0), (h > 0 ? h : 0), -1);
	cached = cache_lookup (loc, hash, &hash);
	
	if (cached) {
		short bgnd = (hash >>24) & 0xFF;
		if (bgnd == 0xFF) {
			img->backgnd = bgnd = -1;  /* no transparency */
		}
		if (w > 0 && h > 0 && hash != img_hash (w, h, bgnd)) {
			cached = NULL;  /* absolute size doesn't match */
		}
		hash = bgnd;
	}
	
	if (cached) {
		cIMGDATA data = cached;
		if (!h) {
			h = img->set_h = data->img_h;
		}
		if (w < 0) {
			w = data->fd_w;
			if (h < 0) {
				h = data->fd_h;
			} else if (h != data->fd_h) {
				cached = NULL;
			}
		} else {
			if (!w) {
				w = img->set_w = data->img_w;
			}
			if (h < 0) {
				h = img->set_h = ((long)data->img_h * -h +512) /1024;
			}
			if (w != data->fd_w || h != data->fd_h || hash != img->backgnd) {
				cached = cache_lookup (loc, img_hash (w, h, img->backgnd), NULL);
			}
		}
		if (cached) {
			img->u.Data = cache_bound (cached, &img->source);
		}
	}
	img->disp_w = (w > 0 ? w : 16);
	img->disp_h = (h > 0 ? h : 16);
	set_word (img);
		
	if (!img->u.Data && PROTO_isLocal (loc->Proto)) {
		sched_insert (image_job, img, (long)img->frame->Container);
		containr_notify (img->frame->Container, HW_ActivityBeg, NULL);
	}
	
	return img;
}

/*============================================================================*/
void
delete_image (IMAGE * _img)
{
	IMAGE img = *_img;
	if (img) {
		if (img->u.Data) {
			CACHEOBJ cob = cache_release ((CACHED*)&img->u.Data, FALSE);
			if (cob) {
				printf ("delete_image(): remove cached\n   '%s%s'\n",
				        location_Path (img->source, NULL), img->source->File);
				free (cob);
			} else if (img->u.Mfdb) {
				printf ("delete_image(): not in cache\n   '%s%s'\n",
				        location_Path (img->source, NULL), img->source->File);
				free (img->u.Mfdb);
				img->u.Mfdb = NULL;
			}
		} else {
			if (sched_remove (image_job, img)) {
				containr_notify (img->frame->Container, HW_ActivityEnd, NULL);
			}
		}
		free_location (&img->source);
		free (img);
		*_img = NULL;
	}
}


/*----------------------------------------------------------------------------*/
static void
img_scale (IMAGE img, short img_w, short img_h, IMGINFO info)
{
	size_t  scale_x, scale_y;

	/* calculate scaling steps (32bit fix point) */
	
	if (!img->set_w) {
		scale_x     = 0x10000uL;
		img->disp_w = img->set_w = img_w;
	} else if (img_w != img->disp_w) {
		scale_x     = (((size_t)img_w <<16) + (img->disp_w /2)) / img->disp_w;
	} else {
		scale_x     = 0x10000uL;
	}
	
	if (img->set_h < 0) {
		scale_y     = (scale_x * 1024 +(-img->set_h /2)) / -img->set_h;
		img->disp_h = ((size_t)img_h <<16) / scale_y;
	} else if (!img->set_h) {
		scale_y     = 0x10000uL;
		img->disp_h = img->set_h = img_h;
	} else if (img_h != img->disp_h) {
		scale_y     = (((size_t)img_h <<16) + (img->disp_h /2)) / img->disp_h;
	} else {
		scale_y     = 0x10000uL;
	}
	
	if (info) {
		info->IncXfx = scale_x;
		info->IncYfx = scale_y;
	}
}

/*============================================================================*/
void
image_calculate (IMAGE img, short par_width)
{
	if (img->set_w < 0) {
		short width = ((long)par_width * -img->set_w +512) /1024 - img->hspace *2;
		if (img->disp_w != (width > 0 ? width :1)) {
			cIMGDATA data = img->u.Data;
			if (!data) {
				long hash = 0;
				data = cache_lookup (img->source, -1, &hash);
			}
			img->disp_w = width;
			if (data) {
				img_scale (img, data->img_w, data->img_h, NULL);
			}
			set_word (img);
			if (img->u.Data) {
				CACHEOBJ cob = cache_release ((CACHED*)&img->u.Data, TRUE);
				if (cob) {
					printf ("image_calculate(): remove cached\n   '%s%s'\n",
					        location_Path (img->source, NULL), img->source->File);
					free (cob);
				} else if (img->u.Mfdb) {
					printf ("image_calculate(): not in cache\n   '%s%s'\n",
					        location_Path (img->source, NULL), img->source->File);
					free (img->u.Mfdb);
					img->u.Mfdb = NULL;
				}
				sched_insert (image_job, img, (long)img->frame->Container);
				containr_notify (img->frame->Container, HW_ActivityBeg, NULL);
			}
		}
	
	} else if (!img->u.Data && (!img->set_w || !img->set_h)) {
		long     hash = 0;
		cIMGDATA data = cache_lookup (img->source, -1, &hash);
		if (data) {
			if (!img->set_w) {
				img->set_w = img->disp_w = data->img_w;
			}
			if (!img->set_h) {
				img->set_h = img->disp_h = data->img_h;
			} else if (img->set_h < 0) {
				img->set_h = img->disp_h = ((long)data->img_h * -img->set_h +512)
				                         / 1024;
			}
			if ((char)(hash >>24) == 0xFF) {
				img->backgnd = -1;
			}
			set_word (img);
		}
	
	} else if (img->word->vertical_align == ALN_TOP) {
		set_word (img);
	}
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static BOOL
image_job (void * arg, long invalidated)
{
	IMAGE    img = arg;
	PARAGRPH par = img->paragraph;
	LOCATION loc = img->source;
	FRAME  frame = img->frame;
	GRECT  rec   = frame->Container->Area, * clip = &rec;
	short  old_w = img->disp_w;
	short  old_h = img->disp_h;
	int  calc_xy = 0;
	
	long   hash   = 0;
	CACHED cached = NULL;
	
	if (invalidated) {
		containr_notify (frame->Container, HW_ActivityEnd, NULL);
		return FALSE;
	
	} else {
		struct s_cache_info info;
		long    ident = (img->set_w <= 0 || img->set_h <= 0 ? 0
		                 : img_hash (img->disp_w, img->disp_h, img->backgnd));
		CRESULT res   = cache_query (loc, ident, &info);
		if (res & CR_MATCH) {
			cached = info.Object;
		
		} else if (res & CR_FOUND) {
			ident = info.Ident;
			if ((char)(ident >>24) == 0xFF) {
				img->backgnd = -1;
			}
			if (ident == img_hash (img->disp_w, img->disp_h, img->backgnd)) {
				cached = info.Object;
			}
		}
	}	
	if (cached) {
		img->u.Data = cache_bound (cached, &img->source);
	
	} else {
		pIMGDATA data = NULL;
		IMGINFO  info;
		
		containr_notify (frame->Container, HW_ImgBegLoad, img->source->FullName);
		
		if ((info = get_decoder (loc->FullName)) != NULL &&
		    (data = setup (img, info))           != NULL) {
			info->RowBuf = malloc (info->ImgWidth * info->NumComps);
			read_img (img, info, data);
			(*info->quit)(info);
			if (info->RowBuf) free (info->RowBuf);
			if (info->DthBuf) free (info->DthBuf);
			free (info);
		}
		if (data) {
			if (data->fd_stand) {
				pIMGDATA trns = malloc (sizeof (struct s_img_data)
				                        + data->mem_size);
				if (trns) {
					*trns         = *data;
					trns->fd_addr = (trns +1);
				} else {
					trns = data;
				}
				vr_trnfm (vdi_handle, (MFDB*)data, (MFDB*)trns);
				
				if (trns != data) {
					free (data);
					data = trns;
				}
			}
			set_word (img);
			hash = img_hash (img->disp_w, img->disp_h, img->backgnd);
			img->u.Data = cache_insert (img->source, hash,
			                            (CACHEOBJ*)&data, data->mem_size, free);
		} else {
			clip = NULL;
		}
	}
	
	if ((img->set_w >= 0 && par->min_width < img->word->word_width)
	    || img->disp_w != old_w || img->disp_h != old_h) {
		long par_x = par->Offset.X;
		long par_y = par->Offset.Y;
		long par_w = par->Width;
		long par_h = par->Height;
		if (par->min_width < img->disp_w) {
			 par->min_width = img->disp_w;
			 par->max_width = 0;
		}
		content_minimum (&frame->Page);
		
		if (containr_calculate (frame->Container, NULL)) {
			calc_xy = 0;
		} else if (par_w == par->Width && par_h == par->Height) {
			calc_xy = 1;
			rec.g_w = par_w;
			rec.g_h = par_h;
		} else if (par_x == par->Offset.X && par_y == par->Offset.Y) {
			calc_xy = -1;
		}
	
	} else if (img->u.Data) {
		calc_xy = 1;
		rec.g_w = img->disp_w + img->hspace;
		rec.g_h = img->disp_h + img->vspace;
	}

	if (calc_xy) {
		OFFSET * origin = img->offset.Origin;
		short x = img->offset.X + frame->clip.g_x - frame->h_bar.scroll;
		short y = img->offset.Y + frame->clip.g_y - frame->v_bar.scroll;
		while (origin) {
			x += origin->X;
			y += origin->Y;
			origin  =  origin->Origin;
		}
		if (calc_xy > 0) {
			rec.g_x = x;
		}
		rec.g_y = y;
	}
	
	if (!cached) {
		containr_notify (frame->Container, HW_ImgEndLoad, clip);
	} else if (img->u.Data) {
		containr_notify (frame->Container, HW_PageUpdated, clip);
	}
	containr_notify (frame->Container, HW_ActivityEnd, NULL);
	
	return FALSE;
}


/*----------------------------------------------------------------------------*/
static pIMGDATA
setup (IMAGE img, IMGINFO info)
{
	short    n_planes = (info->BitDepth > 1 ? planes : 1);
	size_t   wd_width;
	size_t   pg_size;
	size_t   mem_size;
	ULONG    transpar;
	pIMGDATA data;
	
	img_scale (img, info->ImgWidth, info->ImgHeight, info);
	
	wd_width = (img->disp_w + 15) / 16;
	pg_size  = wd_width * img->disp_h;
	mem_size = pg_size *2 * n_planes;
	data     = malloc (sizeof (struct s_img_data) + mem_size);
	data->mem_size   = mem_size;
	data->img_w      = info->ImgWidth;
	data->img_h      = info->ImgHeight;
	data->fd_addr    = (data +1);
	data->fd_w       = img->disp_w;
	data->fd_h       = img->disp_h;
	data->fd_wdwidth = wd_width;
	data->fd_stand   = (info->BitDepth > 1 ? stnd_bitmap : FALSE);
	data->fd_nplanes = n_planes;
	data->fd_r1 = data->fd_r2 = data->fd_r3 = 0;
	
	info->DthWidth = img->disp_w;
	info->PixMask  = (1 << info->BitDepth) -1;
	info->PgSize   = pg_size;
	info->LnSize   = wd_width;
	if (!data->fd_stand) {
		info->LnSize *= n_planes;
	}
	if (info->Transp < 0) {
		transpar = img->backgnd = -1;
	} else if (planes <= 8) {
		transpar = img->backgnd;
	} else {
		short  rgb[3];
		vq_color (vdi_handle, img->backgnd, 0, rgb);
		transpar = ((( (((long)rgb[0] * 255 +500) / 1000)  <<8)
		             | (((long)rgb[1] * 255 +500) / 1000)) <<8)
		           |    ((long)rgb[2] * 255 +500) / 1000;
	}
	if (info->BitDepth > 1) {
		if (planes <= 2) {
			size_t size = (img->disp_w +1) *2;
			info->DthBuf = malloc   (size);
			memset (info->DthBuf, 0, size);
		}
		if (info->Palette) {
			(*cnvpal_color)(info, transpar);
			info->raster = raster_cmap;
		} else {
			info->raster = (info->NumComps == 3 ? raster_true : raster_gray);
		}
		data->bgnd = G_WHITE;
		data->fgnd = G_BLACK;
	} else {
		cnvpal_mono (info, transpar);
		data->bgnd = info->Pixel[0];
		data->fgnd = info->Pixel[1];
		info->raster = raster_mono;
	}
	
/*	printf ("scale: [%i,%i] %lX.%04lX/%lX.%04lX [%i,%i] \n", img_w, img_h,
	        scale_x >>16, scale_x & 0xFFFF, scale_y >>16, scale_y & 0xFFFF,
	        img->disp_w, img->disp_h);
*/
	return data;
}


/*******************************************************************************
 *
 *   Line Raster Functions
 */

/*------------------------------------------------------------------------------
 * monochrome format
 */
static void
raster_mono (IMGINFO info, void * _dst)
{
#if defined(__GNUC__)
	__asm__ volatile ("
		subq.w	#1, %2 | width
		move.l	%3, d0 | scale -> index
		addq.l	#1, d0
		lsr.l		#1, d0
		0:	clr.w 	d1          | chunk
			move.w	#0x8000, d2 | pixel
		1:	swap		d0
			btst		#0, (d0.w,%1) | (src[index>>16] & 1)
			beq.b		2f
			or.w		d2, d1        | chunk |= pixel
		2:	swap		d0
			add.l		%3, d0        | index += info->IncXfx
			lsr.w		#1, d2
			dbeq		%2, 1b
		move.w	d1, (%0)+
		subq.w	#1, %2
		bpl.b		0b
		"
		:                                       /* output */
		: "a"(_dst), "a"(info->RowBuf),
		  /*  %0         %1             */
		  "d"(info->DthWidth),"d"(info->IncXfx) /* input  */
		  /*  %2                  %3            */
		: "d0","d1","d2"                        /* clobbered */
	);
	
#elif defined(__PUREC__)
	extern void pc_raster_mono (void *, void *, short, size_t);
	pc_raster_mono (_dst, info->RowBuf, info->DthWidth, info->IncXfx);
	
#else
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		if (info->RowBuf[x >>16] & 1) chunk |= pixel;
		
		if (!--width || !(pixel >>= 1)) {
			*(dst++) = chunk;
			chunk    = 0;
			pixel    = 0x8000;
		}
		x += info->IncXfx;
	
	} while (width);
#endif
}

/*------------------------------------------------------------------------------
 * device independend, for unrecognized screen format.
 */
static void
raster_stnd (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	UWORD   pixel = 0x8000;
	do {
		short   color = map[(UWORD)info->RowBuf[x >>16] & mask];
		UWORD * chunk = dst;
		short   i     = planes;
		do {
			if (color & 1) *chunk |=  pixel;
			else           *chunk &= ~pixel;
			chunk +=  info->PgSize;
			color >>= 1;
		} while (--i);
		
		if (!(pixel >>= 1)) {
			pixel = 0x8000;
			dst++;
		}
		x += info->IncXfx;
	
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 1 plane, uses a simple intensity dither.  the pixel index array must contain
 * intensity values of [0..4080].
 */
static void
raster_D1 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	short * buf   = info->DthBuf;
	short   ins   = 0;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		ins += *buf + map[(short)info->RowBuf[x >>16] & mask];
		
		if (ins < 2040) {
			chunk |= pixel;
		} else {
			ins -= 4080;
		}
		*(buf++) = (ins >>= 2);
		
		if (!--width || !(pixel >>= 1)) {
			*(dst++) = chunk;
			chunk    = 0;
			pixel    = 0x8000;
		}
		x += info->IncXfx;
		
	} while (width);
}
/*------------------------------------*/
static void
gscale_D1 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	short * buf   = info->DthBuf;
	short   ins   = 0;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		ins += *buf + (short)info->RowBuf[x >>16];
		
		if (ins < 0x80) {
			chunk |= pixel;
		} else {
			ins -= 0xFF;
		}
		*(buf++) = (ins >>= 2);
		
		if (!--width || !(pixel >>= 1)) {
			*(dst++) = chunk;
			chunk    = 0;
			pixel    = 0x8000;
		}
		x += info->IncXfx;
		
	} while (width);
}
/*------------------------------------*/
static void
dither_D1 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	short * buf   = info->DthBuf;
	short   ins   = 0;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		CHAR  * rgb   = &info->RowBuf[(x >>16) *3];
		ins += *buf + (WORD)rgb[0] *5 + (WORD)rgb[1] *9 + (WORD)rgb[2] *2;
		
		if (ins < 2040) {
			chunk |= pixel;
		} else {
			ins -= 4080;
		}
		*(buf++) = (ins >>= 2);
		
		if (!--width || !(pixel >>= 1)) {
			*(dst++) = chunk;
			chunk    = 0;
			pixel    = 0x8000;
		}
		x += info->IncXfx;
		
	} while (width);
}

/*------------------------------------------------------------------------------
 * 2 planes interleaved words format, uses a simple intensity dither.  the pixel
 * index array must contain intensity values of [0..4080].
 */
static void
raster_D2 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	short * buf   = info->DthBuf;
	short   ins   = 0;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		ins += *buf + map[(short)info->RowBuf[x >>16] & mask];
		
		if (ins < 2040) {
			chunk |= pixel;
		} else {
			ins -= 4080;
		}
		*(buf++) = (ins >>= 2);
		
		if (!--width || !(pixel >>= 1)) {
			*(dst++) = chunk;
			*(dst++) = chunk;
			chunk    = 0;
			pixel    = 0x8000;
		}
		x += info->IncXfx;
		
	} while (width);
}
/*------------------------------------*/
static void
gscale_D2 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	short * buf   = info->DthBuf;
	short   ins   = 0;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		ins += *buf + (short)info->RowBuf[x >>16];
		
		if (ins < 0x80) {
			chunk |= pixel;
		} else {
			ins -= 0xFF;
		}
		*(buf++) = (ins >>= 2);
		
		if (!--width || !(pixel >>= 1)) {
			*(dst++) = chunk;
			*(dst++) = chunk;
			chunk    = 0;
			pixel    = 0x8000;
		}
		x += info->IncXfx;
		
	} while (width);
}
/*------------------------------------*/
static void
dither_D2 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	short * buf   = info->DthBuf;
	short   ins   = 0;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		CHAR  * rgb   = &info->RowBuf[(x >>16) *3];
		ins += *buf + (WORD)rgb[0] *5 + (WORD)rgb[1] *9 + (WORD)rgb[2] *2;
		
		if (ins < 2040) {
			chunk |= pixel;
		} else {
			ins -= 4080;
		}
		*(buf++) = (ins >>= 2);
		
		if (!--width || !(pixel >>= 1)) {
			*(dst++) = chunk;
			*(dst++) = chunk;
			chunk    = 0;
			pixel    = 0x8000;
		}
		x += info->IncXfx;
		
	} while (width);
}

/*------------------------------------------------------------------------------
* Converts 'num'=[1..16] pixel bytes into 'depth'=[1..8] word chunks.
* Used for the I4 and I8 interleaved words formats.
*/
static void
raster_chunks (CHAR * src, UWORD * dst, UWORD num, UWORD depth)
{
	UWORD   mask  = 0x8000;
	UWORD * chunk = dst;
	CHAR    pixel = *src;
	short   i     = depth;
	do {
		*(chunk++) = (pixel & 1 ? mask : 0);
		pixel >>= 1;
	} while (--i);
	
	while (--num) {
		mask >>= 1;
		chunk = dst;
		pixel = *(++src);
		i     = depth;
		do {
			if (pixel & 1) *chunk |=  mask;
			else           *chunk &= ~mask;
			chunk++;
			pixel >>= 1;
		} while (--i);
	}
}
#define raster_chunk4(s,d,n)   raster_chunks (s, d, n, 4)
#define raster_chunk8(s,d,n)   raster_chunks (s, d, n, 8)

/*------------------------------------------------------------------------------
 * 4 planes interleaved words format
 */
static void
raster_I4 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		UWORD idx = info->RowBuf[x >>16];
		*(tmp++)  = info->Pixel[idx];
		
		if (!--width || !--n) {
			raster_chunk4 (buf, dst, tmp - buf);
			dst += 4;
			n    = 16;
			tmp  = buf;
		}
		x += info->IncXfx;
		
	} while (width);
}
/*------------------------------------*/
static void
gscale_I4 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		UWORD idx = info->RowBuf[x >>16] >>3;
		*(tmp++)  = *(CHAR*)&graymap[idx];
		
		if (!--width || !--n) {
			raster_chunk4 (buf, dst, tmp - buf);
			dst += 4;
			n    = 16;
			tmp  = buf;
		}
		x += info->IncXfx;
		
	} while (width);
}
/*------------------------------------*/
static void
dither_I4 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		UWORD  idx = ((((UWORD)rgb[0] *6) /256)  *6
		           +  (((UWORD)rgb[1] *6) /256)) *6
		           +  (((UWORD)rgb[2] *6) /256);
		*(tmp++)   = *(CHAR*)&cube216[idx];
		
		if (!--width || !--n) {
			raster_chunk4 (buf, dst, tmp - buf);
			dst += 4;
			n    = 16;
			tmp  = buf;
		}
		x += info->IncXfx;
		 
	} while (width);
}

/*------------------------------------------------------------------------------
 * 8 planes interleaved words format
 */
static void
raster_I8 (IMGINFO info, void * _dst)
{
#if 0 && defined(__GNUC__)
	size_t  x     = (info->IncXfx +1) /2;
	__asm__ volatile ("
		subq.w	#1, %4
		swap		%6 | -> chunk counter
		
		1: | line loop
		clr.w		%6
		clr.l		d4
		clr.l		d5
		clr.l		d6
		clr.l		d7
		
		5: | chunk loop
		swap		%4 | -> value
		swap		%6 | -> mask
		move.w	(%3), d3
		add.l		%5, (%3)
		move.b	0(%1,d3.w), d3 | palette index
		and.w		%6, d3
		lsl.w		#2, d3
		move.w	2(%2,d3.w), %4 | pixel value
		swap		%6 | -> chunk counter
		
		moveq.l	#0x03, d3 |chunks 0/1
		and.b		%4, d3
		ror.l 	#1, d3
		ror.w		#1, d3
		lsr.l		%6, d3
		or.l		d3, d4
		moveq.l	#0x0C, d3 |chunks 2/3
		and.b		%4, d3
		ror.l 	#3, d3
		ror.w		#1, d3
		lsr.l		%6, d3
		or.l		d3, d5
		moveq.l	#0x30, d3 |chunks 4/5
		and.b		%4, d3
		ror.l 	#5, d3
		ror.w		#1, d3
		lsr.l		%6, d3
		or.l		d3, d6
		move.l	#0xC0, d3 |chunks 6/7
		and.b		%4, d3
		ror.l 	#7, d3
		ror.w		#1, d3
		lsr.l		%6, d3
		or.l		d3, d7
		
		swap		%4 | -> width
		addq.b	#1, %6
		btst.b	#4, %6
		dbne		%4, 5b | chunk loop
		
		movem.l	d4-d7, (%0)
		adda.w	#16, %0
		subq.w	#1, %4
		bpl.b		1b
		"
		:
		: "a"(_dst),"a"(info->RowBuf),"a"(info->Pixel),"a"(&x),
		/*    %0        %1                %2               %3 */
		  "d"(info->DthWidth),"d"(info->IncXfx),"d"(info->PixMask)
		/*    %4                  %5                %6          */
		: "d3","d4","d5","d6","d7"
	);
	
#elif 0 && defined(__PUREC__)
	extern void pc_raster_I8 (void *, void *, void *, short, size_t, UWORD);
	pc_raster_I8 (_dst, info->RowBuf, info->Pixel,
	              info->DthWidth, info->IncXfx, info->PixMask);
	
#else
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		UWORD idx = info->RowBuf[x >>16];
		*(tmp++)  = info->Pixel[idx];
		
		if (!--width || !--n) {
			raster_chunk8 (buf, dst, tmp - buf);
			dst += 8;
			n    = 16;
			tmp  = buf;
		}
		x += info->IncXfx;
		
	} while (width);
#endif
}
/*------------------------------------*/
static void
gscale_I8 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		UWORD idx = info->RowBuf[x >>16] >>3;
		*(tmp++)  = *(CHAR*)&graymap[idx];
		
		if (!--width || !--n) {
			raster_chunk8 (buf, dst, tmp - buf);
			dst += 8;
			n    = 16;
			tmp  = buf;
		}
		x += info->IncXfx;
		
	} while (width);
}
/*------------------------------------*/
static void
dither_I8 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		UWORD  idx = ((((UWORD)rgb[0] *6) /256)  *6
		           +  (((UWORD)rgb[1] *6) /256)) *6
		           +  (((UWORD)rgb[2] *6) /256);
		*(tmp++)   = *(CHAR*)&cube216[idx];
		
		if (!--width || !--n) {
			raster_chunk8 (buf, dst, tmp - buf);
			dst += 8;
			n    = 16;
			tmp  = buf;
		}
		x += info->IncXfx;
		 
	} while (width);
}

/*------------------------------------------------------------------------------
 * 8 planes packed pixels formart.
 */
static void
raster_P8 (IMGINFO info, void * _dst)
{
	CHAR  * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		*(dst++) = map[(short)info->RowBuf[x >>16] & mask];
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_P8 (IMGINFO info, void * _dst)
{
	CHAR * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		UWORD idx = info->RowBuf[x >>16] >>3;
		*(dst++)  = *(CHAR*)&graymap[idx];
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_P8 (IMGINFO info, void * _dst)
{
	CHAR * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		UWORD  idx = ((((UWORD)rgb[0] *6) /256)  *6
		           +  (((UWORD)rgb[1] *6) /256)) *6
		           +  (((UWORD)rgb[2] *6) /256);
		*(dst++)   = *(CHAR*)&cube216[idx];
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 15 planes packed pixels formart, the pixel index array must contain 15bit RGB
 * values.
 */
static void
raster_15 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		*(dst++) = map[(short)info->RowBuf[x >>16] & mask];
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_15 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		UWORD rgb = info->RowBuf[(x >>16)];
		*(dst++)  = ((rgb & 0xF8) <<7) | ((rgb & 0xF8) <<2) | (rgb >>3);
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_15 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++)   = (((UWORD)rgb[0] & 0xF8) <<7)
		           | (((UWORD)rgb[1] & 0xF8) <<2)
		           |         (rgb[2]         >>3);
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 15 planes packed pixels formart in wrong (intel) byte order, the pixel index
 * array must contain 15bit RGB values.
 */
static void
raster_15r (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		UWORD rgb = map[(short)info->RowBuf[x >>16] & mask];
		*(dst++)  = (rgb >> 8) | (rgb << 8);
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_15r (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		UWORD rgb = info->RowBuf[(x >>16)];
		rgb       = ((rgb & 0xF8) <<7) | ((rgb & 0xF8) <<2) | (rgb >>3);
		*(dst++)  = (rgb >> 8) | (rgb << 8);
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_15r (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		UWORD  pix = (((UWORD)rgb[0] & 0xF8) <<7)
		           | (((UWORD)rgb[1] & 0xF8) <<2)
		           |         (rgb[2]         >>3);
		*(dst++)   = (pix >> 8) | (pix << 8);
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 16 planes packed pixels formart, the pixel index array must contain 16bit RGB
 * values.
 */
static void
raster_16 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		*(dst++) = map[(short)info->RowBuf[x >>16] & mask];
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_16 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		UWORD rgb = info->RowBuf[(x >>16)];
		*(dst++)  = ((rgb & 0xF8) <<8) | ((rgb & 0xFC) <<3) | (rgb >>3);
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_16 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++)   = (((UWORD)rgb[0] & 0xF8) <<8)
		           | (((UWORD)rgb[1] & 0xFC) <<3)
		           |         (rgb[2]         >>3);
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 16 planes packed pixels formart in wrong (intel) byte order, the pixel index
 * array must contain 16bit RGB values.
 */
static void
raster_16r (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		UWORD rgb = map[(short)info->RowBuf[x >>16] & mask];
		*(dst++)  = (rgb >> 8) | (rgb << 8);
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_16r (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		UWORD rgb = info->RowBuf[(x >>16)];
		rgb       = ((rgb & 0xF8) <<8) | ((rgb & 0xFC) <<3) | (rgb >>3);
		*(dst++)  = (rgb >> 8) | (rgb << 8);
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_16r (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		UWORD  pix = (((UWORD)rgb[0] & 0xF8) <<8)
		           | (((UWORD)rgb[1] & 0xFC) <<3)
		           |         (rgb[2]         >>3);
		*(dst++)   = (pix >> 8) | (pix << 8);
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 24 planes packed pixels formart.
 */
static void
raster_24 (IMGINFO info, void * _dst)
{
	CHAR  * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		CHAR * rgb = (CHAR*)&map[(short)info->RowBuf[x >>16] & mask];
		*(dst++) = *(++rgb);
		*(dst++) = *(++rgb);
		*(dst++) = *(++rgb);
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_24 (IMGINFO info, void * _dst)
{
	CHAR * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR rgb = info->RowBuf[(x >>16)];
		*(dst++) = rgb;
		*(dst++) = rgb;
		*(dst++) = rgb;
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_24 (IMGINFO info, void * _dst)
{
	CHAR * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++) = *(rgb++);
		*(dst++) = *(rgb++);
		*(dst++) = *(rgb);
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 24 planes packed pixels formart in wrong (intel) byte order.
 */
static void
raster_24r (IMGINFO info, void * _dst)
{
	CHAR  * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		CHAR * rgb = (CHAR*)&map[(short)info->RowBuf[x >>16] & mask];
		*(dst++) = rgb[3];
		*(dst++) = rgb[2];
		*(dst++) = rgb[1];
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_24r (IMGINFO info, void * _dst)
{
	CHAR * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR rgb = info->RowBuf[(x >>16)];
		*(dst++) = rgb;
		*(dst++) = rgb;
		*(dst++) = rgb;
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_24r (IMGINFO info, void * _dst)
{
	CHAR * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++) = rgb[2];
		*(dst++) = rgb[1];
		*(dst++) = rgb[0];
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 32 planes packed pixels formart.
 */
static void
raster_32 (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		*(dst++) = map[(short)info->RowBuf[x >>16] & mask];
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_32 (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		ULONG rgb = info->RowBuf[(x >>16)];
		*(dst++)  = (((rgb <<8) | rgb) <<8) | rgb;
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_32 (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short  width = info->DthWidth;
	size_t x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++)   = ((((ULONG)rgb[0] <<8) | rgb[1]) <<8) | rgb[2];
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 32 planes packed pixels formart in wrong (intel) byte order.
 */
static void
raster_32r (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		CHAR * rgb = (CHAR*)&map[(short)info->RowBuf[x >>16] & mask];
		*(dst++) = (((((long)rgb[3] <<8) | rgb[2]) <<8) | rgb[1]) <<8;
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_32r (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		ULONG rgb = info->RowBuf[(x >>16)];
		*(dst++)  = ((((rgb <<8) | rgb) <<8) | rgb) <<8;
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
dither_32r (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++)   = (((((ULONG)rgb[2] <<8) | rgb[1]) <<8) | rgb[0]) <<8;
		x += info->IncXfx;
	} while (--width);
}


/*******************************************************************************
 *
 *   Palette Converter
 */

/*----------------------------------------------------------------------------*/
static void
cnvpal_mono (IMGINFO info, ULONG backgnd)
{
	if (info->Palette) {
		ULONG  bgnd, fgnd;
		char * rgb = info->Palette;
		bgnd = ((((long)rgb[info->PalRpos] <<8) | rgb[info->PalGpos]) <<8)
		              | rgb[info->PalBpos];
		rgb += info->PalStep;
		fgnd = ((((long)rgb[info->PalRpos] <<8) | rgb[info->PalGpos]) <<8)
		              | rgb[info->PalBpos];
		info->Pixel[0] = (info->Transp == 0 ? backgnd : remap_color (bgnd));
		info->Pixel[1] = (info->Transp == 1 ? backgnd : remap_color (fgnd));
		
	} else {
		info->Pixel[0] = G_WHITE;
		info->Pixel[1] = G_BLACK;
	}
}

/*----------------------------------------------------------------------------*/
static void
cnvpal_1_2 (IMGINFO info, ULONG backgnd)
{
	ULONG * pal = info->Pixel;
	char  * r   = info->Palette + info->PalRpos;
	char  * g   = info->Palette + info->PalGpos;
	char  * b   = info->Palette + info->PalBpos;
	short   t   = info->Transp;
	short   n   = info->NumColors;
	do {
		*(pal++) = (!t-- ? (backgnd == G_WHITE ? 4080 : 0)
		                 : (WORD)*r *5 + (WORD)*g *9 + (WORD)*b *2);
		r += info->PalStep;
		g += info->PalStep;
		b += info->PalStep;
	} while (--n);
}

/*----------------------------------------------------------------------------*/
static void
cnvpal_4_8 (IMGINFO info, ULONG backgnd)
{
	ULONG * pal = info->Pixel;
	char  * r   = info->Palette + info->PalRpos;
	char  * g   = info->Palette + info->PalGpos;
	char  * b   = info->Palette + info->PalBpos;
	short   t   = info->Transp;
	short   n   = info->NumColors;
	do {
		*(pal++) = pixel_val[!t--
		                     ? backgnd & 0xFF
		                     : remap_color (((((long)*r <<8) | *g) <<8) | *b)];
		r += info->PalStep;
		g += info->PalStep;
		b += info->PalStep;
	} while (--n);
}

/*----------------------------------------------------------------------------*/
static void
cnvpal_15 (IMGINFO info, ULONG backgnd)
{
	ULONG * pal = info->Pixel;
	char  * r   = info->Palette + info->PalRpos;
	char  * g   = info->Palette + info->PalGpos;
	char  * b   = info->Palette + info->PalBpos;
	short   t   = info->Transp;
	short   n   = info->NumColors;
	do {
		if (!t--) {
			char * z = ((char*)&backgnd) +1;
			*(pal++) = ((short)(z[0] & 0xF8) <<7)
			         | ((short)(z[1] & 0xF8) <<2)
			         |         (z[2]         >>3);
		} else {
			*(pal++) = ((short)(*r & 0xF8) <<7)
			         | ((short)(*g & 0xF8) <<2)
			         |         (*b         >>3);
		}
		r += info->PalStep;
		g += info->PalStep;
		b += info->PalStep;
	} while (--n);
}

/*----------------------------------------------------------------------------*/
static void
cnvpal_high (IMGINFO info, ULONG backgnd)
{
	ULONG * pal = info->Pixel;
	char  * r   = info->Palette + info->PalRpos;
	char  * g   = info->Palette + info->PalGpos;
	char  * b   = info->Palette + info->PalBpos;
	short   t   = info->Transp;
	short   n   = info->NumColors;
	do {
		if (!t--) {
			char * z = ((char*)&backgnd) +1;
			*(pal++) = ((short)(z[0] & 0xF8) <<8)
			         | ((short)(z[1] & 0xFC) <<3)
			         |         (z[2]         >>3);
		} else {
			*(pal++) = ((short)(*r & 0xF8) <<8)
			         | ((short)(*g & 0xFC) <<3)
			         |         (*b         >>3);
		}
		r += info->PalStep;
		g += info->PalStep;
		b += info->PalStep;
	} while (--n);
}

/*----------------------------------------------------------------------------*/
static void
cnvpal_true (IMGINFO info, ULONG backgnd)
{
	ULONG * pal = info->Pixel;
	short   t   = info->Transp;
	short   n   = info->NumColors;
	if (info->PalStep == 4 &&
	    info->PalRpos == 1 && info->PalGpos == 2 && info->PalBpos == 3) {
		ULONG * rgb = (ULONG*)info->Palette;
		do {
			*(pal++) = (!t-- ? backgnd : *rgb);
			rgb++;
		} while (--n);
	} else {
		char  * r   = info->Palette + info->PalRpos;
		char  * g   = info->Palette + info->PalGpos;
		char  * b   = info->Palette + info->PalBpos;
		do {
			*(pal++) = (!t-- ? backgnd : ((((long)*r <<8) | *g) <<8) | *b);
			r += info->PalStep;
			g += info->PalStep;
			b += info->PalStep;
		} while (--n);
	}
}


/*----------------------------------------------------------------------------*/
static void
init_display (void)
{
	short out[273] = { -1, };
	short depth;
	BOOL  pervert;
	
	vq_scrninfo (vdi_handle, out);
	memcpy (pixel_val, out + 16, 512);
	depth   = ((UWORD)out[4] == 0x8000u ? 15 : out[2]); /* bits per pixel used */
	pervert = (out[14] & 0x80);                         /* intel crap... */
	
	sprintf (disp_info, "%c%02i%s",
	         (out[0] == 0 ? 'i' : out[0] == 2 ? 'p' : 'x'), depth,
	         (pervert ? "r" : ""));

	if (depth == 1) {                        /* monochrome */
			cnvpal_color = cnvpal_1_2;
			raster_cmap  = raster_D1;
			raster_gray  = gscale_D1;
			raster_true  = dither_D1;
	} else if (out[0] == 0) switch (depth) { /* interleaved words */
		case 2:
			cnvpal_color = cnvpal_1_2;
			raster_cmap  = raster_D2;
			raster_gray  = gscale_D2;
			raster_true  = dither_D2;
			break;
		case 4:
			cnvpal_color = cnvpal_4_8;
			raster_cmap  = raster_I4;
			raster_gray  = gscale_I4;
			raster_true  = dither_I4;
			break;
		case 8:
			cnvpal_color = cnvpal_4_8;
			raster_cmap  = raster_I8;
			raster_gray  = gscale_I8;
			raster_true  = dither_I8;
			break;
	} else if (out[0] == 2) switch (depth) { /* packed pixels */
		case  8:
			cnvpal_color = cnvpal_4_8;
			raster_cmap  = raster_P8;
			raster_gray  = gscale_P8;
			raster_true  = dither_P8;
			break;
		case 15: if (!(out[14] & 0x02)) {
			cnvpal_color = cnvpal_15;
			raster_cmap  = (pervert ? raster_15r : raster_15);
			raster_gray  = (pervert ? gscale_15r : gscale_15);
			raster_true  = (pervert ? dither_15r : dither_15);
			break;
		}
		case 16:
			cnvpal_color = cnvpal_high;
			raster_cmap  = (pervert ? raster_16r : raster_16);
			raster_gray  = (pervert ? gscale_16r : gscale_16);
			raster_true  = (pervert ? dither_16r : dither_16);
			break;
		case 24:
			cnvpal_color = cnvpal_true;
			raster_cmap  = (pervert ? raster_24r : raster_24);
			raster_gray  = (pervert ? gscale_24r : gscale_24);
			raster_true  = (pervert ? dither_24r : dither_24);
			break;
		case 32:
			cnvpal_color = cnvpal_true;
			raster_cmap  = (pervert ? raster_32r : raster_32);
			raster_gray  = (pervert ? gscale_32r : gscale_32);
			raster_true  = (pervert ? dither_32r : dither_32);
			break;
	}
	if (!raster_cmap) {                     /* standard format */
		cnvpal_color = cnvpal_4_8;
		raster_cmap  = raster_stnd;
		stnd_bitmap  = TRUE;
	}
	
	if (depth == 4 || depth == 8) {
		ULONG * dst;
		ULONG   r, g, b;
		dst = cube216;
		for (r = 0x000000uL; r <= 0xFF0000uL; r += 0x330000uL) {
			for (g = 0x000000uL; g <= 0x00FF00uL; g += 0x003300uL) {
				for (b = 0x000000uL; b <= 0x0000FFuL; b += 0x000033uL) {
					short i = remap_color (r | g | b), rgb[3];
					vq_color (vdi_handle, i, 0, rgb);
					*(dst++) = ((((((long)pixel_val[i] <<8)
					         | (((long)rgb[0] * 255 +500) / 1000)) <<8)
					         | (((long)rgb[1] * 255 +500) / 1000)) <<8)
					         | (((long)rgb[2] * 255 +500) / 1000);
				}
			}
		}
		dst = graymap;
		for (g = 0x000000uL; g <= 0xF8F8F8uL; g += 0x080808uL) {
			short i = remap_color (g | ((g >>5) & 0x030303uL)), rgb[3];
			vq_color (vdi_handle, i, 0, rgb);
			*(dst++) = ((((((long)pixel_val[i] <<8)
			         | (((long)rgb[0] * 255 +500) / 1000)) <<8)
			         | (((long)rgb[1] * 255 +500) / 1000)) <<8)
			         | (((long)rgb[2] * 255 +500) / 1000);
		}
	}
}


/*----------------------------------------------------------------------------*/
static void
read_img (IMAGE img, IMGINFO info, pIMGDATA data)
{
	BOOL (*img_rd)(IMGINFO, char * buf) = info->read;
	void (*raster)(IMGINFO, void * dst) = info->raster;
	
	short   img_h = info->ImgHeight;
	char  * buf   = info->RowBuf;
	UWORD * dst   = data->fd_addr;
	short   y     = 0;
	
	if (info->Interlace <= 0) {
		size_t scale = (info->IncYfx +1) /2;
		while (y < img_h) {
			(*img_rd)(info, buf);
			y++;
			while ((scale >>16) < y) {
				(*raster) (info, dst);
				dst   += info->LnSize;
				scale += info->IncYfx;
			}
		}
	
	} else {
		short  interlace = info->Interlace;
		BOOL   first = TRUE;
		size_t y_mul = (((size_t)img->disp_h <<16) + (img_h /2)) / img_h;
		do {
			while (y < img_h) {
				size_t scale = (size_t)y * y_mul + (info->IncYfx +1) /2;
				short  y_beg =  scale          >>16;
				short  y_end = (scale + y_mul) >>16;
				(*img_rd)(info, buf);
				y += interlace;
				if (y_beg < y_end) {
					dst = (UWORD*)data->fd_addr + info->LnSize * y_beg;
					do {
						(*raster) (info, dst);
						dst   += info->LnSize;
					} while (++y_beg < y_end);
				}
			}
			if (first) {
				first = FALSE;
			} else {
				interlace /= 2;
			}
			y = interlace /2;
		} while (interlace > 1);
	}
}


/*******************************************************************************
 *
 *   Image Decoders
 */

typedef struct s_decoder {
	struct s_decoder * Next;
	BOOL (*start)(const char * file, IMGINFO);
} DECODER;
#define DECODER_CHAIN NULL

#include "img_gif.c"
#include "img_jpg.c"
#include "img_png.c"

/*----------------------------------------------------------------------------*/
static IMGINFO
get_decoder (const char * file)
{
	static DECODER * decoder_chain = DECODER_CHAIN;
	
	DECODER * decoder = decoder_chain;
	IMGINFO   info    = (decoder ? malloc (sizeof (struct s_img_info)) : NULL);
	
	if (info) {
		memset (info, 0, offsetof (struct s_img_info, Pixel));
		while (decoder && !(*decoder->start)(file, info)) {
			decoder = decoder->Next;
		}
		if (!info->_priv_data) {
			free (info);
			info = NULL;
		}
	}
	return info;
}
