/* @(#)highwire/image.c
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h> /* memcpy() */
#include <gemx.h>

#ifdef __PUREC__
# define CONTAINR struct s_containr *
#endif

#include "global.h"
#include "image_P.h"
#include "Location.h"
#include "Containr.h"
#include "schedule.h"
#include "cache.h"
#include "Loader.h"


static pIMGDATA setup       (IMAGE, IMGINFO);
static void     read_img    (IMAGE, IMGINFO, pIMGDATA);
static int      image_job   (void *, long);

#define img_hash(w,h,c) (((((long)((char)c)<<12) ^ w)<<12) ^ h)


/*============================================================================*/
const char *
image_dispinfo(void)
{
	return rasterizer(0,0)->DispInfo;
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
           short w, short h, short vspace, short hspace, BOOL win_image)
{
	LOCATION loc     = new_location (file, base);
	BOOL     blocked = ((loc->Flags & (1uL << ('I' - 'A'))) != 0);
	IMAGE    img     = malloc (sizeof(struct s_image));
	long     hash;
	CACHED   cached;
	
	img->vspace = vspace;
	img->hspace = hspace;

	img->set_w = w;
	img->set_h = h;
	
	img->source    = loc;
	img->u.Data    = NULL;
	img->frame     = frame;
	img->paragraph = current->paragraph;
	img->word      = current->word;
	img->map       = NULL;
	img->backgnd   = current->backgnd;
	img->alt_w     = 0;
	img->alt_h     = img->word->word_height;
	img->offset.Origin = &img->paragraph->Box;
	img->word->image = img;
	
	hash   = img_hash ((w > 0 ? w : 0), (h > 0 ? h : 0), -1);
	cached = (blocked || !cfg_ViewImages
	          ? NULL : cache_lookup (loc, hash, &hash));
	
	if (cached) {
		short bgnd = (hash >>24) & 0xFF;
		if (bgnd == 0xFF) {
			img->backgnd = bgnd = -1;  /* no transparency */
		}
		if (w > 0 && h > 0 && hash != img_hash (w, h, bgnd)) {
			cached = NULL;  /* absolute size doesn't match */
		} else {
			hash = bgnd;
		}
	}
	if (cached) {
		cIMGDATA data = cached;
		if (!h) {
/*			h = img->set_h = data->img_h; */
			h = data->img_h;

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
/*				w = img->set_w = data->img_w; */
				w = data->img_w;

			}
			if (h < 0) {
/*				h = img->set_h = ((long)data->img_h * -h +512) /1024; */
				h = ((long)data->img_h * -h +512) /1024;
			}
			if (w != data->fd_w || h != data->fd_h || hash != img->backgnd) {
				cached = cache_lookup (loc, img_hash (w, h, img->backgnd), NULL);
			}
		}
		if (cached) {
			img->u.Data = cache_bound (cached, &img->source);
		}
	
	} else if (hash) {
/*		if (!w) w = img->set_w = (hash >>12) & 0x0FFF;
		if (!h) h = img->set_h = (hash     ) & 0x0FFF; */
		if (!w) w = (hash >>12) & 0x0FFF;
		if (!h) h = (hash     ) & 0x0FFF;

	}
	
	if (blocked) {
		img->disp_w = (w > 0 ? w : 1);
		img->disp_h = (h > 0 ? h : 1);
		set_word (img);
	
	} else {
		img->disp_w = (w > 0 ? w : 16);
		img->disp_h = (h > 0 ? h : 16);
		set_word (img);
		
		if (!img->u.Data && (cfg_ViewImages || win_image)) {
			if (sched_insert (image_job, img, (long)img->frame->Container, 1)) {
				containr_notify (img->frame->Container, HW_ActivityBeg, NULL);
			}
		}
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
void reload_image(IMAGE * _img)
{
	IMAGE img = *_img;
	CACHED cached;
	
	/* Free memory of the image */
	if (img->u.Data) {
		CACHEOBJ cob = cache_release ((CACHED*)&img->u.Data, TRUE);
		if (cob) {
			free (cob);
		} 
		else if (img->u.Mfdb) {
			free (img->u.Mfdb);
			img->u.Mfdb = NULL;
		}
	}
	/* remove cached to ensure freshness: */
	/* maybe some elements are stored twice in cache */ 
	/* one time in memory, one time on disk	*/
	/* I just assume that! */
	cached = cache_lookup( img->source, 0, NULL);
	while( cached ) { 
		cache_clear( cached );
		cached = cache_lookup( img->source, 0, NULL);
	}
	
	if (sched_insert (image_job, img, (long)img->frame->Container, 1)) {
		containr_notify (img->frame->Container, HW_ActivityBeg, NULL);
	}
}

/*----------------------------------------------------------------------------*/
static void
img_scale (IMAGE img, short img_w, short img_h, IMGINFO info)
{
	size_t  scale_x = 0x10000uL; /* to avoid a  */
	size_t  scale_y = 0x10000uL; /* gcc warning */

	/* calculate scaling steps (32bit fix point) */
	
	if (!img->set_w && !img->set_h) { /* neither width nor height */	
		scale_x     = scale_y = 0x10000uL;
	} else {
		if (img->set_w  && !img->set_h) { /* only width */
			if (img_w != img->disp_w) {
				scale_x     = (((size_t)img_w <<16) + (img->disp_w /2) ) / img->disp_w;
				scale_y	= scale_x;

			} else {
				scale_y= scale_x     = 0x10000uL;
			}


		} else {
		if (!img->set_w && img->set_h) { /* only height */
			if (img->set_h < 0) {
				scale_x=  scale_y = (scale_x * 1024 +(-img->set_h /2) ) / -img->set_h;
			} else if (img_h != img->disp_h) {
				scale_y     = (((size_t)img_h <<16) + (img->disp_h /2) ) / img->disp_h;
				scale_x = scale_y;
			} else {
				scale_x= scale_y     = 0x10000uL;
			}
		} else {
			if (img_w != img->disp_w) { /* width and height */
				scale_x     = (((size_t)img_w <<16) + (img->disp_w /2) ) / img->disp_w;
			}
			if (img->set_h < 0) {
				scale_y     = (scale_x * 1024 +(-img->set_h /2) ) / -img->set_h;
			} else if (img_h != img->disp_h) {
				scale_y     = (((size_t)img_h <<16) + (img->disp_h /2) ) / img->disp_h;
				}
		        
			}
		}
	}
	if (scale_x != 0x10000uL) {
		img->disp_w = (((size_t)img_w <<16 )  + (scale_x / 2) ) / scale_x;
	} else {
		img->disp_w = img_w;
	}
	if (scale_y != 0x10000uL) {
		img->disp_h = (((size_t)img_h <<16 ) +  (scale_y / 2) ) / scale_y;
	} else {
		img->disp_h = img_h;
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
	if (img->set_w < 0 ) {
		short width = ((long)par_width * -img->set_w +512) /1024 - img->hspace *2;

		if (width <= 0) width = 1;
		if (/*img->disp_w != width || */ !img->u.Data) {
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
				if (sched_insert (image_job, img, (long)img->frame->Container, 1)) {
					containr_notify (img->frame->Container, HW_ActivityBeg, NULL);
				}
			}
		}
	
	} else if (!img->u.Data && (!img->set_w || img->set_h <= 0)) {
		long     hash = 0;
		cIMGDATA data = cache_lookup (img->source, -1, &hash);
		if (data) {
			if ((char)(hash >>24) == 0xFF) {
				img->backgnd = -1;
			}
			img_scale (img, data->img_w, data->img_h, NULL);
			set_word (img);
		}
		
	} else if (img->word->vertical_align == ALN_TOP) {
		set_word (img);
	} 
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
image_ldr (void * arg, long invalidated)
{
	LOADER loader = arg;
	IMAGE  img    = loader->FreeArg;
	
	if (!invalidated && (!loader->Cached || loader->Error)) {
	/*	if (!loader->Cached || loader->Error <= 0) {
			char buf[1024];
			location_FullName (loader->Location, buf, sizeof(buf));
			printf ("image_ldr(%s): load error %i.\n", buf, loader->Error);
		}
	*/
		invalidated = TRUE;
	
	} else if (loader->Location != img->source) {
		free_location (&img->source);
		img->source = location_share (loader->Location);
	}
	if (MIME_Major (img->frame->MimeType) == MIME_IMAGE) {
		free_location (&img->frame->Location);
		img->frame->Location = location_share (img->source);
	}
	delete_loader (&loader);
	
/*	sched_insert (image_job, img, invalidated, 1);*/
	image_job (img, invalidated);
	
	return FALSE;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int
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
	BOOL   fresh = FALSE;
	
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
		if (!cached) {
			if (res & CR_LOCAL) {
				loc   = info.Local;
				fresh = (info.Ident == 0);
			
			} else if (res & CR_BUSY) {
				return TRUE;
			
			} else if (PROTO_isRemote (loc->Proto)) {
				LOADER ldr = start_objc_load (frame->Container,
				                              NULL, loc, image_ldr, img);
				if (ldr) {
					if (PROTO_isRemote (frame->Location->Proto)) {
						ldr->Referer = location_share (frame->Location);
					}
				}
				return FALSE;
			}
		}
	}	
	if (cached) {
		img->u.Data = cache_bound (cached, &img->source);
	
	} else {
		pIMGDATA data = NULL;
		IMGINFO  info;
		
		containr_notify (frame->Container, HW_ImgBegLoad, img->source->FullName);
		
		if ((info = get_decoder (loc->FullName)) != NULL) {
			if ((data = setup (img, info))        != NULL) {
				read_img (img, info, data);
			}
			(*info->quit)(info);
			if (info->RowMem) free (info->RowMem);
			if (info->DthBuf) free (info->DthBuf);
			free (info);
		}
		if (data) {
			long ident = (fresh ? img_hash (data->img_w, data->img_h, 0) : 0);
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
			img->u.Data = cache_insert (img->source, hash, ident,
			                            (CACHEOBJ*)&data, data->mem_size, free);
		} else {
			clip = NULL;
		}
	}
	
	if ((img->set_w >= 0 && par->Box.MinWidth < img->word->word_width)
	    || img->disp_w != old_w || img->disp_h != old_h) {
/*		long par_x = par->Box.Rect.X;*/
/*		long par_y = par->Box.Rect.Y;*/
		long off_y = img->offset.Y;
		long par_w = par->Box.Rect.W;
		long par_h = par->Box.Rect.H;
		if (par->Box.MinWidth < img->disp_w) {
			 par->Box.MinWidth = img->disp_w;
			 par->Box.MaxWidth = 0;
		}
		dombox_MinWidth (&frame->Page);
		
	/*	if (containr_calculate (frame->Container, NULL)) {
			calc_xy = 0;
		} else if (par_w == par->Box.Rect.W && par_h == par->Box.Rect.H) {
			calc_xy = 1;
			rec.g_w = par_w;
			rec.g_h = par_h;
		} else if (par_x == par->Box.Rect.X && par_y == par->Box.Rect.Y) {
			calc_xy = -1;
	*/
		if (containr_calculate (frame->Container, NULL)
		    && par_w == par->Box.Rect.W) {
			long x, y;
			dombox_Offset (img->offset.Origin, &x, &y);
			x += frame->clip.g_x - frame->h_bar.scroll;
			y += frame->clip.g_y - frame->v_bar.scroll;
			if (off_y == img->offset.Y) {
				y    += off_y;
			} else {
				off_y = 0;
			}
			rec.g_y = y;
			if (par_h == par->Box.Rect.H) {
				rec.g_x = x;
				rec.g_w = par_w;
				rec.g_h = par_h - off_y;
			}
		}
	
	} else if (img->u.Data) {
		calc_xy = 1;
		rec.g_w = img->disp_w + img->hspace;
		rec.g_h = img->disp_h + img->vspace;
	}

	if (calc_xy) {
		DOMBOX * box = img->offset.Origin;
		short x = img->offset.X + frame->clip.g_x - frame->h_bar.scroll;
		short y = img->offset.Y + frame->clip.g_y - frame->v_bar.scroll;
		while (box) {
			x  += box->Rect.X;
			y  += box->Rect.Y;
			box = box->Parent;
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
	ULONG    transpar = (info->Transp < 0 ? (img->backgnd = -1) : img->backgnd);
	RASTERIZER raster = rasterizer (info->BitDepth,
	                                (info->Palette ? 0 : info->NumComps));
	pIMGDATA data;
	
	img_scale (img, info->ImgWidth, info->ImgHeight, info);
	
	wd_width = (img->disp_w + 31) / 16;
	pg_size  = wd_width * img->disp_h;
	mem_size = pg_size *2 * n_planes;
	if ((data = malloc (sizeof (struct s_img_data) + mem_size)) == NULL) {
		return NULL;
	}
	data->mem_size   = mem_size;
	data->img_w      = info->ImgWidth;
	data->img_h      = info->ImgHeight;
	data->fd_addr    = (data +1);
	data->fd_w       = img->disp_w;
	data->fd_h       = img->disp_h;
	data->fd_wdwidth = wd_width;
	data->fd_stand   = (info->BitDepth > 1 ? raster->StndBitmap : FALSE);
	data->fd_nplanes = n_planes;
	data->fd_r1 = data->fd_r2 = data->fd_r3 = 0;
	
	if (info->RowBytes) {
		size_t psize = sizeof(void*) * info->ImgHeight;
		info->RowMem = malloc (psize +
		                       info->RowBytes * info->ImgHeight + info->NumComps);
		info->RowBuf = (char*)info->RowMem + psize;
	} else {
		info->RowMem = malloc ((info->ImgWidth +1) * info->NumComps);
		info->RowBuf = info->RowMem;
	}
	if (!info->RowMem) {
		free (data);
		return NULL;
	}
	
	if (info->BitDepth > 1 && planes <= 8) {
		size_t size = (img->disp_w +1) *3;
		if ((info->DthBuf = malloc (size)) == NULL) {
			free (data);
			return NULL;
		}
		memset (info->DthBuf, 0, size);
	}
	
	info->DthWidth = img->disp_w;
	info->PixMask  = (1 << info->BitDepth) -1;
	info->PgSize   = pg_size;
	info->LnSize   = wd_width;
	if (!data->fd_stand) {
		info->LnSize *= n_planes;
	}
	
	if (info->BitDepth > 1) {
		if (info->Palette) {
			(*raster->cnvpal)(info, transpar);
		}
		data->bgnd = G_WHITE;
		data->fgnd = G_BLACK;
	} else {
		(*raster->cnvpal) (info, transpar);
		data->bgnd = info->Pixel[0];
		data->fgnd = info->Pixel[1];
	}
	info->raster = raster->functn;
	
/*	printf ("scale: [%i,%i] %lX.%04lX/%lX.%04lX [%i,%i] \n", img_w, img_h,
	        scale_x >>16, scale_x & 0xFFFF, scale_y >>16, scale_y & 0xFFFF,
	        img->disp_w, img->disp_h);
*/
	return data;
}


/*----------------------------------------------------------------------------*/
static BOOL
skip_corrupted (IMGINFO info, char * buf)
{
	memset (buf, 0x99, info->ImgWidth * info->NumComps);
	return TRUE;
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
		short  y_dst = img->disp_h;
		while (y < img_h) {
			if (!(*img_rd)(info, buf)) {
				img_rd = skip_corrupted;
			}
			y++;
			while ((scale >>16) < y) {
				(*raster) (info, dst);
				dst   += info->LnSize;
				scale += info->IncYfx;
				if (!--y_dst) {
					scale = (long)img_h <<16; /* don't write below target bottom */
					break;
				}
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
				if (y_end > img->disp_h) {
					y_end = img->disp_h;
					if (y_end > 0 && y_end <= y_beg) {
						y_beg = y_end -1;
					}
				}
				if (!(*img_rd)(info, buf)) {
					img_rd = skip_corrupted;
				}
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
