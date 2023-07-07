/*
 *	Platform-specific part of taskman.h
 */

#ifndef	TASKMAN__ARCH__H
 #define TASKMAN__ARCH__H

#include "sosdef.h"

typedef	struct	REG_STATE
{
	dword	eax;	// [+0]
	dword	ebx;	// [+4]
	dword	ecx;	// [+8]
	dword	edx;	// [+12]
	dword	esi;	// [+16]
	dword	edi;	// [+20]
	dword	esp;	// [+24]
	dword	ebp;	// [+28]
	dword	eflags;	// [+32]
	dword	eip;	// [+36]
} __attribute__((packed)) REG_STATE;

typedef	struct	FP_STATE
{
	byte	st0[ 10 ];
	byte	st1[ 10 ];
	byte	st2[ 10 ];
	byte	st3[ 10 ];
	byte	st4[ 10 ];
	byte	st5[ 10 ];
	byte	st6[ 10 ];
	byte	st7[ 10 ];
} FP_STATE;

// x86 EFLAGS (relevant fields)
#define	FL_CF	0x1
#define	FL_PF	0x4
#define	FL_AF	0x10
#define	FL_ZF	0x40
#define	FL_SF	0x80
#define	FL_TF	0x100
#define	FL_IF	0x200
#define	FL_DF	0x400
#define	FL_OF	0x800

#define	_spin_lock(lock_word)\
do\
{\
	__asm__ __volatile__ ("push %eax\n" "push %ecx\n");\
	__asm__ __volatile__ ("mov	%%eax, 1\n" : : "c"(lock_word) );\
	__asm__ __volatile__ ("\n"\
"spin:\n"\
		"xchg	%eax, [%ecx]\n"\
		"test	%eax, %eax\n"\
		"jnz	spin\n");\
\
	__asm__ __volatile__ ("pop %ecx\n" "pop %eax\n");\
} while(0)


#define	spin_unlock(lock_word)\
do {\
	__asm__ __volatile__ ("push %eax\n" "push %ecx\n");\
	__asm__ __volatile__ ("sub %%eax, %%eax" : : "c"(lock_word) );\
	__asm__ __volatile__ ("xchg	%eax, [%ecx]\n");\
	__asm__ __volatile__ ("pop %ecx\n" "pop %eax\n");\
} while(0)


#endif // TASKMAN__ARCH__H

