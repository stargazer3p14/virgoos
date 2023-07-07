#ifndef	TIMERS_ARCH__H
 #define	TIMERS_ARCH__H

// Base addresses for Timer0, Timer1 & Timer2
#define	TIMER0_BASE	0x01C21400
#define	TIMER1_BASE	0x01C21800
#define	TIMER2_BASE	0x01C21C00

// Registers
#define	PID12	0x0
#define	EMUMGT	0x4
#define	TIM12	0x10
#define	TIM34	0x14
#define	PRD12	0x18
#define	PRD34	0x1C
#define	TCR	0x20
#define	TGCR	0x24
#define	WDTCR	0x28

// PLL definitions
#define	PLL1_BASE	0x01C40800
#define	PLL2_BASE	0x01C40C00

// PLL registers
#define	PID	0x0
#define	PLLCTL	0x100
#define	PLLM	0x110

#endif //	TIMERS_ARCH__H

