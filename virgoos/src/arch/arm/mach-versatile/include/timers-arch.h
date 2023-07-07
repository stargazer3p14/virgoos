#ifndef	TIMERS_ARCH__H
 #define	TIMERS_ARCH__H

#define	TIMER0_BASE	0x101E2000	// Timer0/1 base
#define	TIMER2_BASE	0x101E3000	// Timer2/3 base

#define	TIMER1_LOAD	0x0
#define	TIMER1_VALUE	0x4
#define	TIMER1_CONTROL	0x8

#define	TIMER_CONTROL_EN	0x80	// Timer is enabled (1 - enabled, 0 - disabled, default)
#define	TIMER_CONTROL_MODE	0x40	// 1 - periodic, 0 - free-running (default; reloads with 0xFFFFFFFF/0xFFFF rather than from TIMERX_LOAD)
#define	TIMER_CONTROL_INTEN	0x20	// interrupt enabled (1 - enabled, default)
#define	TIMER_CONTROL_SIZE	0x2	// 1 - 32-bit, 0 - 16-bit counter (default)
#define	TIMER_CONTROL_ONESHOT	0x1	// 1 - one-shot, 0 - wrapping (default)

#define	TIMER1_INT_CLR	0xC
#define	TIMER1_RIS	0x10
#define	TIMER1_MIS	0x14
#define	TIMER1_BGLOAD	0x18
#define	TIMER2_LOAD	0x20
#define	TIMER2_VALUE	0x24
#define	TIMER2_CONTROL	0x28
#define	TIMER2_INT_CLR	0x2C
#define	TIMER2_RIS	0x30
#define	TIMER2_MIS	0x34
#define	TIMER2_BGLOAD	0x38

#endif //	TIMERS_ARCH__H

