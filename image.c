/* @(#)highwire/image.c
 */
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

#define img_hash(w,h,c) (((((long)((char)c)<<12) ^ w)<<12) ^ h)

#ifdef LIBGIF
#	include <gif_lib.h>
#	undef TRUE
#	undef FALSE
	static pIMGDATA read_gif (IMAGE);
#else
#	define read_gif( i )   NULL
#endif

#ifdef LIBPNG
#	include <png.h>
	static pIMGDATA read_png (IMAGE);
#else
#	define read_png( i )   NULL
#endif

#ifdef LIBJPG
#	include <jpeglib.h>
	static pIMGDATA read_jpg (IMAGE);
#else
#	define read_jpg( i )   NULL
#endif


#ifdef LATTICE
#	define STATIC
#else
#	define STATIC static
#endif

static short pixel_val[256];


typedef struct raster_info {
	short  * PixIdx;
	long   * Palette;
	short  * DthBuf;
	short    Width;
	UWORD    PixMask;
	void   (*Rasterizer)(UWORD *, struct raster_info *, char *);
	size_t   PgSize;
	size_t   LnSize;
	size_t   IncXfx;
	size_t   IncYfx;
} RASTERINFO;

typedef void (RASTERFUNC)(UWORD *, RASTERINFO *, char *);
typedef       RASTERFUNC * RASTERIZER;
static RASTERIZER raster_color = NULL;
static BOOL       stnd_bitmap  = FALSE;
/*static BOOL       falcon_ovlay = FALSE;*/


static BOOL image_job (void *, long);


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
	
	if (!raster_color) {
		STATIC RASTERFUNC
			raster_stnd, raster_D1, raster_D2, raster_I4, raster_I8,
			raster_P8, raster_16, raster_16r, raster_24, raster_24r,
			raster_32, raster_32r;
		short out[273] = { -1, }; out[14] = 0;
		vq_scrninfo (vdi_handle, out);
		memcpy (pixel_val, out + 16, 512);
		
		if (planes == 1) {                        /* monochrome */
			raster_color = raster_D1;
		
		} else if (out[0] == 0) switch (planes) { /* interleaved words */
			case 2: raster_color = raster_D2; break;
			case 4: raster_color = raster_I4; break;
			case 8: raster_color = raster_I8; break;
		
		} else if (out[0] == 2) switch (planes) { /* packed pixels */
			case  8: raster_color = raster_P8; break;
			case 16:
				raster_color = (out[14] & 0x80 ? raster_16r : raster_16); break;
			case 24:
				raster_color = (out[14] & 0x80 ? raster_24r : raster_24); break;
			case 32:
				raster_color = (out[14] & 0x80 ? raster_32r : raster_32); break;
		}
		if (!raster_color) {                     /* standard format */
			raster_color = raster_stnd;
			stnd_bitmap  = TRUE;
		}
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
		sched_insert (image_job, img, 0);
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
			sched_remove (image_job, img);
		}
		free_location (&img->source);
		free (img);
		*_img = NULL;
	}
}


/*----------------------------------------------------------------------------*/
static void
img_scale (IMAGE img, short img_w, short img_h, RASTERINFO * info)
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
				data = cache_lookup (img->source, 0, &hash);
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
				sched_insert (image_job, img, 0);
			}
		}
	
	} else if (!img->u.Data && (!img->set_w || !img->set_h)) {
		long     hash = 0;
		cIMGDATA data = cache_lookup (img->source, 0, &hash);
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


/*----------------------------------------------------------------------------*/
static BOOL
image_job (void * arg, long ignore)
{
	IMAGE    img = arg;
	PARAGRPH par = img->paragraph;
	FRAME frame = img->frame;
	GRECT rec   = frame->Container->Area, * clip = &rec;
	short old_w = img->disp_w;
	short old_h = img->disp_h;
	int calc_xy = 0;
	
	long   hash   = (img->set_w && img->set_h
	                 ? img_hash (img->disp_w, img->disp_h, img->backgnd) : 0);
	CACHED cached = cache_lookup (img->source, hash, (hash ? & hash : NULL));
	
	(void)ignore; /* not used here */
	
	if (cached) {
		if ((char)(hash >>24) == 0xFF) {
			img->backgnd = -1;
		}
		if (hash != img_hash (img->disp_w, img->disp_h, img->backgnd)) {
			cached = NULL;
		}
	}	
	if (cached) {
		img->u.Data = cache_bound (cached, &img->source);
	
	} else {
		pIMGDATA data;
		
		containr_notify (frame->Container, HW_ImgBegLoad, img->source->FullName);
		
		if ((data = read_gif (img)) != NULL ||
		    (data = read_png (img)) != NULL || (data = read_jpg (img)) != NULL) {
			
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
	containr_notify (frame->Container, HW_ImgEndLoad, clip);
	
	return FALSE;
}


/*----------------------------------------------------------------------------*/
static pIMGDATA
setup (IMAGE img, short depth, short img_w, short img_h, RASTERINFO * info)
{
	static   RASTERFUNC raster_mono;
	short    n_planes = (depth > 1 ? planes : 1);
	size_t   wd_width;
	size_t   pg_size;
	size_t   mem_size;
	pIMGDATA data;
	
	img_scale (img, img_w, img_h, info);
	
	wd_width = (img->disp_w + 15) / 16;
	pg_size  = wd_width * img->disp_h;
	mem_size = pg_size *2 * n_planes;
	data     = malloc (sizeof (struct s_img_data) + mem_size);
	data->mem_size   = mem_size;
	data->img_w      = img_w;
	data->img_h      = img_h;
	data->fd_addr    = (data +1);
	data->fd_w       = img->disp_w;
	data->fd_h       = img->disp_h;
	data->fd_wdwidth = wd_width;
	data->fd_stand   = (depth > 1 ? stnd_bitmap : FALSE);
	data->fd_nplanes = n_planes;
	data->fd_r1 = data->fd_r2 = data->fd_r3 = 0;
	
	info->Width   = img->disp_w;
	info->PixMask = (1 << depth) -1;
	info->PgSize  = pg_size;
	info->LnSize  = wd_width;
	if (!data->fd_stand) {
		info->LnSize *= n_planes;
	}
	if (planes <= 2 && depth > 1) {
		info->DthBuf = malloc ((img->disp_w +1) *2);
		memset (info->DthBuf, 0, (img->disp_w *2));
	}
	info->Rasterizer = (depth > 1 ? raster_color : raster_mono);
	
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
STATIC void
raster_mono (UWORD * dst, RASTERINFO * info, char * src)
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
		:                                                      /* output */
		: "a"(dst),"a"(src),"d"(info->Width),"d"(info->IncXfx) /* input  */
		/*    %0       %1       %2               %3     */
		: "d0","d1","d2"                                    /* clobbered */
	);
	
#elif defined(__PUREC__)
	extern void pc_raster_mono (void *, void *, short, size_t);
	pc_raster_mono (dst, src, info->Width, info->IncXfx);
	
#else
	short  width = info->Width;
	size_t x     = (info->IncXfx +1) /2;
	UWORD  pixel = 0x8000;
	UWORD  chunk = 0;
	do {
		if (src[x >>16] & 1) chunk |= pixel;
		
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
STATIC void
raster_stnd (UWORD * dst, RASTERINFO * info, char * src)
{
	short * map   = info->PixIdx;
	UWORD   mask  = info->PixMask;
	short   width = info->Width;
	size_t  x     = (info->IncXfx +1) /2;
	UWORD   pixel = 0x8000;
	do {
		short   color = map[(UWORD)src[x >>16] & mask];
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
STATIC void
raster_D1 (UWORD * dst, RASTERINFO * info, char * src)
{
	short * buf   = info->DthBuf;
	short * pix   = info->PixIdx;
	short   ins   = 0;
	UWORD   mask  = info->PixMask;
	short   width = info->Width;
	size_t  x     = (info->IncXfx +1) /2;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	do {
		ins += *buf + pix[(short)src[x >>16] & mask];
		
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
 * 2 planes interleaved words format, uses a simple intensity dither.
 */
STATIC void
raster_D2 (UWORD * dst, RASTERINFO * info, char * src)
{
	short * buf   = info->DthBuf;
	long  * pal   = info->Palette;
	short   ins   = *buf;
	UWORD   mask  = info->PixMask;
	short   width = info->Width;
	size_t  x     = (info->IncXfx +1) /2;
	UWORD   pixel = 0x8000;
	UWORD   chunk = 0;
	*buf = 0;
	do {
		char * rgb = (char*)&pal[(short)src[x >>16] & mask];
		short  err;
		ins += (short)rgb[0] *5 + (short)rgb[1] *9 + (short)rgb[2] *2;
		
		if (ins < 2040) {
			chunk |= pixel;
			if ((err = ins) < 0) {
	/*			err =0;//>>= 1;*/
			}
		} else {
			if ((err = ins - 4080) > 0) {
	/*			err =0;//>>= 1;*/
			}
		}
		ins = buf[1];
		
	/*	if ((err >>= 2) != 0) {*/
	/*		buf[1] =  err;*/     /* 2/8 to next RGB of next line */
		if ((err >>= 1) != 0) {
			buf[1] =  0;     /* 2/8 to next RGB of next line */
	/*		err    += err >>1;*/
			buf[0] += err;     /* 3/8 to this RGB of next line */
			ins    += err;     /* 3/8 to next RGB of this line */
		} else {
			buf[1] = 0;
		}
		buf++;
		
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
 * 4 planes interleaved words format
 */
STATIC void
raster_I4 (UWORD * dst, RASTERINFO * info, char * src)
{
	short * map   = info->PixIdx;
	UWORD   mask  = info->PixMask;
	short   width = info->Width;
	size_t  x     = (info->IncXfx +1) /2;
	UWORD   pixel = 0x8000;
	do {
		short   color = map[(short)src[x >>16] & mask];
		UWORD * chunk = dst;
		short   i     = 4;
		do {
			if (color & 1) *chunk |=  pixel;
			else           *chunk &= ~pixel;
			chunk++;
			color >>= 1;
		} while (--i);
		
		if (!(pixel >>= 1)) {
			pixel = 0x8000;
			dst += 4;
		}
		x += info->IncXfx;
		
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 8 planes interleaved words format
 */
STATIC void
raster_I8 (UWORD * dst, RASTERINFO * info, char * src)
{
#if defined(__GNUC__)
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
		lsl.w		#1, d3
		move.w	0(%2,d3.w), %4 | pixel value
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
		: "a"(dst),"a"(src),"a"(info->PixIdx),"a"(&x),
		/*    %0       %1       %2                %3 */
		  "d"(info->Width),"d"(info->IncXfx),"d"(info->PixMask)
		/*    %4               %5                %6          */
		: "d3","d4","d5","d6","d7"
	);
	
#elif defined(__PUREC__)
	extern void pc_raster_I8 (void *, void *, void *, short, size_t, UWORD);
	pc_raster_I8 (dst, src, info->PixIdx,
	              info->Width, info->IncXfx, info->PixMask);
	
#else
	short * map   = info->PixIdx;
	UWORD   mask  = info->PixMask;
	short   width = info->Width;
	size_t  x     = (info->IncXfx +1) /2;
	UWORD   pixel = 0x8000;
	do {
		short   color = map[(short)src[x >>16] & mask];
		UWORD * chunk = dst;
		short   i     = 8;
		do {
			if (color & 1) *chunk |=  pixel;
			else           *chunk &= ~pixel;
			chunk++;
			color >>= 1;
		} while (--i);
		
		if (!(pixel >>= 1)) {
			pixel = 0x8000;
			dst += 8;
		}
		x += info->IncXfx;
		 
	} while (--width);
#endif
}

/*------------------------------------------------------------------------------
 * 8 planes packed pixels formart.
 */
STATIC void
raster_P8 (UWORD * _dst, RASTERINFO * info, char * src)
{
	char  * dst   = (char*)_dst;
	short * pix   = info->PixIdx;
	UWORD   mask  = info->PixMask;
	size_t  x     = (info->IncXfx +1) /2;
	short   width = info->Width;
	do {
		*(dst++) = pix[(short)src[x >>16] & mask];
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 16 planes packed pixels formart, the pixel index array must contain 16bit RGB
 * values.
 */
STATIC void
raster_16 (UWORD * dst, RASTERINFO * info, char * src)
{
	short * pix   = info->PixIdx;
	UWORD   mask  = info->PixMask;
	size_t  x     = (info->IncXfx +1) /2;
	short   width = info->Width;
	do {
		*(dst++) = pix[(short)src[x >>16] & mask];
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 16 planes packed pixels formart in wrong (intel) byte order, the pixel index
 * array must contain 16bit RGB values.
 */
STATIC void
raster_16r (UWORD * dst, RASTERINFO * info, char * src)
{
	short * pix   = info->PixIdx;
	UWORD   mask  = info->PixMask;
	size_t  x     = (info->IncXfx +1) /2;
	short   width = info->Width;
	do {
		UWORD rgb = pix[(short)src[x >>16] & mask];
		*(dst++) = (rgb >> 8) | (rgb << 8);
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 24 planes packed pixels formart.
 */
STATIC void
raster_24 (UWORD * _dst, RASTERINFO * info, char * src)
{
	char * dst   = (char*)_dst;
	UWORD  mask  = info->PixMask;
	size_t x     = (info->IncXfx +1) /2;
	short  width = info->Width;
#if defined(LATTICE) && defined(LIBGIF)
	GifColorType * pal = (GifColorType*)info->Palette;
#else
	long         * pal = info->Palette;
#endif
	do {
		char * rgb = (char*)&pal[(short)src[x >>16] & mask];
		*(dst++) = *(rgb++);
		*(dst++) = *(rgb++);
		*(dst++) = *(rgb);
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 24 planes packed pixels formart in wrong (intel) byte order.
 */
STATIC void
raster_24r (UWORD * _dst, RASTERINFO * info, char * src)
{
	char * dst   = (char*)_dst;
	UWORD  mask  = info->PixMask;
	size_t x     = (info->IncXfx +1) /2;
	short  width = info->Width;
#if defined(LATTICE) && defined(LIBGIF)
	GifColorType * pal = (GifColorType*)info->Palette;
#else
	long         * pal = info->Palette;
#endif
	do {
		char * rgb = (char*)&pal[(short)src[x >>16] & mask];
		*(dst++) = rgb[2];
		*(dst++) = rgb[1];
		*(dst++) = rgb[0];
		x += info->IncXfx;
	} while (--width);
}

/*------------------------------------------------------------------------------
 * 32 planes packed pixels formart.
 */
STATIC void
raster_32 (UWORD * _dst, RASTERINFO * info, char * src)
{
	long * dst   = (long*)_dst;
	UWORD  mask  = info->PixMask;
	size_t x     = (info->IncXfx +1) /2;
	short  width = info->Width;
#if defined(LATTICE) && defined(LIBGIF)
	GifColorType * pal = (GifColorType*)info->Palette;
	do {
		GifColorType * rgb = &pal[src[x >>16] & mask];
		*(dst++) = ((((rgb->Red <<8) | rgb->Green) <<8) | rgb->Blue);
		x += info->IncXfx;
	} while (--width);
#else
	long         * pal = info->Palette;
	do {
		*(dst++) = pal[(short)src[x >>16] & mask] >>8;
		x += info->IncXfx;
	} while (--width);
#endif
}

/*------------------------------------------------------------------------------
 * 32 planes packed pixels formart in wrong (intel) byte order.
 */
STATIC void
raster_32r (UWORD * _dst, RASTERINFO * info, char * src)
{
	long * dst   = (long*)_dst;
	UWORD  mask  = info->PixMask;
	size_t x     = (info->IncXfx +1) /2;
	short  width = info->Width;
	GifColorType * pal = (GifColorType*)info->Palette;
	do {
		GifColorType * rgb = &pal[src[x >>16] & mask];
		*(dst++) = (((((long)rgb->Blue <<8) | rgb->Green) <<8) | rgb->Red) <<8;
		x += info->IncXfx;
	} while (--width);
}


/*******************************************************************************
 *
 *   Image Decoders
 */

/*============================================================================*/
#ifdef LIBGIF
static pIMGDATA
read_gif (IMAGE img)
{
	pIMGDATA   data      = NULL;
	RASTERINFO info      = { NULL, NULL, NULL, };
	short      interlace = 0;
	short      transpar  = -1;
	short      img_w     = img->disp_w;
	short      img_h     = img->disp_h;
	short      fgnd      = G_BLACK;
	short      bgnd      = G_WHITE;
	ColorMapObject * map = NULL;
	GifFileType    * gif = DGifOpenFileName (img->source->FullName);
	if (gif) {
		GifRecordType    rec;
		do {
			if (DGifGetRecordType (gif, &rec) == GIF_ERROR) {
				printf ("DGifGetRecordType() ");
				PrintGifError();
				break;
			
			} else if (rec == IMAGE_DESC_RECORD_TYPE) {
				if (DGifGetImageDesc(gif) == GIF_ERROR) {
					printf ("DGifGetImageDesc() ");
					PrintGifError();
					break;
				
				} else {
					GifImageDesc * dsc = &gif->Image;
					map = (dsc->ColorMap ? dsc->ColorMap : gif->SColorMap);
					img_w = dsc->Width;
					img_h = dsc->Height;
					if (dsc->Interlace) {
						interlace = 8;
					}
				}
				break;
			
			} else if (rec == EXTENSION_RECORD_TYPE) {
				int           code;
				GifByteType * block;
				if (DGifGetExtension (gif, &code, &block) == GIF_ERROR) {
					printf ("DGifGetExtension() ");
					PrintGifError();
					break;
				
				} else while (block != NULL) {
					if (code == 0xF9 && (block[1] & 1)) {
						transpar = (short)block[4];
					}
					if (DGifGetExtensionNext (gif, &block) == GIF_ERROR) {
						printf ("DGifGetExtensionNext() ");
						PrintGifError();
						break;
					}
				}
			
			} else {
				printf ("other: %i \n", rec);
				break;
			}
		} while (rec != TERMINATE_RECORD_TYPE);
	}
	
	if (transpar < 0) {
		img->backgnd = -1;
	}
	
	if (map) {
		info.Palette = (long*)map->Colors;
		
		if (gif->SColorMap->BitsPerPixel == 1) {
			bgnd = (transpar == 0 ? img->backgnd 
			                      : remap_color (info.Palette[0] >>8));
			fgnd = (transpar == 1 ? img->backgnd
			                      : remap_color (info.Palette[1] >>8));
		
		} else if (planes < 24) {
			short          n   = map->ColorCount;
			short        * dst = malloc (sizeof(short) * n);
			GifColorType * src = map->Colors;
			info.PixIdx = dst;
			
			if (planes <= 2) {
				do {
					if (!transpar--) {
						*(dst++) = (img->backgnd == G_WHITE ? 4080 : 0);
					} else {
						*(dst++) = (short)src->Red   *5
						         + (short)src->Green *9
						         + (short)src->Blue  *2;
					}
					src++;
				} while (--n);
			
			} else if (planes == 16) {
				do {
					if (!transpar--) {
						short rgb[3];
						vq_color (vdi_handle, img->backgnd, 0, rgb);
						*(dst++) = (((((long)rgb[0] * 255 +500) / 1000) & 0xF8) <<8)
						         | (((((long)rgb[1] * 255 +500) / 1000) & 0xFC) <<3)
						         |  ((((long)rgb[2] * 255 +500) / 1000)         >>3);
					} else {
						*(dst++) = (((short)src->Red   & 0xF8) <<8)
						         | (((short)src->Green & 0xFC) <<3)
						         |         (src->Blue          >>3);
					}
					src++;
				} while (--n);
			
			} else {
				do {
					if (!transpar--) {
						*(dst++) = pixel_val[img->backgnd];
					} else {
						long rgb = ((((long)src->Red <<8) | src->Green) <<8)
						         | src->Blue;
						*(dst++) = pixel_val[remap_color (rgb) & 0xFF];
					}
					src++;
				} while (--n);
			}
		
		} else if (transpar >= 0) {
			GifColorType * pal = &map->Colors[transpar];
			short rgb[3];
			vq_color (vdi_handle, img->backgnd, 0, rgb);
			pal->Red   = ((long)rgb[0] * 255 +500) / 1000;
			pal->Green = ((long)rgb[1] * 255 +500) / 1000;
			pal->Blue  = ((long)rgb[2] * 255 +500) / 1000;
		}
		data = setup (img, map->BitsPerPixel, img_w, img_h, &info);
	}
	
	if (data) {
		RASTERIZER raster = info.Rasterizer;
		char     * buf    = malloc (img_w);
		UWORD    * dst    = data->fd_addr;
		short      y      = 0;
		
		data->fgnd = fgnd;
		data->bgnd = bgnd;
		
		if (!interlace) {
			size_t scale = (info.IncYfx +1) /2;
			while (y < img_h) {
				DGifGetLine (gif, buf, img_w);
				y++;
				while ((scale >>16) < y) {
					(*raster) (dst, &info, buf);
					dst   += info.LnSize;
					scale += info.IncYfx;
				}
			}
		
		} else {
			BOOL   first = TRUE;
			size_t y_mul = (((size_t)img->disp_h <<16) + (img_h /2)) / img_h;
			do {
				while (y < img_h) {
					size_t scale = (size_t)y * y_mul + (info.IncYfx +1) /2;
					short  y_beg =  scale          >>16;
					short  y_end = (scale + y_mul) >>16;
					DGifGetLine (gif, buf, img_w);
					y += interlace;
					if (y_beg < y_end) {
						dst = (UWORD*)data->fd_addr + info.LnSize * y_beg;
						do {
							(*raster) (dst, &info, buf);
							dst   += info.LnSize;
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
		free (buf);
	}
	if (info.PixIdx) free (info.PixIdx);
	if (info.DthBuf) free (info.DthBuf);
	if (gif) DGifCloseFile (gif);

	return data;
}

#endif /* LIBGIF */
