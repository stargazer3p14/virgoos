;
;	INIT.ASM
;
;	Source code for initialization and lowest level interface of SOS.
;

;.586p
;.model	FLAT

CPU	586
USE32

;%INCLUDE	"include-gcc/bootsec.inc"
%INCLUDE	"include/bootsec.inc"



%define	PUBLIC	GLOBAL

;_DATA	SEGMENT
SEGMENT	.data

;Idt		LABEL	Gate32
Idt:
%IF 0
	Gate32	< LOWWORD ( offset divide_by_0 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset divide_by_0 ) >
	Gate32	< LOWWORD ( offset debug_trap ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset debug_trap ) >
	Gate32	< LOWWORD ( offset nmi ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset nmi ) >
	Gate32	< LOWWORD ( offset int_3 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset int_3 ) >
	Gate32	< LOWWORD ( offset overflow ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset overflow ) >
	Gate32	< LOWWORD ( offset bound_ex ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset bound_ex ) >
	Gate32	< LOWWORD ( offset inv_op ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset inv_op ) >
	Gate32	< LOWWORD ( offset fp_np ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset fp_np ) >
	Gate32	< LOWWORD ( offset double_fault ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset double_fault ) >
	Gate32	< LOWWORD ( offset exc_09 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_09 ) >
	Gate32	< LOWWORD ( offset inv_tss ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset inv_tss ) >
	Gate32	< LOWWORD ( offset seg_np ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset seg_np ) >
	Gate32	< LOWWORD ( offset stack_fault ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset stack_fault ) >
	Gate32	< LOWWORD ( offset gp_fault ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset gp_fault ) >
	Gate32	< LOWWORD ( offset page_fault ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset page_fault ) >
	Gate32	< LOWWORD ( offset exc_0f ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_0f ) >
	Gate32	< LOWWORD ( offset exc_10 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_10 ) >
	Gate32	< LOWWORD ( offset exc_11 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_11 ) >
	Gate32	< LOWWORD ( offset exc_12 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_12 ) >
	Gate32	< LOWWORD ( offset exc_13 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_13 ) >
	Gate32	< LOWWORD ( offset exc_14 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_14 ) >
	Gate32	< LOWWORD ( offset exc_15 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_15 ) >
	Gate32	< LOWWORD ( offset exc_16 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_16 ) >
	Gate32	< LOWWORD ( offset exc_17 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_17 ) >
	Gate32	< LOWWORD ( offset exc_18 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_18 ) >
	Gate32	< LOWWORD ( offset exc_19 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_19 ) >
	Gate32	< LOWWORD ( offset exc_1a ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_1a ) >
	Gate32	< LOWWORD ( offset exc_1b ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_1b ) >
	Gate32	< LOWWORD ( offset exc_1c ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_1c ) >
	Gate32	< LOWWORD ( offset exc_1d ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_1d ) >
	Gate32	< LOWWORD ( offset exc_1e ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_1e ) >
	Gate32	< LOWWORD ( offset exc_1f ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset exc_1f ) >

	Gate32	< LOWWORD ( offset irq_0 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_0 ) >
	Gate32	< LOWWORD ( offset irq_1 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_1 ) >
	Gate32	< LOWWORD ( offset irq_2 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_2 ) >
	Gate32	< LOWWORD ( offset irq_3 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_3 ) >
	Gate32	< LOWWORD ( offset irq_4 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_4 ) >
	Gate32	< LOWWORD ( offset irq_5 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_5 ) >
	Gate32	< LOWWORD ( offset irq_6 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_6 ) >
	Gate32	< LOWWORD ( offset irq_7 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_7 ) >

	Gate32	< LOWWORD ( offset irq_8 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_8 ) >
	Gate32	< LOWWORD ( offset irq_9 ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_9 ) >
	Gate32	< LOWWORD ( offset irq_0a ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_0a ) >
	Gate32	< LOWWORD ( offset irq_0b ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_0b ) >
	Gate32	< LOWWORD ( offset irq_0c ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_0c ) >
	Gate32	< LOWWORD ( offset irq_0d ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_0d ) >
	Gate32	< LOWWORD ( offset irq_0e ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_0e ) >
	Gate32	< LOWWORD ( offset irq_0f ), CODE_32, 0, INT_386_ACCESS , HIGHWORD( offset irq_0f ) >
%ELSE
	NUM_INTS	EQU	30h

;	Gate32	NUM_INTS DUP (< , CODE_32, 0, INT_386_ACCESS, >)

	%REP	NUM_INTS
;
;
;	May be NASM can do it, but I have no nerve
;
;		ISTRUC	Gate32
;			AT	Offset16, 0
;			AT	Selector, CODE_32
;			AT	Params, 0
;			AT	GateType, INT_386_ACCESS
;			AT	Offset32, 0
;		IEND


		DW	0
		DW	CODE_32
		DB	0
		DB	INT_386_ACCESS
		DW	0
	%ENDREP



	IntHandlers	DD	 divide_by_0,  debug_trap,  nmi,  int_3
				DD	 overflow,  bound_ex,  inv_op,  fp_np
				DD	 double_fault,  exc_09,  inv_tss,  seg_np
				DD	 stack_fault,  gp_fault,  page_fault,  exc_0f
				DD	 exc_10,  exc_11,  exc_12,  exc_13
				DD	 exc_14,  exc_15,  exc_16,  exc_17
				DD	 exc_18,  exc_19,  exc_1a,  exc_1b
				DD	 exc_1c,  exc_1d,  exc_1e,  exc_1f
				DD	 irq_0,  irq_1,  irq_2,  irq_3
				DD	 irq_4,  irq_5,  irq_6,  irq_7
				DD	 irq_8,  irq_9,  irq_0a,  irq_0b
				DD	 irq_0c,  irq_0d,  irq_0e,  irq_0f

;
;	_callbacks is a bitmask which is set by set_int_callback() API call. If a particular bit
;	is set, call_callbacks() is called from the main interrupt/exception handler. The later
;	is responsible for calling the callbacks chains.
;
;EXTERN	_int_callbacks
;EXTERN	_exc_callbacks

;
; Test interrupt reporting time
;
;%DEFINE	TEST_INT_RESPONSE

%IFDEF	TEST_INT_RESPONSE
PUBLIC	_int_received
	_int_received	DD	0, 0
PUBLIC	_int_handler_start
	_int_handler_start	DD	0, 0
%ENDIF
	
%ENDIF

IDT_SIZE	EQU	NUM_INTS * 8 - 1

;Idtr		DTR		< IDT_SIZE, offset Idt >
Idtr:

;
;
;	Nasm is YAKKIE!!!!!.....
;
;	ISTRUC	DTR
;		AT	Limit,	IDT_SIZE
;		AT	Base, Idt
;	IEND

		DW	IDT_SIZE
		DD	Idt


TimerPtr	DD	0B8000h


ExcMsg		DB	"Exception: ", 0
ErrCodeMsg	DB	"Error code: ", 0
NoneMsg		DB	"none", 0
ExcAddrMsg	DB	"Exception address: ", 0
RegsMsg		DB	"Registers: EAX=XXXXXXXX EBX=XXXXXXXX ECX=XXXXXXXX EDX=XXXXXXXX ESI=XXXXXXXX EDI=XXXXXXXX ESP=XXXXXXXX EBP=XXXXXXXX Eflags=XXXXXXXX", 0
Columns		DB	80

ExcEax	DD	0
ExcEbx	DD	0
ExcEcx	DD	0
ExcEdx	DD	0
ExcEsi	DD	0
ExcEdi	DD	0
ExcEsp	DD	0
ExcEbp	DD	0

ExcAddr	DD	0

;IntNum		DD	?
;IntNum	RESD	1

STR_BUF		EQU	50000h

;_DATA	ENDS

;EXTRN	_entry: NEAR
;EXTRN	_call_int_callbacks: NEAR
EXTERN	_entry, _call_int_callbacks

EXTERN	_running_irq
EXTERN	_isr_proc

PUBLIC	_init
;_TEXT	SEGMENT
;SEGMENT	_TEXT

;.code
SEGMENT	.text

;-----------------------------------------------------------------------------
;
;	I:  AL = 1-byte hex, ES:EDI -> converted 2 bytes, 0-term.
;	O:  nothing
;
;	Converts number in AL to hex. representation at ES:EDI
;
;-----------------------------------------------------------------------------
;HexToA	PROC	near	USES eax
HexToA:
	push	eax
	mov	ah, al
	and	al, 0F0h
	and	ah, 0Fh
	add	ah, '0'
	cmp	ah, '9'
	jna	@1
	add	ah, 'A' - '0' - 10
@1:
	shr	al, 4
	add	al, '0'
	cmp	al, '9'
	jna	@2
	add	al, 'A' - '0' - 10
@2:
	es mov	[edi], ax
;	mov	byte ptr es:[edi+2], 0
	es mov	byte [edi+2], 0

	pop	eax
	ret
;HexToA	ENDP


;-----------------------------------------------------------------------------
;
;	I:  AX = 2-byte hex, ES:EDI -> converted 4 bytes, 0-term.
;	O:  nothing
;
;	Converts number in AX to hex. representation at ES:EDI
;
;-----------------------------------------------------------------------------
;Hex16ToA	PROC	near
Hex16ToA:
	xchg	ah, al
	call	HexToA
	add	edi, 2
	xchg	ah, al
	call	HexToA
	sub	edi, 2
	ret
;Hex16ToA	ENDP


;-----------------------------------------------------------------------------
;
;	I:  EAX = 4-byte hex, ES:EDI -> converted 8 bytes, 0-term.
;	O:  nothing
;
;	Converts number in EAX to hex. representation at ES:EDI
;
;-----------------------------------------------------------------------------
;Hex32ToA	PROC	near
Hex32ToA:
	ror	eax, 16
	call	Hex16ToA
	add	edi, 4
	ror	eax, 16
	call	Hex16ToA
	sub	edi, 4
	ret
;Hex32ToA	ENDP


;-----------------------------------------------------------------------------
;
;	I:  DS:ESI -> source 0-termin. string.
;	O:  EAX = string length.
;
;-----------------------------------------------------------------------------
;StrLen	PROC	near	USES esi
StrLen:
	push	esi
	sub	eax, eax
find_0_loop:
;	cmp	byte ptr ds:[esi][eax], 0
	cmp	byte [esi+eax], 0
	jz	found_0
	inc	eax
	jmp	find_0_loop
found_0:
	pop	esi
	ret
;StrLen	ENDP


;----------------------------------------------------------------------------
;
;	I:  DS:ESI -> String (0 - terminated).
;	    DL:DH = column: row
;	    BL = color
;
;	R:	PROTMODE.
;
;	Prints a color string.
;
;-----------------------------------------------------------------------------
;WriteStr	PROC	near	USES eax ecx esi edi
WriteStr:
	push	eax
	push	ecx
	push	esi
	push	edi

	call	StrLen
	mov	ecx, eax
	mov	al, dh
;	mul	Columns
	mul	byte [Columns]
	add	al, dl
	adc	ah, 0
	shl	eax, 1
	movzx	edi, ax
	add	edi, 0B8000h
	mov	ah, bl
	cld
write_loop:
	lodsb
	stosw
	loop	write_loop

	pop	edi
	pop	esi
	pop	ecx
	pop	eax
	ret
;WriteStr	ENDP



_init:
;
;	Set up IDT (MASM won't allow relocatable offsets for LOWWORD and HIGHWORD).
;
	sub	ecx, ecx
next_handler:
;	mov	eax, IntHandlers[ ecx * 4 ]
	mov	eax, [IntHandlers+ecx*4]
;	mov	( Gate386 PTR Idt[ ecx * 8 ] ).DestOff, ax
	mov	[Idt+ecx*8+Offset16], ax
	shr	eax, 16
;	mov	( Gate386 PTR Idt[ ecx * 8 ] ).DestOff32, ax
	mov	[Idt+ecx*8+Offset32], ax
	inc	ecx
	cmp	ecx, NUM_INTS
	jb	next_handler

;
;	Reprogram PIC (8259) to:
;
;		IRQ 0 .. 7 <-> interrupt 20h .. 27h
;		IRQ 8 .. F <-> interrupt 28h .. 2Fh
;

; Master 8259
	mov	edx, PIC_MASTER
	mov	al, CMD_ICW1
	out	dx, al

	inc	dx
	mov	al, PIC0_BASE_INT
	out	dx, al

	mov	al, CMD_ICW3
	out	dx, al

	mov	al, CMD_ICW4
	out	dx, al


; Slave 8259
	mov	edx, PIC_SLAVE
	mov	al, CMD_ICW1
	out	dx, al

	inc	dx
	mov	al, PIC1_BASE_INT
	out	dx, al

	mov	al, CMD_SLAVE_ICW3
	out	dx, al

	mov	al, CMD_ICW4
	out	dx, al


; Load IDTR
;	lidt	fword ptr Idtr
	lidt	[Idtr]

; Enable interrupts
	sti

; Jump to application's entry point.
	;jmp	_entry
	pushfd
	push	cs
	push	_entry
	iretd


;
;	Exception handlers.
;
;		The stub handlers will work for all exceptions but Stack Fault and Double Fault.
;	For these exceptions the entry point must be a task gate
;

divide_by_0:
	push	0
	push	0
	jmp	handle_exc

debug_trap:
	push	0
	push	1
	jmp	handle_exc

nmi:
	push	0
	push	2
	jmp	handle_exc

int_3:
	push	0
	push	3
	jmp	handle_exc

overflow:
	push	0
	push	4
	jmp	handle_exc

bound_ex:
	push	0
	push	5
	jmp	handle_exc

inv_op:
	push	0
	push	6
	jmp	handle_exc

fp_np:
	push	0
	push	7
	jmp	handle_exc

double_fault:
	push	4
	push	8
	jmp	handle_exc

exc_09:
	push	4
	push	9
	jmp	handle_exc

inv_tss:
	push	4
	push	0ah
	jmp	handle_exc

seg_np:
	push	4
	push	0bh
	jmp	handle_exc

stack_fault:
	push	4
	push	0ch
	jmp	handle_exc

gp_fault:
	push	4
	push	0dh
	jmp	handle_exc

page_fault:
	push	4
	push	0eh
	jmp	handle_exc

exc_0f:
	push	0
	push	0fh
	jmp	handle_exc

exc_10:
	push	0
	push	10h
	jmp	handle_exc

exc_11:
	push	0
	push	11h
	jmp	handle_exc

exc_12:
	push	0
	push	12h
	jmp	handle_exc

exc_13:
	push	0
	push	13h
	jmp	handle_exc

exc_14:
	push	0
	push	14h
	jmp	handle_exc

exc_15:
	push	0
	push	15h
	jmp	handle_exc

exc_16:
	push	0
	push	16h
	jmp	handle_exc

exc_17:
	push	0
	push	17h
	jmp	handle_exc

exc_18:
	push	0
	push	18h
	jmp	handle_exc

exc_19:
	push	0
	push	19h
	jmp	handle_exc

exc_1a:
	push	0
	push	1ah
	jmp	handle_exc

exc_1b:
	push	0
	push	1bh
	jmp	handle_exc

exc_1c:
	push	0
	push	1ch
	jmp	handle_exc

exc_1d:
	push	0
	push	1dh
	jmp	handle_exc

exc_1e:
	push	0
	push	1eh
	jmp	handle_exc

exc_1f:
	push	0
	push	1fh
	jmp	handle_exc



;
;	Common exception handler.
;
;	On entry:
;		[ ESP ] = exception number
;		[ ESP + 4 ] = number of bytes in error code ( 0 or 4 ).
;
handle_exc:

; Get registers
	pushad
	pop	DWORD [ExcEdi]
	pop	DWORD [ExcEsi]
	pop	DWORD [ExcEbp]
	pop	DWORD [ExcEsp]
	pop	DWORD [ExcEbx]
	pop	DWORD [ExcEdx]
	pop	DWORD [ExcEcx]
	pop	DWORD [ExcEax]

; Write exception number
;	mov	esi, offset ExcMsg
	mov	esi, ExcMsg
	call	StrLen
	sub	dl, dl
	sub	dh, dh
	mov	bl, 4
	call	WriteStr
	add	dl, al

; Get exception number
	pop	eax
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	call	WriteStr

; Write error code
;	mov	esi, offset ErrCodeMsg
	mov	esi, ErrCodeMsg
	call	StrLen
	mov	dh, 1
	sub	dl, dl
	call	WriteStr
	add	dl, al

; Check if there is an error code
	pop	eax
	test	eax, eax
	jnz	@3

;	mov	esi, offset NoneMsg
	mov	esi, NoneMsg
	jmp	write_err_code

; There is an error code, pop it off stack
@3:
	pop	eax
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF

write_err_code:
	call	WriteStr

; Write faulting address.
;	mov	esi, offset ExcAddrMsg
	mov	esi, ExcAddrMsg
	call	StrLen
	mov	dh, 2
	sub	dl, dl
	call	WriteStr
	add	dl, al
; Get exception (or return) address
	pop	eax
	mov	[ExcAddr], eax
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	call	WriteStr

; Dump registers
	mov	esi, RegsMsg
	mov	dh, 3
	sub	dl, dl
	call	WriteStr

; EAX
	mov	eax, [ExcEax]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 15
	call	WriteStr

; EBX
	mov	eax, [ExcEbx]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 28
	call	WriteStr

; ECX
	mov	eax, [ExcEcx]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 41
	call	WriteStr

; EDX
	mov	eax, [ExcEdx]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 54
	call	WriteStr

; ESI
	mov	eax, [ExcEsi]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 67
	call	WriteStr

; EDI
	mov	eax, [ExcEdi]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 80
	call	WriteStr

; ESP
	mov	eax, [ExcEsp]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 93
	call	WriteStr

; EBP
	mov	eax, [ExcEbp]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 106
	call	WriteStr

; Eflags
	mov	eax, [esp+4]
	mov	edi, STR_BUF
	call	Hex32ToA
	mov	esi, STR_BUF
	mov	dl, 122
	call	WriteStr

; Dump 32 bytes from exception address
	mov	edi, STR_BUF
	mov	esi, edi
	mov	ebx, [ExcAddr]
	mov	ecx, 32
	mov	dh, 5
	sub	dl, dl
dump_exc_opcodes:
	mov	al, [ebx]
	call	HexToA
	push	ebx
	mov	bl, 4
	call	WriteStr
	pop	ebx
	inc	ebx
	add	dl, 3
	dec	ecx
	jnz	dump_exc_opcodes

; Halt
	cli
	hlt

;===================================================
;	Interrupt handlers entry.
;===================================================
irq_0:
	push	0
	jmp	handle_int
	
irq_1:
	push	1
	jmp	handle_int

irq_2:
	push	2
	jmp	handle_int

irq_3:
	push	3
	jmp	handle_int

irq_4:
	push	4
	jmp	handle_int

irq_5:
	push	5
	jmp	handle_int

irq_6:
	push	6
	jmp	handle_int

irq_7:
	push	7
	jmp	handle_int

irq_8:
	push	8
	jmp	handle_int

irq_9:
	push	9
	jmp	handle_int

irq_0a:
	push	0Ah
	jmp	handle_int

irq_0b:
	push	0Bh
	jmp	handle_int

irq_0c:
	push	0Ch
	jmp	handle_int

irq_0d:
	push	0Dh
	jmp	handle_int

irq_0e:
	push	0Eh
	jmp	handle_int

irq_0f:
	push	0Fh
; Fall through

;
;	Handle interrupt received through master or slave 8259 PIC.
;
handle_int:
	push	eax
	push	ebx

%IFDEF	TEST_INT_RESPONSE
;
;	Test latency for interrupt.
;	This code measures latency without itself. With the measurement code the latency is slightly higher
;
	push	edx
	rdtsc
	mov	[_int_received], eax
	mov	[_int_received+4], edx

; Dummy instructions that take about the same time as instructions above 'rdtsc'
	push	10h
	jmp	dummy1
dummy1:
	push	eax
	push	ebx

; Enough, go ahead
	add	esp, 12
	pop	edx
;
;	End of test latency for interrupt
;
%ENDIF

	mov	ebx, [esp+8]

;
; Ts from here to int handler is 2200-2350 cycles
;

;	bt	[_int_callbacks], ebx
;	jnc	return_int

;
; Ts from here to int handler is 2200-2300 cycles
;

	; On stack saved already: EAX, EBX
	; Don't trust C code, save all regs (except for EBP, ESP)
	push	ecx
	push	edx
	push	esi
	push	edi

;
; Ts from here to int handler is 2340 (cycles)
;
;	push	ebx			; Save IRQ num.
	
%IF 1
	push	ebx			; int_no
	call	_call_int_callbacks
	add	esp, 4
%ELSE
;
; Implement _call_int_callbacks in assembly
;

;;;;; Just to test where cycles go
;	push	eax
;	push	edx
;	rdtsc
;	mov	[_int_received], eax
;	mov	[_int_received+4], edx
;	pop	edx
;	pop	eax
;;;;;

; This increment seems to take about 1000-2000 cycles (does it invoke mandatory cache miss?)
	inc	dword [_running_irq]
;
; Ts from here to int handler is 105-115 (cycles)
;
	mov	eax, 10							; MAX_IRQ_HANDLERS
	mov	ecx, eax

;
; Ts from here to int handler is 105-115 (cycles)
;

	mul	ebx
	lea	eax, [eax*4]

;
; Ts from here to int handler is 90-105 (cycles)
;

next_int_handler:
	push	eax

;
; Ts from here to int handler is 90-105 (cycles)
;

	call	dword [_isr_proc+eax]
	test	eax, eax
	pop	eax
	jnz	irq_done
	dec	ecx								; This fixes bug in call_int_callbacks()
	jz	irq_done
	add	eax, 4
	jmp	next_int_handler
irq_done:
	dec	dword [_running_irq]


;
; Call _call_int_callbacks() to start pending task if necessary
;
	push	ebx			; int_no
;	call	_call_int_callbacks
	add	esp, 4
%ENDIF
;	pop	ebx		; Restore IRQ num

	pop	edi
	pop	esi
	pop	edx
	pop	ecx

return_int:

	;
	; Acknowledge interupt with PIC(s).
	; After calling callbacks, to enable nesting of interrupt handlers according to priorities
	; TODO: check rotation priorities of 8259 and other options
	;
	;mov	al, CMD_EOI
	;out	PIC_MASTER, al
	;cmp	ebx, 8
	;jb	eoi_done
	;out	PIC_SLAVE, al
;eoi_done:

	pop	ebx
	pop	eax
	add	esp, 4
	iretd

;_TEXT	ENDS
END

