/* @(#)highwire/image.c
 */
#include <stddef.h>
#include <string.h>

#include <gemx.h>

#include "global.h"
#include "image_P.h"

static ULONG cube216[216];
static ULONG graymap[32];


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
	__asm__ volatile (
		"subq.w	#1, %2 | width\n"
	"	move.l	%3, d0 | scale -> index\n"
	"	addq.l	#1, d0\n"
	"	lsr.l		#1, d0\n"
	"0:\n"
	"	clr.w 	d1          | chunk\n"
	"	move.w	#0x8000, d2 | pixel\n"
	"1:\n"
	"	swap		d0\n"
	"	btst		#0, (d0.w,%1) | (src[index>>16] & 1)\n"
	"	beq.b		2f\n"
	"	or.w		d2, d1        | chunk |= pixel\n"
	"2:\n"
	"	swap		d0\n"
	"	add.l		%3, d0        | index += info->IncXfx\n"
	"	lsr.w		#1, d2\n"
	"	dbeq		%2, 1b\n"
	"	move.w	d1, (%0)+\n"
	"	subq.w	#1, %2\n"
	"	bpl.b		0b"
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

#if defined(__GNUC__)
/*------------------------------------------------------------------------------
 * Converts 'num'=[1..16] pixel bytes into 4 word chunks of the the I4
 * interleaved words formats.
 */
static void
raster_chunk4 (CHAR * src, UWORD * dst, size_t num)
{
	__asm__ volatile (
		"clr.l	d4\n"
	"	move.b	(%0)+, d4\n"
	"	move.l	d4, d5\n"
	"	andi.b	#0x03, d4 |chunks 0/1\n"
	"	ror.l 	#1, d4\n"
	"	ror.w		#1, d4\n"
	"	andi.b	#0x0C, d5 |chunks 2/3\n"
	"	ror.l 	#3, d5\n"
	"	ror.w		#1, d5\n"
		
	"	subq.l	#2, %2\n"
	"	bmi			9f\n"
		
	"	moveq.l	#1, d1\n"
		
	"1: | chunk loop\n"
	"	move.b	(%0)+, d2\n"
		
	"	moveq.l	#0x03, d3 |chunks 0/1\n"
	"	and.b		d2, d3\n"
	"	ror.l 	#1, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		d1, d3\n"
	"	or.l		d3, d4\n"
	"	moveq.l	#0x0C, d3 |chunks 2/3\n"
	"	and.b		d2, d3\n"
	"	ror.l 	#3, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		d1, d3\n"
	"	or.l		d3, d5\n"
		
	"	addq.w	#1, d1\n"
	"	dbra		%2, 1b | chunk loop\n"
	"9:\n"
	"	movem.l	d4-d5, (%1)"
		: /* no return value */
		: "a"(src),"a"(dst), "d"(num)
		/*    %0       %1        %2 */
		: "d1","d2","d3","d4","d5"
	);
}

/*------------------------------------------------------------------------------
 * Converts 'num'=[1..16] pixel bytes into 8 word chunks.
 * Used for the I8 interleaved words formats.
 */
static void
raster_chunk8 (CHAR * src, UWORD * dst, size_t num)
{
	__asm__ volatile (
		"clr.l	d4\n"
	"	move.b	(%0)+, d4\n"
	"	move.l	d4, d5\n"
	"	move.l	d4, d6\n"
	"	move.l	d4, d7\n"
	"	andi.b	#0x03, d4 |chunks 0/1\n"
	"	ror.l 	#1, d4\n"
	"	ror.w		#1, d4\n"
	"	andi.b	#0x0C, d5 |chunks 2/3\n"
	"	ror.l 	#3, d5\n"
	"	ror.w		#1, d5\n"
	"	andi.b	#0x30, d6 |chunks 4/5\n"
	"	ror.l 	#5, d6\n"
	"	ror.w		#1, d6\n"
	"	andi.b	#0xC0, d7 |chunks 6/7\n"
	"	ror.l 	#7, d7\n"
	"	ror.w		#1, d7\n"
		
	"	subq.l	#2, %2\n"
	"	bmi			9f\n"
		
	"	moveq.l	#1, d1\n"
		
	"1: | chunk loop\n"
	"	move.b	(%0)+, d2\n"
		
	"	moveq.l	#0x03, d3 |chunks 0/1\n"
	"	and.b		d2, d3\n"
	"	ror.l 	#1, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		d1, d3\n"
	"	or.l		d3, d4\n"
	"	moveq.l	#0x0C, d3 |chunks 2/3\n"
	"	and.b		d2, d3\n"
	"	ror.l 	#3, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		d1, d3\n"
	"	or.l		d3, d5\n"
	"	moveq.l	#0x30, d3 |chunks 4/5\n"
	"	and.b		d2, d3\n"
	"	ror.l 	#5, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		d1, d3\n"
	"	or.l		d3, d6\n"
	"	move.l	#0xC0, d3 |chunks 6/7\n"
	"	and.b		d2, d3\n"
	"	ror.l 	#7, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		d1, d3\n"
	"	or.l		d3, d7\n"
		
	"	addq.w	#1, d1\n"
	"	dbra		%2, 1b | chunk loop\n"
	"9:\n"
	"	movem.l	d4-d7, (%1)"
		: /* no return value */
		: "a"(src),"a"(dst), "d"(num)
		/*    %0       %1        %2 */
		: "d1","d2","d3","d4","d5","d6","d7"
	);
}

#elif defined(__PUREC__)
void raster_chunk4 (CHAR * src, UWORD * dst, size_t num);
void raster_chunk8 (CHAR * src, UWORD * dst, size_t num);

#else
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
#endif


/*----------------------------------------------------------------------------*/
#ifdef __GNUC__
static inline CHAR
#else
static CHAR
#endif
dither_gray (CHAR * gray, WORD * err, BYTE ** buf)
{
	BYTE * dth  = *buf;
	UWORD  idx  = ((err[0] += (WORD)dth[0] + gray[0])
	                           <= 0x07 ? 0 : err[0] >= 0xF8 ? 0x1F : err[0] >>3);
	CHAR * irgb = (CHAR*)&graymap[idx];
	
	err[0] -= irgb[1];
	dth[0] =  (err[0] <= -254 ? (err[0] = -127) :
	           err[0] >= +254 ? (err[0] = +127) : (err[0] /= 2));
	(*buf) += 1;
	
	return irgb[0];
}

/*----------------------------------------------------------------------------*/
#ifdef __GNUC__
static inline CHAR
#else
static CHAR
#endif
dither_true (CHAR * rgb, WORD * err, BYTE ** buf)
{
	BYTE * dth  = *buf;
	UWORD  r    = ((err[0] += (WORD)dth[0] + rgb[0])
	                            <= 42 ? 0 : err[0] >= 213 ? 5 : (err[0] *3) >>7);
	UWORD  g    = ((err[1] += (WORD)dth[1] + rgb[1])
	                            <= 42 ? 0 : err[1] >= 213 ? 5 : (err[1] *3) >>7);
	UWORD  b    = ((err[2] += (WORD)dth[2] + rgb[2])
	                            <= 42 ? 0 : err[2] >= 213 ? 5 : (err[2] *3) >>7);
	CHAR * irgb;
	if (r == g && g == b) {
		irgb = (CHAR*)&graymap[(err[0] + err[1] + err[2]) /24];
	} else {
		irgb = (CHAR*)&cube216[(r *6 + g) *6 + b];
	}
	err[0] -= irgb[1];
	dth[0] =  (err[0] <= -254 ? (err[0] = -127) :
	           err[0] >= +254 ? (err[0] = +127) : (err[0] /= 2));
	err[1] -= irgb[2];
	dth[1] =  (err[1] <= -254 ? (err[1] = -127) :
	           err[1] >= +254 ? (err[1] = +127) : (err[1] /= 2));
	err[2] -= irgb[3];
	dth[2] =  (err[2] <= -254 ? (err[2] = -127) :
	           err[2] >= +254 ? (err[2] = +127) : (err[2] /= 2));
	(*buf) += 3;
	
	return irgb[0];
}

/*------------------------------------------------------------------------------
 * 4 planes interleaved words format
 */
static void
raster_I4 (IMGINFO info, void * _dst)
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	
#if 1
	BYTE * dth    = info->DthBuf;
	WORD   err[3] = { 0, 0, 0 };
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		UWORD idx = info->RowBuf[x >>16];
		*(tmp++)  = dither_true ((CHAR*)&info->Pixel[idx] +1, err, &dth);
		
#else
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		UWORD idx = info->RowBuf[x >>16];
		*(tmp++)  = *(CHAR*)&info->Pixel[idx];
#endif
		
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
	
	BYTE * dth    = info->DthBuf;
	WORD   err    = 0;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		*(tmp++) = dither_gray (&info->RowBuf[x >>16], &err, &dth);
		
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
	
	BYTE * dth    = info->DthBuf;
	WORD   err[3] = { 0, 0, 0 };
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		*(tmp++) = dither_true (&info->RowBuf[(x >>16) *3], err, &dth);
		
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
#if defined(__GNUC__)
	size_t  x     = (info->IncXfx +1) /2;
	__asm__ volatile (
		"subq.w	#1, %4\n"
	"	swap		%6 | -> chunk counter\n"
		
	"1: | line loop\n"
	"	clr.w		%6\n"
	"	clr.l		d4\n"
	"	clr.l		d5\n"
	"	clr.l		d6\n"
	"	clr.l		d7\n"
		
	"5: | chunk loop\n"
	"	swap		%4 | -> value\n"
	"	swap		%6 | -> mask\n"
	"	move.w	(%3), d3\n"
	"	add.l		%5, (%3)\n"
	"	move.b	(d3.w,%1), d3 | palette index\n"
	"	and.w		%6, d3\n"
	"	lsl.w		#2, d3\n"
	"	move.b	(d3.w,%2), %4 | pixel value\n"
	"	swap		%6 | -> chunk counter\n"
		
	"	moveq.l	#0x03, d3 |chunks 0/1\n"
	"	and.b		%4, d3\n"
	"	ror.l 	#1, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		%6, d3\n"
	"	or.l		d3, d4\n"
	"	moveq.l	#0x0C, d3 |chunks 2/3\n"
	"	and.b		%4, d3\n"
	"	ror.l 	#3, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		%6, d3\n"
	"	or.l		d3, d5\n"
	"	moveq.l	#0x30, d3 |chunks 4/5\n"
	"	and.b		%4, d3\n"
	"	ror.l 	#5, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		%6, d3\n"
	"	or.l		d3, d6\n"
	"	move.l	#0xC0, d3 |chunks 6/7\n"
	"	and.b		%4, d3\n"
	"	ror.l 	#7, d3\n"
	"	ror.w		#1, d3\n"
	"	lsr.l		%6, d3\n"
	"	or.l		d3, d7\n"
		
	"	swap		%4 | -> width\n"
	"	addq.b	#1, %6\n"
	"	btst.b	#4, %6\n"
	"	dbne		%4, 5b | chunk loop\n"
		
	"	movem.l	d4-d7, (%0)\n"
	"	adda.w	#16, %0\n"
	"	subq.w	#1, %4\n"
	"	bpl.b		1b"
		:
		: "a"(_dst),"a"(info->RowBuf),"a"(info->Pixel),"a"(&x),
		/*    %0        %1                %2               %3 */
		  "d"(info->DthWidth),"d"(info->IncXfx),"d"(info->PixMask)
		/*    %4                  %5                %6          */
		: "d3","d4","d5","d6","d7"
	);
	
#elif defined(__PUREC__)
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
		*(tmp++)  = *(CHAR*)&info->Pixel[idx];
		
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
	
	BYTE * dth    = info->DthBuf;
	WORD   err    = 0;
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		*(tmp++) = dither_gray (&info->RowBuf[x >>16], &err, &dth);
		
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
	
	BYTE * dth    = info->DthBuf;
	WORD   err[3] = { 0, 0, 0 };
	
	CHAR   buf[16];
	short  n   = 16;
	CHAR * tmp = buf;
	do {
		*(tmp++) = dither_true (&info->RowBuf[(x >>16) *3], err, &dth);
		
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
		*(dst++) = *(CHAR*)&map[(short)info->RowBuf[x >>16] & mask];
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
	
	BYTE * dth   = info->DthBuf;
	WORD   err   = 0;
	do {
		*(dst++) = dither_gray (&info->RowBuf[x >>16], &err, &dth);
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
	
	BYTE * dth    = info->DthBuf;
	WORD   err[3] = { 0, 0, 0 };
	do {
		*(dst++) = dither_true (&info->RowBuf[(x >>16) *3], err, &dth);
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
#if defined (__GNUC__)
	if (info->IncXfx == 0x00010000uL) {
		__asm__ volatile (
			"subq.l	#1, %2\n"
		"	lsr.l		#1, %2\n"
		"1:\n"
		"	clr.l		d0\n"
		"	move.w	(%0)+, d0 |........:........|12345678:12345678|\n"
		"	lsl.l		#8, d0    |........:12345678|12345678:........|\n"
		"	lsr.w		#8, d0    |........:12345678|........:12345678|\n"
		"	move.l	d0, d1\n"
		"	lsr.l  	 #2, d0    |........:..123456|78......:..123456|\n"
		"	andi.w	#0x3F, d0 |........:..123456|........:..123456|\n"
		"	lsl.l		#5, d0    |.....123:456.....|.....123:456.....|\n"
		"	lsr.l		#3, d1    |........:...12345|678.....:...12345|\n"
		"	andi.w	#0x1F, d1 |........:...12345|........:...12345|\n"
		"	or.l		d1, d0\n"
		"	lsl.l		#8, d1    |...12345:........|...12345:........|\n"
		"	lsl.l		#3, d1    |12345...:........|12345...:........|\n"
		"	or.l		d1, d0\n"
		"	move.l	d0, (%1)+\n"
		"	dbra		%2, 1b"
			: /* no return */
			: "a"(info->RowBuf),"a"(_dst), "d"((long)info->DthWidth)
			/*    %0                %1         %2           */
			: "d0","d1","d2"
		);
	} else
#endif
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
#if defined (__GNUC__)
		__asm__ volatile (
			"move.b	(%0), d0\n"
		"	move.b	d0, d1\n"
		"	lsl.w		#5, d0\n"
		"	move.b	d1, d0\n"
		"	lsl.l		#6, d0\n"
		"	move.b	d1, d0\n"
		"	lsr.l		#3, d0\n"
		"	move.w	d0, (%1)"
			: /* no return */
			: "a"(&info->RowBuf[x >>16]),"a"(dst++)
			: "d0","d1"
		);
#else
		UWORD rgb = info->RowBuf[(x >>16)];
		*(dst++)  = ((rgb & 0xF8) <<8) | ((rgb & 0xFC) <<3) | (rgb >>3);
#endif
		x += info->IncXfx;
	} while (--width);
}}
/*------------------------------------*/
static void
dither_16 (IMGINFO info, void * _dst)
{
#if defined (__GNUC__)
	if (info->IncXfx == 0x00010000uL) {
		__asm__ volatile (
			"subq.l	#1, %2\n"
		"	lsr.l		#1, %2\n"
		"1:\n"
		"	movem.w	(%0), d0/d1/d2 |R8:G8| / |B8:r8| / |g8:b8|\n"
		"	addq.l	#6, %0\n"
		"	lsl.l		#5, d0      |........:...RRRRR|RRRGGGGG:GGG00000|\n"
		"	lsl.w		#3, d0	  |........:...RRRRR|GGGGGGGG:00000000|\n"
		"	lsl.l		#6, d0      |.....RRR:RRGGGGGG|GG000000:00000000|\n"
		"	move.w	d1, d0      |.....RRR:RRGGGGGG|BBBBBBBB:rrrrrrrr|\n"
		"	lsl.l		#5, d0      |RRRRRGGG:GGGBBBBB|BBBrrrrr:rrr00000|\n"
		"	move.w	d0, d1      |........:........|BBBrrrrr:rrr00000|\n"
		"	lsl.l		#8, d1      |........:BBBrrrrr|rrr00000:00000000|\n"
		"	move.w	d2, d1      |........:BBBrrrrr|gggggggg:bbbbbbbb|\n"
		"	lsl.l		#6, d1      |..BBBrrr:rrgggggg|ggbbbbbb:bb000000|\n"
		"	lsl.w		#2, d1	  |..BBBrrr:rrgggggg|bbbbbbbb:00000000|\n"
		"	lsl.l		#5, d1      |rrrrrggg:gggbbbbb|bbb00000:00000000|\n"
		"	swap		d1\n"
		"	move.w	d1, d0\n"
		"	move.l	d0, (%1)+\n"
		"	dbra		%2, 1b"
			: /* no return */
			: "a"(info->RowBuf),"a"(_dst), "d"(info->DthWidth)
			/*    %0                %1        %2           */
			: "d0","d1","d2"
		);
	} else
#endif
{
	UWORD * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
#if defined (__GNUC__)
		__asm__ volatile (
			"move.b	(%0)+, d0\n"
		"	lsl.w		#5, d0\n"
		"	move.b	(%0)+, d0\n"
		"	lsl.l		#6, d0\n"
		"	move.b	(%0)+, d0\n"
		"	lsr.l		#3, d0\n"
		"	move.w	d0, (%1)"
			: /* no return */
			: "a"(&info->RowBuf[(x >>16) *3]),"a"(dst++)
			: "d0"
		);
#else
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++)   = (((UWORD)rgb[0] & 0xF8) <<8)
		           | (((UWORD)rgb[1] & 0xFC) <<3)
		           |         (rgb[2]         >>3);
#endif
		x += info->IncXfx;
	} while (--width);
}}

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

/*------------------------------------------------------------------------------
 * 32 planes packed pixels formart in wrong (intel) byte order with the
 * zero byte at the left side in the pixel.
 */
static void
raster_32z (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	ULONG * map   = info->Pixel;
	UWORD   mask  = info->PixMask;
	do {
		*(dst++) = map[(short)info->RowBuf[x >>16] & mask] <<8;
		x += info->IncXfx;
	} while (--width);
}
/*------------------------------------*/
static void
gscale_32z (IMGINFO info, void * _dst)
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
dither_32z (IMGINFO info, void * _dst)
{
	ULONG * dst   = _dst;
	short   width = info->DthWidth;
	size_t  x     = (info->IncXfx +1) /2;
	do {
		CHAR * rgb = &info->RowBuf[(x >>16) *3];
		*(dst++)   = (((((ULONG)rgb[0] <<8) | rgb[1]) <<8) | rgb[2]) <<8;
		x += info->IncXfx;
	} while (--width);
}


/*******************************************************************************
 *
 *   Palette Converter
 */

static short pixel_val[256];

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
		/* store original RGB values for dithering */
		if (!t--) {
			*(pal++) = color_lookup (backgnd) | ((long)pixel_val[backgnd] <<24);
		} else {
			ULONG rgb = ((((long)*r <<8) | *g) <<8) | *b;
			*(pal++)  = rgb | ((long)pixel_val[remap_color (rgb)] <<24);
		}
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
			ULONG  z = color_lookup (backgnd);
			*(pal++) = ((short)(((CHAR*)&z)[1] & 0xF8) <<7)
			         | ((short)(((CHAR*)&z)[2] & 0xF8) <<2)
			         |         (((CHAR*)&z)[3]         >>3);
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
			ULONG  z = color_lookup (backgnd);
			*(pal++) = ((short)(((CHAR*)&z)[1] & 0xF8) <<8)
			         | ((short)(((CHAR*)&z)[2] & 0xFC) <<3)
			         |         (((CHAR*)&z)[3]         >>3);
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
			*(pal++) = (!t-- ? color_lookup (backgnd) : *rgb);
			rgb++;
		} while (--n);
	} else {
		char  * r   = info->Palette + info->PalRpos;
		char  * g   = info->Palette + info->PalGpos;
		char  * b   = info->Palette + info->PalBpos;
		do {
			*(pal++) = (!t-- ? color_lookup (backgnd)
			                 : ((((long)*r <<8) | *g) <<8) | *b);
			r += info->PalStep;
			g += info->PalStep;
			b += info->PalStep;
		} while (--n);
	}
}


/*============================================================================*/
#define IFNC(t) { hwUi_fatal ("image::"t"()", "undefined function called"); }
static void invalid_cnvpal(void) IFNC ("cnvpal_color")
static void invalid_raster(void) IFNC ("raster_cmap")
static void invalid_gscale(void) IFNC ("raster_gray")
static void invalid_dither(void) IFNC ("raster_true")

RASTERIZER
rasterizer (UWORD depth, UWORD comps)
{
	static void (*cnvpal_color)(IMGINFO, ULONG)  = (void*)invalid_cnvpal;
	static void (*raster_cmap) (IMGINFO, void *) = (void*)invalid_raster;
	static void (*raster_gray) (IMGINFO, void *) = (void*)invalid_gscale;
	static void (*raster_true) (IMGINFO, void *) = (void*)invalid_dither;
	
	static struct s_rasterizer raster = { NULL, FALSE, };
	if (!raster.DispInfo) {
		static char disp_info[10] = "";
		short out[273] = { -1, };
		short sdepth;
		BOOL  reverse, z_trail;
		
		vq_scrninfo (vdi_handle, out);
		memcpy (pixel_val, out + 16, 512);
		sdepth  = ((UWORD)out[4] == 0x8000u
		           ? 15 : out[2]);           /* bits per pixel used */
		reverse = (out[16] < out[48]);       /* intel crap...       */
		z_trail = (out[48] > 0);             /* bits are shifted to the right side */
		
		sprintf (disp_info, "%c%02i%s",
		         (out[0] == 0 ? 'i' : out[0] == 2 ? 'p' : 'x'), sdepth,
		         (reverse ? "r" : z_trail ? "z" : ""));
	
		if (sdepth == 1) {                        /* monochrome */
				cnvpal_color = cnvpal_1_2;
				raster_cmap  = raster_D1;
				raster_gray  = gscale_D1;
				raster_true  = dither_D1;
		} else if (out[0] == 0) switch (sdepth) { /* interleaved words */
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
		} else if (out[0] == 2) switch (sdepth) { /* packed pixels */
			case  8:
				cnvpal_color = cnvpal_4_8;
				raster_cmap  = raster_P8;
				raster_gray  = gscale_P8;
				raster_true  = dither_P8;
				break;
			case 15: if (!(out[14] & 0x02)) {
				cnvpal_color = cnvpal_15;
				if (reverse) {
					raster_cmap = raster_15r;
					raster_gray = gscale_15r;
					raster_true = dither_15r;
				} else {
					raster_cmap = raster_15;
					raster_gray = gscale_15;
					raster_true = dither_15;
				}
				break;
			}
			case 16:
				cnvpal_color = cnvpal_high;
				if (reverse) {
					raster_cmap = raster_16r;
					raster_gray = gscale_16r;
					raster_true = dither_16r;
				} else {
					raster_cmap = raster_16;
					raster_gray = gscale_16;
					raster_true = dither_16;
				}
				break;
			case 24:
				cnvpal_color = cnvpal_true;
				if (reverse) {
					raster_cmap = raster_24r;
					raster_gray = gscale_24r;
					raster_true = dither_24r;
				} else {
					raster_cmap = raster_24;
					raster_gray = gscale_24;
					raster_true = dither_24;
				}
				break;
			case 32:
				cnvpal_color = cnvpal_true;
				if (reverse) {
					raster_cmap = raster_32r;
					raster_gray = gscale_32r;
					raster_true = dither_32r;
				} else if (z_trail) {
					raster_cmap = raster_32z;
					raster_gray = gscale_32z;
					raster_true = dither_32z;
				} else {
					raster_cmap = raster_32;
					raster_gray = gscale_32;
					raster_true = dither_32;
				}
				break;
		} else {
			hwUi_error ("image::init_display()",
			            "Unrecognized screen format!\n"
			            "sdepth=%i out+0=%i out+4=%04X out+14=%04X mode='%.10s'",
			            sdepth, out[0], out[4], out[14], disp_info);
		}
		if (!raster_cmap) {                     /* standard format */
			cnvpal_color      = cnvpal_4_8;
			raster_cmap       = raster_stnd;
			raster.StndBitmap = TRUE;
		}
		
		if (sdepth == 4 || sdepth == 8) {
			color_tables (cube216, graymap, pixel_val);
		}
		
		raster.DispInfo = disp_info;
	}
	
	if (depth == 1) {
		raster.cnvpal = cnvpal_mono;
		raster.functn = raster_mono;
	} else if (!depth) {
		raster.cnvpal = (void*)invalid_cnvpal;
		raster.functn = (void*)invalid_raster;
	} else if (comps > 0) {
		raster.cnvpal = (void*)invalid_cnvpal;
		raster.functn = (comps == 3 ? raster_true : raster_gray);
	} else {
		raster.cnvpal = cnvpal_color;
		raster.functn = raster_cmap;
	}
	return &raster;
}
