/* @(#)highwire/image.c
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h> /* memcpy() */
#include <time.h>
#include <gemx.h>

#ifndef CLK_TCK
#   define CLK_TCK     CLOCKS_PER_SEC
#endif

#ifdef __PUREC__
# define CONTAINR struct s_containr *
#endif

#include "global.h"
#ifdef __GNUC__
# include <osbind.h>
#endif
#include "Logging.h"
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

/*----------------------------------------------------------------------------*/
/* scale v by the 16.16 factor, rounded, at least 1 */
static short
squash (short v, ULONG ffx)
{
	v = (short)(((ULONG)v * ffx + 0x8000uL) >>16);
	return (v > 0 ? v : 1);
}

/* Screen shape and squash factor are constant, so decide them once. */
static WORD  aspect_axis = -1;
static ULONG aspect_ffx;

static void
aspect_setup (void)
{
	if (aspect_axis < 0) {
		WORD wp = vdi_dev.wpixel;
		WORD hp = vdi_dev.hpixel;
		aspect_axis = 0;
		if (cfg_ImgAspect && wp > 0 && hp > 0) {
			if (hp >= wp + wp /2) {
				aspect_axis = 'h';
				aspect_ffx  = (((ULONG)wp <<16) + hp /2) / hp;
			} else if (wp >= hp + hp /2) {
				aspect_axis = 'w';
				aspect_ffx  = (((ULONG)hp <<16) + wp /2) / wp;
			}
		}
	}
}

/*============================================================================*/
/* The shape of one screen pixel, for a transcoding proxy to size images with.
 * Reports what we will really do, not what the hardware says: 1:1 whenever no
 * correction would be applied, so the far end needs no policy of its own.
 * Sharing aspect_setup() with aspect_adjust() keeps the two from drifting --
 * a disagreement here would show up as subtly wrong geometry rather than as a
 * failure.
*/
void
image_AspectRatio (WORD * wpx, WORD * hpx)
{
	aspect_setup();

	if (aspect_axis) {
		*wpx = vdi_dev.wpixel;
		*hpx = vdi_dev.hpixel;
	} else {
		*wpx = *hpx = 1;
	}
}


/*----------------------------------------------------------------------------*/
/* Where screen pixels are far from square, squash the display size's long
 * axis by the reported pixel size so pictures keep their real world aspect
 * ratio: the height on tall pixels (ST medium), the width on wide ones
 * (TT low).
 */
static void
aspect_adjust (IMAGE img)
{
	aspect_setup();

	if (aspect_axis == 'h') {
		img->disp_h = squash (img->disp_h, aspect_ffx);
	} else if (aspect_axis == 'w') {
		img->disp_w = squash (img->disp_w, aspect_ffx);
	}
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
	
	{
		short fb = (blocked ? 1 : 16); /* fallback for unsized images */
		img->disp_w = (w > 0 ? w : fb);
		img->disp_h = (h > 0 ? h : fb);
	}
	aspect_adjust (img); /* so the placeholder matches the later rendering */
	set_word (img);

	if (!blocked && !img->u.Data && (cfg_ViewImages || win_image)) {
		if (sched_insert (image_job, img, (long)img->frame->Container, 1)) {
			containr_notify (img->frame->Container, HW_ActivityBeg, NULL);
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
				errprintf ("delete_image(): remove cached\n   '%s%s'\n",
				        location_Path (img->source, NULL), img->source->File);
				free (cob);
			} else if (img->u.Mfdb) {
				errprintf ("delete_image(): not in cache\n   '%s%s'\n",
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
	short   old_w, old_h;

	/* the display size doubles as the scaling target below; re-derive it
	 * from absolute attributes so repeated calls (and the placeholder's
	 * aspect_adjust) cannot squash it twice
	 */
	if (img->set_w > 0) img->disp_w = img->set_w;
	if (img->set_h > 0) img->disp_h = img->set_h;

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

	old_w = img->disp_w;
	old_h = img->disp_h;
	aspect_adjust (img);

	/* The scale steps are only owed to rasterizing callers (info set); the
	 * layout paths just need the display size.
	 */
	if (info) {
		if (img->disp_w != old_w) {
			scale_x = (((size_t)img_w <<16) + (img->disp_w /2)) / img->disp_w;
		}
		if (img->disp_h != old_h) {
			scale_y = (((size_t)img_h <<16) + (img->disp_h /2)) / img->disp_h;
		}
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
					errprintf ("image_calculate(): remove cached\n   '%s%s'\n",
					        location_Path (img->source, NULL), img->source->File);
					free (cob);
				} else if (img->u.Mfdb) {
					errprintf ("image_calculate(): not in cache\n   '%s%s'\n",
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
#if 0
		if (!loader->Cached || loader->Error <= 0) {
			char buf[1024];
			location_FullName (loader->Location, buf, sizeof(buf));
			printf ("image_ldr(%s): load error %i.\n", buf, loader->Error);
		}
#endif
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

/*------------------------------------------------------------------------------
 * Deferred reflow.
 *
 * Every image that arrives with a size the layout did not expect used to
 * recalculate the whole page there and then, which costs the same whether the
 * picture is a banner or a 14 pixel bullet, and a page with sixty distinct
 * pictures paid it sixty times.  Instead the request is parked here and one
 * recalculation serves everything that arrived in the same burst: the job
 * waits, cheaply, until no new request has come in for half a second (or two
 * seconds after the first, so a steady trickle cannot starve it), then lays
 * the page out once and redraws it.
 *
 * Only the recalculation is deferred.  Text renders as it always did, images
 * whose size was already right still draw the moment they decode, and the
 * scheduler keeps running, so the page stays live throughout.
*/
#define REFLOW_QUIET (CLK_TCK /2)   /* fire after this much calm...          */
#define REFLOW_LIMIT (CLK_TCK *2)   /* ...but never wait longer than this    */

static FRAME reflow_frame = NULL;   /* page with a recalculation pending     */
static long  reflow_first = 0;      /* when the first request arrived        */
static long  reflow_last  = 0;      /* when the latest one did               */
static int   reflow_count = 0;
static long  reflow_top   = 0;      /* page y of the topmost affected line   */

#ifdef REFLOWTEST
static BOOL reflow_testing = FALSE;
static int  rt_fires       = 0;
#endif

static void
reflow_run (FRAME frame)
{
	GRECT  rec;
	time_t t;
	int    n    = reflow_count;

#ifdef REFLOWTEST
	if (reflow_testing) {
		/* The layout half is proven by the pixel comparisons; here the frame
		 * is a fake and only the timing is under test, so just say we fired.
		*/
		rt_fires++;
		printf ("RT fire n=%d first+%ldms last+%ldms\n",
		        n, (long)(clock() - reflow_first) * 1000 / CLK_TCK,
		           (long)(clock() - reflow_last)  * 1000 / CLK_TCK);
		reflow_frame = NULL;
		reflow_count = 0;
		return;
	}
#endif
	rec = frame->Container->Area;
	t   = clock();
	reflow_frame = NULL;
	reflow_count = 0;

	dombox_MinWidth (&frame->Page);
	containr_calculate (frame->Container, NULL);

	/* Layout is top down, so nothing above the topmost line that asked for
	 * this batch has moved: repaint from there, not the whole window.  On
	 * the Mega STE the full repaint was costing a batch half as much again
	 * as the recalculation itself.
	*/
	if (reflow_top > 0) {
		long y = reflow_top + frame->clip.g_y - frame->v_bar.scroll;
		if (y > rec.g_y + rec.g_h) {
			rec.g_h = 0;              /* everything affected is scrolled away */
		} else if (y > rec.g_y) {
			rec.g_h -= (WORD)(y - rec.g_y);
			rec.g_y  = (WORD)y;
		}
	}
	if (rec.g_h > 0) {
		containr_notify (frame->Container, HW_PageUpdated, &rec);
	}

	if (logging_is_on) {
		logprintf (LOG_BLUE, "img reflow %ldms for %d images\n",
		           (long)(clock() - t) * 1000 / CLK_TCK, n);
	}
}

static int
reflow_job (void * arg, long invalidated)
{
	FRAME frame = arg;

	if (frame != reflow_frame) {
		return FALSE;                /* flushed, or a stale entry             */
	}
	if (invalidated) {
		reflow_frame = NULL;         /* the page is being torn down           */
		reflow_count = 0;
		return FALSE;
	}
	if (clock() - reflow_last  < REFLOW_QUIET &&
	    clock() - reflow_first < REFLOW_LIMIT) {
		return -2;                   /* JOB_NOOP: more may be about to arrive */
	}
	reflow_run (frame);
	return FALSE;
}

static void
reflow_defer (FRAME frame)
{
	if (reflow_frame == frame &&
	    clock() - reflow_first > REFLOW_LIMIT) {
		/* Overdue.  The parked job cannot be relied on to fire during a page
		 * load, because the loader's jobs outrank it for as long as anything
		 * is arriving; so the arrival that finds the batch overdue settles it
		 * here, and the parked job is only the tail collector.
		*/
		reflow_run (frame);
	}
	if (reflow_frame != frame) {
		if (reflow_frame) {          /* another page is waiting: settle it    */
			reflow_run (reflow_frame);
		}
		reflow_frame = frame;
		reflow_first = clock();
		reflow_top   = 0;
		sched_insert (reflow_job, frame, (long)frame->Container, 1);
	}
	reflow_last = clock();
	reflow_count++;
}

/*----------------------------------------------------------------------------*/
static void
reflow_defer_at (IMAGE img)
{
	long x, y;
	dombox_Offset (img->offset.Origin, &x, &y);
	reflow_defer (img->frame);
	if (reflow_count == 1 || y < reflow_top) {
		reflow_top = y;
	}
}

#ifdef REFLOWTEST
/*============================================================================*/
/* Drives the deferral with the arrival patterns the network produces and a
 * local page cannot: bursts, a steady trickle, two pages at once, teardown.
 * The job is polled directly, as the main loop's timer would, and the fake
 * frames' queued jobs are removed afterwards so nothing fires later against
 * a frame that never existed.
*/
static void
rt_wait (long ticks)
{
	long t0 = clock();
	while (clock() - t0 < ticks);
}

void image_reflow_selftest (void);

void
image_reflow_selftest (void)
{
	static struct frame_item fa, fb;
	long t0;
	int  r, i, fires;

	fa.Container = (CONTAINR)&fa;
	fb.Container = (CONTAINR)&fb;
	reflow_testing = TRUE;
	printf ("REFLOWTEST begin (quiet=%ldms limit=%ldms)\n",
	        (long)REFLOW_QUIET * 1000 / CLK_TCK,
	        (long)REFLOW_LIMIT * 1000 / CLK_TCK);

	/* 1: a burst coalesces into one firing, after the quiet time */
	for (i = 0; i < 5; i++) reflow_defer (&fa);
	r = reflow_job (&fa, 0);
	printf ("  burst: poll right away -> %d (want -2, still waiting)\n", r);
	rt_wait (REFLOW_QUIET /2);
	r = reflow_job (&fa, 0);
	printf ("  burst: poll at quiet/2 -> %d (want -2)\n", r);
	rt_wait (REFLOW_QUIET /2 + REFLOW_QUIET /8);
	r = reflow_job (&fa, 0);
	printf ("  burst: poll past quiet -> %d (want 0, fired above with n=5)\n", r);

	/* 2: a steady trickle cannot starve it, even unpolled.  On a real page
	 * load the parked job is outranked by the loader's jobs the whole time,
	 * so nothing polls it; the arrivals themselves must enforce the cap.
	 * This is the pattern that bit on hardware: 177 images, one batch.
	*/
	t0 = clock();
	fires = rt_fires;
	for (i = 0; i < 8; i++) {
		reflow_defer (&fa);
		rt_wait (REFLOW_QUIET *3 /5);          /* under quiet: always "hot" */
	}
	if (reflow_count) {
		rt_wait (REFLOW_QUIET + REFLOW_QUIET /8);
		reflow_job (&fa, 0);                   /* the tail collector */
	}
	printf ("  trickle: %d firings for 8 unpolled images (want 2)\n",
	        rt_fires - fires);
	(void)t0;

	/* 3: a second page flushes the first at once */
	reflow_defer (&fa);
	reflow_defer (&fb);                        /* must fire fa immediately */
	printf ("  flush: fa fired above with n=1; fb now pending: %s\n",
	        (reflow_frame == &fb ? "yes" : "NO"));

	/* 4: teardown cancels cleanly and the next page starts fresh */
	r = reflow_job (&fb, (long)fb.Container);
	printf ("  teardown: invalidated -> %d (want 0), pending cleared: %s\n",
	        r, (reflow_frame == NULL ? "yes" : "NO"));
	reflow_defer (&fa);
	rt_wait (REFLOW_QUIET + REFLOW_QUIET /8);
	r = reflow_job (&fa, 0);
	printf ("  restart: poll past quiet -> %d (want 0, fired with n=1)\n", r);

	sched_remove (reflow_job, &fa);
	sched_remove (reflow_job, &fb);
	reflow_testing = FALSE;
	printf ("REFLOWTEST end\n");
}
#endif

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

	/* Nobody currently knows whether an image costs its ~3 s in the decoder or
	 * in the reflow it triggers, and the two want opposite fixes.  Time them
	 * apart before optimising either. */
	time_t t_decode = 0;
	time_t t_calc   = 0;
	time_t t_draw   = 0;
	time_t t_mark;

	if (invalidated) {
		containr_notify (frame->Container, HW_ActivityEnd, NULL);
		return FALSE;
	
	} else {
		struct s_cache_info info;
		long    ident = (img->set_w <= 0 || img->set_h <= 0 ? 0
		                 : img_hash (img->disp_w, img->disp_h, img->backgnd));
		CRESULT res   = cache_query (loc, ident, &info);
#ifdef CACHEDIAG
		printf ("QRY %-14s set=%dx%d disp=%dx%d bg=%d ask=%08lx res=%02x got=%08lx\n",
		        loc->File, img->set_w, img->set_h, img->disp_w, img->disp_h,
		        img->backgnd, ident, (int)res, info.Ident);
#endif
		if (res & CR_MATCH) {
			cached = info.Object;
		
		} else if (res & CR_FOUND) {
			/* Entries are stored under the background the decoder settled on,
			 * but we asked with the one the paragraph gave us, so CR_MATCH
			 * cannot have fired.  Correct it and ask again rather than making
			 * do with whatever the query happened to offer: it returns the
			 * first entry for this location, which is the most recent, so a
			 * second size of the same image would otherwise never match its
			 * own entry and would be decoded again on every alternation.
			*/
			if ((char)(info.Ident >>24) == 0xFF) {
				img->backgnd = -1;
			}
			if (!ident) {
				/* Asked without a size, so disp_w/disp_h still hold the
				 * placeholder and no key drawn from them can match.  Take the
				 * size off the found copy exactly as image_calculate() does
				 * during layout -- which is what used to repair this as a side
				 * effect, back when every arriving image relaid the page out.
				*/
				cIMGDATA data = info.Object;
				img_scale (img, data->img_w, data->img_h, NULL);
				set_word (img);
			}
			ident = img_hash (img->disp_w, img->disp_h, img->backgnd);
			if (ident == info.Ident) {
				cached = info.Object;      /* the correction was enough */

			} else if ((res = cache_query (loc, ident, &info)) & CR_MATCH) {
				cached = info.Object;      /* our own entry was further down */
			}
#ifdef CACHEDIAG
			printf ("  found: recomputed=%08lx stored=%08lx -> %s\n",
			        ident, info.Ident, (cached ? "HIT" : "MISS"));
#endif
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
		/* A size the page gave lets a JPEG decode smaller to begin with; a
		 * side relative to the image's own size leaves the other unknown */
		UWORD    want_w = (img->set_w > 0 && img->set_h >= 0 ? img->set_w : 0);
		UWORD    want_h = (img->set_h > 0 && img->set_w >= 0 ? img->set_h : 0);
		
		containr_notify (frame->Container, HW_ImgBegLoad, img->source->FullName);

		t_mark = clock();
		if ((info = get_decoder (loc->FullName, want_w, want_h)) != NULL) {
			if ((data = setup (img, info))        != NULL) {
				read_img (img, info, data);
			}
			(*info->quit)(info);
			if (info->RowMem) free (info->RowMem);
			if (info->DthBuf) free (info->DthBuf);
			free (info);
		}
		t_decode = clock() - t_mark;
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
		if (par->Box.MinWidth < img->disp_w) {
			 par->Box.MinWidth = img->disp_w;
			 par->Box.MaxWidth = 0;
		}
		/* The layout this picture lands in is stale until the deferred
		 * recalculation runs, so there is nowhere correct to draw it yet;
		 * the batch redraw brings it in.  Everything else on the page keeps
		 * drawing as normal in the meantime.
		*/
		reflow_defer_at (img);
		clip = NULL;
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
	
	/* Both of these redraw synchronously -- window_redraw() walks the AES
	 * rectangle list and calls the draw function itself -- so the dither and
	 * blit land here rather than back in the event loop.  Timed because
	 * decode and reflow together do not account for the time between one
	 * image arriving and the next being asked for. */
	t_mark = clock();
	if (!cached) {
		containr_notify (frame->Container, HW_ImgEndLoad, clip);
	} else if (img->u.Data) {
		containr_notify (frame->Container, HW_PageUpdated, clip);
	}
	t_draw = clock() - t_mark;

	containr_notify (frame->Container, HW_ActivityEnd, NULL);

	if (logging_is_on && (t_decode || t_calc || t_draw)) {
		logprintf (LOG_BLUE,
		           "img %ldms decode + %ldms reflow + %ldms draw  %dx%d %s'%s'\n",
		           (long)t_decode * 1000 / CLK_TCK,
		           (long)t_calc   * 1000 / CLK_TCK,
		           (long)t_draw   * 1000 / CLK_TCK,
		           img->disp_w, img->disp_h,
		           (cached ? "cached " : ""), img->source->File);
	}

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
	ULONG    transpar = (info->Transp < 0 && !info->Alpha
	                     ? (img->backgnd = -1) : img->backgnd);
	RASTERIZER raster = rasterizer (info->BitDepth,
	                                (info->Palette ? 0 : info->NumComps));
	pIMGDATA data;
	
	img_scale (img, info->ImgWidth, info->ImgHeight, info);
	
	wd_width = (img->disp_w + 31) / 16;
	pg_size  = wd_width * img->disp_h;
	mem_size = pg_size *2 * n_planes;
	/* A bitmap several screens across is a layout gone wrong, not a picture;
	 * refuse it before it takes the memory everything else needs. */
	if (img->disp_w >= 4096 || img->disp_h >= 4096
	    || mem_size >= (size_t)Malloc (-1L) /2
	    || (data = malloc (sizeof (struct s_img_data) + mem_size)) == NULL) {
		return NULL;
	}
	data->mem_size   = mem_size;
	data->img_w      = (info->FullW ? info->FullW : info->ImgWidth);
	data->img_h      = (info->FullH ? info->FullH : info->ImgHeight);
	data->fd_addr    = (data +1);
	data->fd_w       = img->disp_w;
	data->fd_h       = img->disp_h;
	data->fd_wdwidth = (WORD)wd_width;
	data->fd_stand   = (info->BitDepth > 1 ? raster->StndBitmap : FALSE);
	data->fd_nplanes = n_planes;
	data->fd_r1 = data->fd_r2 = data->fd_r3 = 0;
	
	if (info->RowBytes) {
		size_t psize = sizeof(void*) * info->ImgHeight;
		info->RowMem = malloc (psize +
		                       info->RowBytes * info->ImgHeight + info->NumComps);
		info->RowBuf = (CHAR*)info->RowMem + psize;
	} else {
		info->RowMem = malloc ((info->ImgWidth +1)
		                       * (info->NumComps + (info->Alpha ? 1 : 0)));
		info->RowBuf = info->RowMem;
	}
	if (!info->RowMem) {
		free (data);
		return NULL;
	}
	
	if (info->BitDepth > 1 && (planes <= 8 || raster->StndBitmap)) {
		size_t size = (img->disp_w +1) *3;
		if ((info->DthBuf = malloc (size)) == NULL) {
			free (data);
			return NULL;
		}
		memset (info->DthBuf, 0, size);
	}
	
	/* Like a GIF's transparent colour, alpha is resolved against the colour
	 * the image sits on, which is why the cache keys on it. */
	if (info->Alpha) {
		info->AlphaBg = (img->backgnd >= 0 ? color_lookup (img->backgnd)
		                                   : 0xFFFFFFuL);
	}
	info->DthWidth = img->disp_w;
	info->PixMask  = (1 << info->BitDepth) -1;
	info->PgSize   = pg_size;
	info->LnSize   = wd_width;
	if (!data->fd_stand) {
		info->LnSize *= n_planes;
	}
	
	if (info->Palette || info->BitDepth <= 1) {
		(*raster->cnvpal) (info, transpar);
		/* Transp names one clear entry; any others take its pixel */
		if (info->PalAlpha) {
			short i;
			for (i = 0; i < info->NumAlpha; i++) {
				if (info->PalAlpha[i] < 128) {
					info->Pixel[i] = info->Pixel[info->Transp];
				}
			}
		}
	}
	if (info->BitDepth > 1) {
		data->bgnd = G_WHITE;
		data->fgnd = G_BLACK;
	} else {
		data->bgnd = (WORD)info->Pixel[0];
		data->fgnd = (WORD)info->Pixel[1];
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
skip_corrupted (IMGINFO info, CHAR * buf)
{
	memset (buf, 0x99, info->ImgWidth * info->NumComps);
	return TRUE;
}

/*----------------------------------------------------------------------------*/
static void
read_img (IMAGE img, IMGINFO info, pIMGDATA data)
{
	BOOL (*img_rd)(IMGINFO, CHAR * buf) = info->read;
	void (*raster)(IMGINFO, void * dst) = info->raster;
	short   img_h = info->ImgHeight;
	CHAR  * buf   = info->RowBuf;
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
