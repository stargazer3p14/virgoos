/*
 *	Platform-specific part of taskman.h
 */

#ifndef	TASKMAN__ARCH__H
 #define TASKMAN__ARCH__H

#include "sosdef.h"

//
//	"rXX" below corresponds to "$XX" in MIPS assembler nomenclature
//
typedef	struct	REG_STATE
{
	// $0 0/scratch register, no need to save it. It reads 0 in any case
	dword	r1;	// [+0]
	dword	r2;	// [+4]
	dword	r3;	// [+8]
	dword	r4;	// [+12]
	dword	r5;	// [+16]
	dword	r6;	// [+20]
	dword	r7;	// [+24]
	// $8-$15 (temporaries) - no need to save them. If task switch is made in IRQ context, IRQ entry point has already saved them on stack, and will restore upon return
	// If task switch is result of a blocking syscall, then no need to save them by definition
	dword	r16;	// [+28]
	dword	r17;	// [+32]
	dword	r18;	// [+36]
	dword	r19;	// [+40]
	dword	r20;	// [+44]
	dword	r21;	// [+48]
	dword	r22;	// [+52]
	dword	r23;	// [+56]
	// $24-25 are temporaries for expression evaluation. The same as for $8-$15 applies
	// $26-$27 are OS kernel temporaries - no need to save them
	dword	r28;	// [+60]
	dword	r29;	// [+64]
	dword	r30;	// [+68]
	dword	r31;	// [+72]
	dword	epc;	// [+76]	When task switch occurs, the switched-off PC is either in ra ($31) or in EPC (CP0 $14 sel. 0)
	dword	status;	// [+80]	CP0 status ($12 sel 0)
	dword	pc;	// [+84]	PC (most probably in switch_task() or reschedule(), but more options may appear later)
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

