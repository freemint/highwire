;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; monochrome format

	.TEXT
	.EXPORT pc_raster_mono ;(A0:dst, A1:src, D0:width, D1:scale)
	
	dst   = A0
	src   = A1
	width = D0
	scale = D1
	
pc_raster_mono:
	
	index = D2
	pixel = D3
	chunk = D4
	
	movem.l	D3-D4, -(a7)
	
	move.l	scale, index
	addq.l	#1, index
	lsr.l		#1, index
	
	subq.w	#1, width
	
.start:
	clr.w 	chunk
	move.w	#$8000, pixel
	
.loop:
	swap		index
	btst		#0, 0(src,index.w)
	beq.b		.l1
	or.w		pixel, chunk
.l1:
	swap		index
	add.l		scale, index
	
	lsr.w		#1, pixel
	dbeq		width, .loop
.store:
	move.w	chunk, (dst)+
	
	subq.w	#1, width
	bpl.b		.start
	
	movem.l	(a7)+, D3-D4
	rts


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Converts 'num'=[1..16] pixel bytes into 4 word chunks of the the I4
;; interleaved words formats.

	.TEXT
	.EXPORT raster_chunk4 ;(A0:src, A1:dst, D0:num)
	
	src = A0
	dst = A1
	num = D0

raster_chunk4:

	movem.l	D3-D5, -(a7)
	clr.l		d4
	move.b	(src)+, d4
	move.l	d4, d5
	andi.b	#0x03, d4 ; chunks 0/1
	ror.l 	#1, d4
	ror.w		#1, d4
	andi.b	#0x0C, d5 ; chunks 2/3
	ror.l 	#3, d5
	ror.w		#1, d5
	
	subq.l	#2, num
	bmi		.store
	
	moveq.l	#1, d1
	
.loop:
	move.b	(src)+, d2
	
	moveq.l	#0x03, d3 ; chunks 0/1
	and.b		d2, d3
	ror.l 	#1, d3
	ror.w		#1, d3
	lsr.l		d1, d3
	or.l		d3, d4
	moveq.l	#0x0C, d3 ; chunks 2/3
	and.b		d2, d3
	ror.l 	#3, d3
	ror.w		#1, d3
	lsr.l		d1, d3
	or.l		d3, d5
	
	addq.w	#1, d1
	dbra		num, .loop
.store:
	movem.l	d4-d5, (dst)
	movem.l	(a7)+, D3-D5
	rts


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Converts 'num'=[1..16] pixel bytes into 8 word chunks of the the I8
;; interleaved words formats.

	.TEXT
	.EXPORT raster_chunk8 ;(A0:src, A1:dst, D0:num)
	
	src = A0
	dst = A1
	num = D0

raster_chunk8:

	movem.l	D3-D7, -(a7)
	clr.l		d4
	move.b	(src)+, d4
	move.l	d4, d5
	move.l	d4, d6
	move.l	d4, d7
	andi.b	#0x03, d4 ; chunks 0/1
	ror.l 	#1, d4
	ror.w		#1, d4
	andi.b	#0x0C, d5 ; chunks 2/3
	ror.l 	#3, d5
	ror.w		#1, d5
	andi.b	#0x30, d6 ; chunks 4/5
	ror.l 	#5, d6
	ror.w		#1, d6
	andi.b	#0xC0, d7 ; chunks 6/7
	ror.l 	#7, d7
	ror.w		#1, d7
	
	subq.l	#2, num
	bmi		.store
	
	moveq.l	#1, d1
	
.loop:
	move.b	(src)+, d2
	
	moveq.l	#0x03, d3 ; chunks 0/1
	and.b		d2, d3
	ror.l 	#1, d3
	ror.w		#1, d3
	lsr.l		d1, d3
	or.l		d3, d4
	moveq.l	#0x0C, d3 ; chunks 2/3
	and.b		d2, d3
	ror.l 	#3, d3
	ror.w		#1, d3
	lsr.l		d1, d3
	or.l		d3, d5
	moveq.l	#0x30, d3 ; chunks 4/5
	and.b		d2, d3
	ror.l 	#5, d3
	ror.w		#1, d3
	lsr.l		d1, d3
	or.l		d3, d6
	move.l	#0xC0, d3 ; chunks 6/7
	and.b		d2, d3
	ror.l 	#7, d3
	ror.w		#1, d3
	lsr.l		d1, d3
	or.l		d3, d7
	
	addq.w	#1, d1
	dbra		num, .loop
.store:
	movem.l	d4-d7, (dst)
	movem.l	(a7)+, D3-D7
	rts


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; 8 planes interleaved words format

	.TEXT
	.EXPORT pc_raster_I8 ;(A0:dst, A1:src, D0:width, D1:scale, D2: mask)
	                     ; 4(A7): palette
	
	dst   = A0
	src   = A1
	width = D0
	scale = D1
	mask  = D2
	
pc_raster_I8:
	
	palette = A2
	
	movem.l	d3-d7/a2, -(a7)
	
	move.l	28(a7), palette
	
	move.l	scale, d3
	addq.l	#1, d3
	lsr.l		#1, d3
	move.l	d3, -(a7)
	
	subq.w	#1, width
	swap		mask ;-> chunk counter
	
.line:
	clr.w		d2
	clr.l		d4
	clr.l		d5
	clr.l		d6
	clr.l		d7
	
.chunk:
	swap		width ;-> value
	swap		d2    ;-> mask
	move.w	(a7), d3
	add.l		scale, (a7)
	move.b	(d3.w,src), d3 ; palette index
	and.w		mask, d3
	lsl.w		#2, d3
	move.b	(d3.w,palette), d0 ; pixel value
	swap		mask  ;-> chunk counter
	
	moveq.l	#0x03, d3 ; chunks 0/1
	and.b		d0, d3
	ror.l 	#1, d3
	ror.w		#1, d3
	lsr.l		d2, d3
	or.l		d3, d4
	moveq.l	#0x0C, d3 ; chunks 2/3
	and.b		d0, d3
	ror.l 	#3, d3
	ror.w		#1, d3
	lsr.l		d2, d3
	or.l		d3, d5
	moveq.l	#0x30, d3 ; chunks 4/5
	and.b		d0, d3
	ror.l 	#5, d3
	ror.w		#1, d3
	lsr.l		d2, d3
	or.l		d3, d6
	move.l	#0xC0, d3 ; chunks 6/7
	and.b		d0, d3
	ror.l 	#7, d3
	ror.w		#1, d3
	lsr.l		d2, d3
	or.l		d3, d7
	
	swap		d0 ;-> width
	addq.b	#1, d2
	btst.b	#4, d2
	dbne		width, .chunk
	
	movem.l	d4-d7, (dst)
	adda.w	#16, dst
	subq.w	#1, width
	bpl.b		.line
	
	addq.l	#4, a7
	movem.l	(a7)+, d3-d7/a2
	rts
