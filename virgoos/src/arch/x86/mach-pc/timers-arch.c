/*
 *	System-dependent (x86) part of timers code
 */

#include	"sosdef.h"
#include	"timers.h"
#include	"config.h"

extern time_t	system_time;
extern dword	timer_counter;
extern dword	uptime;					// In seconds
int	timer_isr(void);

void	init_8254()
{
	unsigned long	ticks;

	outb( CMD_8254, 0x36 );		// Binary, Mode 3 - periodic quad. wave, write LSB then MSB, Ch 0
	ticks = CLOCK_8254 / TICKS_PER_SEC;
	outb( CH0_8254, ( byte )( ticks & 0xFF ) );
	outb( CH0_8254, ( byte )( ( ticks >> 8 ) & 0xFF ) );
}

static int	bcd_to_num(int bcd)
{
	return	(bcd >> 4 & 0xF) * 10 + bcd & 0xF;
}

void	plat_init_sys_time(void)
{
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
}

void	plat_init_timers(void)
{
	init_8254();
	set_int_callback(TIMER_IRQ, timer_isr);
}

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
	__asm__ __volatile__ ("pushfd\n"
						"cli\n");
	outb(CMD_8254, 0x6);				// Binary, mode 3, latch counters
	dest->ticks = inb(CH0_8254);			// Read LSB
	dest->ticks += inb(CH0_8254) << 8;	// Read MSB
	dest->ticks += timer_counter * (CLOCK_8254 / TICKS_PER_SEC);
	dest->ticks %= CLOCK_8254;
	dest->sec = uptime;
	__asm__ __volatile__ ("popfd\n");
}

