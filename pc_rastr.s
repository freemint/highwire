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
	move.b	0(src,d3.w), d3 ; palette index
	and.w		mask, d3
	lsl.w		#1, d3
	move.w	0(palette,d3.w), d0 ; pixel value
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
