/*
 *	Platform-specific part of taskman.h
 */

#ifndef	TASKMAN__ARCH__H
 #define TASKMAN__ARCH__H

#include "sosdef.h"

typedef	struct	REG_STATE
{
	dword	r0;	// [+0]
	dword	r1;	// [+4]
	dword	r2;	// [+8]
	dword	r3;	// [+12]
	dword	r4;	// [+16]
	dword	r5;	// [+20]
	dword	r6;	// [+24]
	dword	r7;	// [+28]
	dword	r8;	// [+32]
	dword	r9;	// [+36]
	dword	r10;	// [+40]
	dword	r11;	// [+44]
	dword	r12;	// [+48]
	dword	r13;	// [+52]		// SP
	dword	r14;	// [+56]		// LR
	dword	r15;	// [+60]		// PC
	dword	cpsr;	// [+64]		// Flags
	dword	spsr0;	// [+68]		// Those are special (system? status regs, one for each exception mode). We will yet figure out if we neet to preserve them all
	dword	spsr1;	// [+72]
	dword	spsr2;	// [+76]
	dword	spsr3;	// [+80]
	dword	spsr4;	// [+84]
}	REG_STATE;


typedef	struct	FP_STATE
{
/*
	byte	st0[ 10 ];
	byte	st1[ 10 ];
	byte	st2[ 10 ];
	byte	st3[ 10 ];
	byte	st4[ 10 ];
	byte	st5[ 10 ];
	byte	st6[ 10 ];
	byte	st7[ 10 ];
*/
}	FP_STATE;

// TODO: spin_lock() and spin_unlock()
#define	_spin_lock(lock_word)\
do\
{\
} while(0)


#define	spin_unlock(lock_word)\
do {\
} while(0)

#endif // TASKMAN__ARCH__H

