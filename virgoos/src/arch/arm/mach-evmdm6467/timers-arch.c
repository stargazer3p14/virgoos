/*
 *	System-dependent (DM6467) part of timers code
 */

#include "sosdef.h"
#include "timers.h"
#include "config.h"

extern time_t	system_time;
extern dword	timer_counter;
extern dword	uptime;					// In seconds
int	timer_isr(void);

// Timer support for DM6467
void	init_timer0(void)
{
	volatile unsigned long	ticks;

#if 0
	// Experiments show that reference TIMCLK changed from 32KHz external source to 1MHz
	outd(TIMER0_BASE + TIMER1_LOAD, 1000000 / TICKS_PER_SEC);
	// Set timer mode: enable, periodic, interrupt enable, size = 32-bit, wrapping, prescale = 00
	outd(TIMER0_BASE + TIMER1_CONTROL, (TIMER_CONTROL_EN | TIMER_CONTROL_MODE | TIMER_CONTROL_INTEN | TIMER_CONTROL_SIZE));
#endif

#if 0
// Dump timer registers (this is generally an illegal place to call serial_printf(), as it comes before initializing UART. We use it here "illegally" because we trust u-boot to have initialized the UART properly (and meanwhile we don't change its configuration)
	serial_printf("Dump Timer0 regs at %08X:\n"
			"[PID12]=%08X [EMUMGT]=%08X [TIM12]=%08X [TIM34]=%08X [PRD12]=%08X [PRD34]=%08X [TCR]=%08X [TGCR]=%08X [WDTCR]=%08X\n",
			TIMER0_BASE, ind(TIMER0_BASE+PID12), ind(TIMER0_BASE+EMUMGT), ind(TIMER0_BASE+TIM12), ind(TIMER0_BASE+TIM34), ind(TIMER0_BASE+PRD12), ind(TIMER0_BASE+PRD34), ind(TIMER0_BASE+TCR), ind(TIMER0_BASE+TGCR), ind(TIMER0_BASE+WDTCR));
	serial_printf("Dump Timer1 regs at %08X:\n"
			"[PID12]=%08X [EMUMGT]=%08X [TIM12]=%08X [TIM34]=%08X [PRD12]=%08X [PRD34]=%08X [TCR]=%08X [TGCR]=%08X [WDTCR]=%08X\n",
			TIMER1_BASE, ind(TIMER1_BASE+PID12), ind(TIMER1_BASE+EMUMGT), ind(TIMER1_BASE+TIM12), ind(TIMER1_BASE+TIM34), ind(TIMER1_BASE+PRD12), ind(TIMER1_BASE+PRD34), ind(TIMER1_BASE+TCR), ind(TIMER1_BASE+TGCR), ind(TIMER1_BASE+WDTCR));
	serial_printf("Dump Timer2 regs at %08X:\n"
			"[PID12]=%08X [EMUMGT]=%08X [TIM12]=%08X [TIM34]=%08X [PRD12]=%08X [PRD34]=%08X [TCR]=%08X [TGCR]=%08X [WDTCR]=%08X\n",
			TIMER2_BASE, ind(TIMER2_BASE+PID12), ind(TIMER2_BASE+EMUMGT), ind(TIMER2_BASE+TIM12), ind(TIMER2_BASE+TIM34), ind(TIMER2_BASE+PRD12), ind(TIMER2_BASE+PRD34), ind(TIMER2_BASE+TCR), ind(TIMER2_BASE+TGCR), ind(TIMER2_BASE+WDTCR));
#endif

// It appears that u-boot doesn't actively use timers, nor it configures them to anything sound. Timer1 and Timer2 modules are held at reset, and Timer0 module holds strange value indicating that its timer0:1 is at reset and timer2:3 - not, while timer is in unchained
// 32-bit mode. Does u-boot use timer2:3 only? What are the interrupt settings?

// We will use the following timer configuration:
// 	Timer0 is used as system timer, we will think of Timer1 later (Timer2 is watchdog, and can hardly be used for anything else)
// 	64-bit mode
//	internal clock source (referenced from PLL0) - we will KNOW this one from CPU's specification. Timer's clock is 1/4 of DSP's clock and 1/2 of ARM's clock. For different models with different clocks of the same CPU we will have to check model/revision
// numbers and set up timer period value accordingly. For now, we assume 27-MHz clock source and 22x multiplier (let's check that last one)

#if 0
	serial_printf("Dump PLL1 some regs at %08X:\n"
			"[PID]=%08X [PLLM]=%08X\n", PLL1_BASE, ind(PLL1_BASE+PID), ind(PLL1_BASE+PLLM));
#endif

	// Reset Timer0 (timer1:2 and timer3:4)
	outd(TIMER0_BASE+TGCR, 0x0);

#if 0
	for (ticks=0; ticks < 1000; ++ticks)
		;
#endif

	// TGCR: TIM12RS = 1 (out of reset), TIM32RS = 1 (out of reset), TIMMODE = 0 (64-bit GP timer mode), PSC34 = 0, TDDR34 = 0 (no prescaling, if it at all matters)
	outd(TIMER0_BASE+TGCR, 0x3);
	// Reset counters to 0
	outd(TIMER0_BASE+TIM12, 0);
	outd(TIMER0_BASE+TIM34, 0);
	// Set period to 148500 <-> 1 KHz
	outd(TIMER0_BASE+PRD12, 148500000 / TICKS_PER_SEC);
	outd(TIMER0_BASE+PRD34, 0);
	// TCR: ENAMODE12 = 2 (continous mode), CLKSRC12 = 0 (internal-PLL clock), ENAMODE34 = 0 (don't care), CLKSRC34 = 0 (internal-PLL clock)
	outd(TIMER0_BASE+TCR, 0x00000080);

#if 0
	serial_printf("After SeptOS initialization -- Dump Timer0 regs at %08X:\n"
			"[PID12]=%08X [EMUMGT]=%08X [TIM12]=%08X [TIM34]=%08X [PRD12]=%08X [PRD34]=%08X [TCR]=%08X [TGCR]=%08X [WDTCR]=%08X\n",
			TIMER0_BASE, ind(TIMER0_BASE+PID12), ind(TIMER0_BASE+EMUMGT), ind(TIMER0_BASE+TIM12), ind(TIMER0_BASE+TIM34), ind(TIMER0_BASE+PRD12), ind(TIMER0_BASE+PRD34), ind(TIMER0_BASE+TCR), ind(TIMER0_BASE+TGCR), ind(TIMER0_BASE+WDTCR));

	for (ticks=0; ticks < 1000; ++ticks)
		;

	serial_printf("After waiting %lu ticks -- Dump Timer0 regs at %08X:\n"
			"[PID12]=%08X [EMUMGT]=%08X [TIM12]=%08X [TIM34]=%08X [PRD12]=%08X [PRD34]=%08X [TCR]=%08X [TGCR]=%08X [WDTCR]=%08X\n", ticks,
			TIMER0_BASE, ind(TIMER0_BASE+PID12), ind(TIMER0_BASE+EMUMGT), ind(TIMER0_BASE+TIM12), ind(TIMER0_BASE+TIM34), ind(TIMER0_BASE+PRD12), ind(TIMER0_BASE+PRD34), ind(TIMER0_BASE+TCR), ind(TIMER0_BASE+TGCR), ind(TIMER0_BASE+WDTCR));
#endif
}

static int	bcd_to_num(int bcd)
{
	return	(bcd >> 4 & 0xF) * 10 + bcd & 0xF;
}

// TODO learn how to work with RTC / Maxim day clock
// It's operated via I2C bus, need to study how to do it
void	plat_init_sys_time(void)
{
#if 0
	int	date, month, year;
	int	hour, min, sec;
	struct tm	tm;

	outb(CMOS_ADDR, CMOS_RTC_SECONDS);
	sec = bcd_to_num((time_t)inb(CMOS_DATA));
	outb(CMOS_ADDR, CMOS_RTC_MINUTES);
	min = bcd_to_num((time_t)inb(CMOS_DATA));
	outb(CMOS_ADDR, CMOS_RTC_HOURS);
	hour = bcd_to_num((time_t)inb(CMOS_DATA));
	outb(CMOS_ADDR, CMOS_RTC_DATE);
	date = bcd_to_num(inb(CMOS_DATA));
	outb(CMOS_ADDR, CMOS_RTC_MONTH);
	month = bcd_to_num(inb(CMOS_DATA));
	outb(CMOS_ADDR, CMOS_RTC_YEAR);
	year = bcd_to_num(inb(CMOS_DATA));
	
	//system_time = date_to_canonic(date, month, year) * 3600 * 24 + sec + min * 60 + hour * 3600;
	tm.tm_year = year;
	tm.tm_mon = month-1;
	tm.tm_mday = date;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;
	system_time = mktime(&tm); 
	serial_printf("%s(): time is %d/%d/%d %d:%d:%d, system_time = %u\r\n", __func__, 
		date, month, year, hour, min, sec, system_time);
#endif
}

void	plat_init_timers(void)
{
	init_timer0();
	set_int_callback(TINTL0_IRQ, timer_isr);
	serial_printf("Timer ISR is installed at int %d\n", TINTL0_IRQ);
}


// On DM6467 there's no special timer interrupt acknowledgement
void	plat_timer_eoi(void)
{
}


/*
 *	Returns amount of timer ticks in dest (an array of 2 dwords), treating as 64-bit int
 *
 *	(???) Probably useless as timestamp with us resolution: 8254 counters are reloaded twice.
 *	For ms resolution timer_counter may be used.
 *	On Pentium or better CPU timestamp may be used
 */
void	timestamp_us(struct timestamp *dest)
{
#if 0
	__asm__ __volatile__ ("pushfd\n"
						"cli\n");
	outb(CMD_8254, 0x6);				// Binary, mode 3, latch counters
	dest->ticks = inb(CH0_8254);			// Read LSB
	dest->ticks += inb(CH0_8254) << 8;	// Read MSB
	dest->ticks += timer_counter * (CLOCK_8254 / TICKS_PER_SEC);
	dest->ticks %= CLOCK_8254;
	dest->sec = uptime;
	__asm__ __volatile__ ("popfd\n");
#endif
}

