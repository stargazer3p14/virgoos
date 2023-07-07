;
;	BOOTSEC.INC
;
;	Include file for SOS boot loader.
;


;
;	Hardcoded instructions
;
;JmpFar16	MACRO	Segm:REQ, Offs:REQ
%MACRO	JmpFar16	2
	DB	0EAh
;	DW	Offs
	DW	%2
;	DW	Segm
	DW	%1
;ENDM
%ENDMACRO


;JmpFar_16_32	MACRO	Segm:REQ, Offs:REQ
%MACRO	JmpFar_16_32	2
	DB	66h
	DB	0EAh
;	DD	Offs
	DD	%2
;	DW	Segm
	DW	%1
;ENDM
%ENDMACRO


;
;	Macros
;
;IODelay		MACRO
%MACRO	IODelay	0
; Delay after in/out instructions.
	push	ecx
	mov	ecx, 8000h
	loop	$
	pop	ecx
;ENDM
%ENDMACRO


;
;	32-bit descriptor definitions.
;
;Descriptor32	STRUC
STRUC Descriptor32
;	Limit16		DW	?			; Low word of segment limit
;	Base16		DW	?			; Low word of segment base address
;	Base24		DB	?			; Bits 16..23 of segment base address
;	Access		DB	?			; Segment access rights
;	Limit20_GD	DB	?			; Bits 16..19 of segment limit, Granularity & Default bits
;	Base32		DB	?			; Bits 24..31 of segment base address

	Limit16	RESW	1
	Base16	RESW	1
	Base24	RESB	1
	Access		RESB	1
	Limit20_GD	RESB	1
	Base32	RESB	1
;Descriptor32	ENDS
ENDSTRUC


;Gate32			STRUC
STRUC	Gate32
;	Offset16	DW	?			; Low word of entry point offset
;	Selector	DW	?			; Entry point code selector
;	Params		DB	?			; Number of parameters to copy (must be 0 for int/trap gates)
;	GateType	DB	?			; Gate type
;	Offset32	DW	?			; High word of entry point offset

	Offset16	RESW	1
	Selector	RESW	1
	Params	RESB	1
	GateType	RESB	1
	Offset32	RESW	1
;Gate32			ENDS
ENDSTRUC


;-----------------------------------------------------------------------------
;	CPU control registers.
;-----------------------------------------------------------------------------
CR0_PG	EQU	80000000h	; Paging enable
CR0_CD	EQU	40000000h	; Caching disable
CR0_NW	EQU	20000000h	; Non-write through
CR0_PE	EQU	00000001h	; Protection enable (protected mode).

;-----------------------------------------------------------------------------
;	Eflags register.
;-----------------------------------------------------------------------------
FL_VM	EQU	00020000h	; VM flag
FL_NT	EQU	00004000h	; NT flag
FL_CF	EQU	00000001h	; C flag
FL_PF	EQU	00000004h	; P flag
FL_AF	EQU	00000010h	; AC flag
FL_ZF	EQU	00000040h	; Z flag
FL_SF	EQU	00000080h	; S flag
FL_TF	EQU	00000100h	; T flag
FL_IF	EQU	00000200h	; I flag
FL_DF	EQU	00000400h	; D flag
FL_OF	EQU	00000800h	; O flag
FL_RF	EQU	00010000h	; R flag

;
;	DTR structure (GDTR/IDTR).
;
;DTR	STRUC
STRUC	DTR
;	Limit	DW	?
;	Base	DD	?
	Limit	RESW	1
	Base	RESD	1
;DTR	ENDS
ENDSTRUC

; Access rights
	ACC_PRESENT		EQU	10000000b
	ACC_CODE		EQU	00011000b
	ACC_CONFORM_CODE	EQU	00000100b
	ACC_READABLE_CODE	EQU	00000010b
	ACC_DATA		EQU	00010000b
	ACC_EXPDOWN_DATA	EQU	00000100b
	ACC_WRITABLE_DATA	EQU	00000010b
	ACC_ACCESSED_SEGMENT	EQU	00000001b
	ACC_INT_GATE		EQU	00000110b
	ACC_CALL_GATE		EQU	00000100b
	ACC_TRAP_GATE		EQU	00000111b
	ACC_INT386_GATE		EQU	00001110b
	ACC_CALL386_GATE	EQU	00001100b
	ACC_TRAP386_GATE	EQU	00001111b
	ACC_TASK_GATE		EQU	00000101b
	ACC_TSS			EQU	00000001b
	ACC_TSS386		EQU	00001001b
	ACC_LDT			EQU	00000010b

; Segment attributes (386+)
	ATTR_DEF		EQU	01000000b	; D/B attr.
	ATTR_GRAN		EQU	10000000b	; Granularity bit.
;	FLAT_ATTR		EQU	ATTR_DEF or ATTR_GRAN
	FLAT_ATTR		EQU	ATTR_DEF | ATTR_GRAN



; Live segments access rights
;	DATA_ACCESS	=	ACC_PRESENT or ACC_DATA or ACC_WRITABLE_DATA
;	CODE_ACCESS	=	ACC_PRESENT or ACC_CODE or ACC_READABLE_CODE
;	INT_ACCESS	=	ACC_PRESENT or ACC_INT_GATE
;	TRAP_ACCESS	=	ACC_PRESENT or ACC_TRAP_GATE
;	INT_386_ACCESS	=	ACC_PRESENT or ACC_INT386_GATE
;	TRAP_386_ACCESS	=	ACC_PRESENT or ACC_TRAP386_GATE
;	TSS_ACCESS	=	ACC_PRESENT or ACC_TSS
;	TASK_GATE_ACCESS = 	ACC_PRESENT or ACC_TASK_GATE
;	TSS386_ACCESS	=	ACC_PRESENT or ACC_TSS386
;	LDT_ACCESS	=	ACC_PRESENT or ACC_LDT

	DATA_ACCESS	 EQU	ACC_PRESENT | ACC_DATA | ACC_WRITABLE_DATA
	CODE_ACCESS	EQU	ACC_PRESENT | ACC_CODE | ACC_READABLE_CODE
	INT_ACCESS	EQU	ACC_PRESENT | ACC_INT_GATE
	TRAP_ACCESS	EQU	ACC_PRESENT | ACC_TRAP_GATE
	INT_386_ACCESS	EQU	ACC_PRESENT | ACC_INT386_GATE
	TRAP_386_ACCESS	EQU	ACC_PRESENT | ACC_TRAP386_GATE
	TSS_ACCESS	EQU	ACC_PRESENT | ACC_TSS
	TASK_GATE_ACCESS EQU 	ACC_PRESENT | ACC_TASK_GATE
	TSS386_ACCESS	EQU	ACC_PRESENT | ACC_TSS386
	LDT_ACCESS	EQU	ACC_PRESENT | ACC_LDT


; Addresses definitions
	CODE_32		EQU		08h
	DATA_32		EQU		10h
	APP_START	EQU		100000h
	NT_HDR_PTR	EQU		3Ch
	ENTRY_PTR	EQU		4 + 20 + 16			; Signature + sizeof(IMAGE_FILE_HEADER) + offsetof(IMAGE_OPTIONAL_HEADER, AddressOfEntryPoint)
	IMAGE_BASE	EQU		4 + 20 + 28			; Signature + sizeof(IMAGE_FILE_HEADER) + offsetof(IMAGE_OPTIONAL_HEADER, ImageBase)
	;STACK_START	EQU		8FFFEh
	STACK_START	EQU		200000h

; Keyboard ports.
	KBD_DATA	EQU	60h
	KBD_PORT_B	EQU	61h
	KBD_STATUS	EQU	64h
	KBD_COMMAND	EQU	64h

; Keyboard controller commands (command port, 64h - wr)
	CMD_ShutDown	EQU	0FEh	; (~Pulse) output port. Bit 0 of
					; output port conn. to CPU reset line
	CMD_A20_Access	EQU	0D1h	; Write output port command.

; Keyboard controller commands (output port)
	CMD_Kbd_Data	EQU	80h
	CMD_Kbd_Clock	EQU	40h
	CMD_Empty_Input	EQU	20h
	CMD_Output_Full	EQU	10h
	CMD_A20		EQU	02h
	CMD_Rst_Off	EQU	01h
	CMD_Undef	EQU	0Ch
;	CMD_A20_On	EQU	CMD_Undef OR CMD_Rst_Off OR CMD_A20 OR CMD_Output_Full OR CMD_Kbd_Clock OR CMD_Kbd_Data
;	CMD_A20_Off	EQU	CMD_A20_On AND (NOT CMD_A20)
	CMD_A20_On	EQU	CMD_Undef | CMD_Rst_Off | CMD_A20 | CMD_Output_Full | CMD_Kbd_Clock | CMD_Kbd_Data
	CMD_A20_Off	EQU	CMD_A20_On & (~ CMD_A20)

; PIC ports.
	PIC_MASTER	EQU	20h
	PIC_MASTER_MASK	EQU	21h
	PIC_SLAVE	EQU	0A0h
	PIC_SLAVE_MASK	EQU	0A1h

; PIC init commands (Initialization Command Words).
	CMD_ICW1	EQU	11h	; ICW4 needed, cascading 8259h,
					; 8-byte vectors, edge-triggered mode
	CMD_ICW2	EQU	0	; bits 3-7 = A3-A7 int vector.
	CMD_ICW3	EQU	04h	; IRQ 2 has slave, rest - no slave.
	CMD_SLAVE_ICW3	EQU	02h	; Bits 0-2 = master's cascade vector.
	CMD_ICW4	EQU	01h	; Normal EOI, no buffering,
					; sequential mode.
; PIC Operation control words. (reg. select for next read)
	CMD_EOI		EQU	20h	; OCW 2: non-specific EOI, upon IRQ 0
	CMD_Read_IRR	EQU	0Ah	; Read IRR on next read.
	CMD_Read_ISR	EQU	0Bh	; Read ISR on next read.

; Base interrupt vectors
	PIC0_BASE_INT	EQU	20h
	PIC1_BASE_INT	EQU	28h

