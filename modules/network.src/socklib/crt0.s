	.globl	_app		; short, declared in crtinit.c
	.globl	_base		; BASEPAGE *, declared in crtinit.c
	.globl	_heapbase	; void *
	.globl	_stksize	; long, declared by user or in stack.c

	.globl _crtinit		; from crtinit.c
	.globl _acc_main	; from crtinit.c
	
; Assumption: basepage is passed in a0 for accessories; for programs
; a0 is always 0.

	.text
	.even
	.globl	_start
_start:
	sub.l	a6,a6		; clear a6 for debuggers
	cmp.w	#0,a0		; test if acc or program
	beq	_startprg		; if a program, go elsewhere
	tst.l	36(a0)		; also test parent basepage pointer
	bne	_startprg		; for acc's, it must be 0
	move.l	a0,_base	; acc basepage is in A0
	lea	252(a0), sp		; use the command line as a temporary stack
	jmp	_acc_main		; function is in crtinit.c

; program startup code: doesn't actually do much, other than save the
; basepage and call _crtinit

_startprg:
	move.l	4(sp),a0	; get basepage
	move.l	a0,_base	; save it
	move.l	4(a0),d0	; get _base->p_hitpa
	bclr.l	#0,d0		; round off
	move.l	d0,sp		; set stack (temporarily)
	jmp	_crtinit		; in crtinit.c

; _setstack: changes the stack pointer; called as
;     void _setstack( void *newsp )
; called from crtinit.c once the new stack size has been decided upon
;
; WARNING WARNING WARNING: after you do this, local variables may no longer
; be accessible!
; destroys a0, a1 and a7
; modified for Pure C: newsp is in a0
;
	.globl	_setstack
_setstack:
	move.l	(sp)+,a1	; save return address
	move.l	a0,sp		; new stack pointer
	jmp	(a1)			; back to caller

;
; interfaces for gprof: for crt0.s, does nothing
; How does profiling work under Sozobon.
; Could it ever work under Pure C?
;
	.globl 	_monstartup
	.globl	_moncontrol
	.globl 	__mcleanup
_monstartup:
_moncontrol:
__mcleanup:
	rts
