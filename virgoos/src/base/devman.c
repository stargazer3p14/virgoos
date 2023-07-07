/*
 *	devman.c
 *
 *	Support routines for device drivers.
 */

#define	DEVMAN__C

#include "config.h"
#include "sosdef.h"
#include "drvint.h"
#include "timers.h"
#include "errno.h"

static	void	timer_handler(void *unused);

timer_t	cal_udelay_timer = {1, 0, TICKS_PER_SEC, TF_PERIODIC, 0, timer_handler, NULL};
volatile int	udelay_cal_started = 0;
volatile int	udelay_cal_done;

static	void	timer_handler(void *unused)
{
	if (udelay_cal_started < 1 << 10)
		++udelay_cal_started;
	else
	{
		udelay_cal_done = 1;
		calibrated_udelay_count1 = 0;
		calibrated_udelay_count2 = 0;
		remove_timer(&cal_udelay_timer);
	}
}

// Returns device ID that corresponds to name (-1 if no such device)
// TODO: currently this opens only subdev 0. Introduce naming convention for sub-devices
int dev_name_to_id(const char *name)
{
	int	i;

	for (i = 0; i < NUM_DEV_TBL_ENTRIES; ++i)
		if (strcmp(name, dev_tbl[i].dev_name) == 0)
			return	dev_tbl[i].dev_id;

	return	-1;
}

void	init_devman(void)
{
	int	i;
#if CALIBRATE_UDELAY
	volatile unsigned	n = 1000000, c, c1, c2;
#endif

	
	for (i = 0; i < NUM_DRIVER_ENTRIES; ++i)
		driver_entries[i]->init(i);

#if CALIBRATE_UDELAY
// Calibrate udelay()
// It is assumed that between two timer interrupts there won't be enough time to roll over 32-bit dummy counter

#if 0
	calibrated_usec = 1;
#else
	calibrated_udelay_count1 = UINT_MAX;
	calibrated_udelay_count2 = UINT_MAX;
#endif
	install_timer(&cal_udelay_timer);
	
	enable_irqs();
#if 0
	while (n-- > 0)
	{
		for (c = 0; udelay_cal_done < calibrated_usec; ++c)
			;
		break;
	}
#else
	for (c1 = 0; c1 <= calibrated_udelay_count1; ++c1)
	{
		for (c2 = 0; c2 < calibrated_udelay_count2; ++c2)
			;
		if (c1 == calibrated_udelay_count1)
			break;
	}
#endif
	disable_irqs();
#if 0
	calibrated_usec = c / (1000000U / TICKS_PER_SEC) / 10;
	serial_printf("%s(): calibrated udelay() = %u loops (c=%u)\n", __func__, calibrated_usec, c);
#else
	serial_printf("%s(): calibrated udelay() -- before calculation: c1=%u, c2=%u\n", __func__, c1, c2);

	c2 >>= 10;
	c2 |= c1 << 32 - 10;
	c1 >>= 10;

	// We've got counter for 1 system timer tick (TICKS_PER_SEC). Need to divide it by (1000000U / TICKS_PER_SEC), so let's assume that c2 == 0. With TICKS_PER_SEC == 1000 it will be true for even 4GHz CPU by 3 orders of magnitude
	c2 /= (1000000U / TICKS_PER_SEC);

	calibrated_udelay_count1 = c1;
	calibrated_udelay_count2 = c2;
	serial_printf("%s(): calibrated udelay(): c1=%u, c2=%u\n", __func__, c1, c2);
#endif

#ifdef	evmdm6467 
#if 0
// This test on DM6467 takes 56.8 seconds instead of 60. Fairly accurate for such big delays using udelay()!
// Fixing this also helped find out disabled caches

// Check if we run with cache enabled
	{
		dword	c1;

		__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r"(c1));
		serial_printf("CP15 c1 = %08X\n", c1);
	}

	serial_printf("Using udelay(1000000)");
	for (i = 0; i < 60; ++i)
	{
		serial_printf(".");
		udelay(1000000);
	}
	serial_printf("done\n");
#endif // 0
#endif // evmdm6467
#endif // CALIBRATE_UDELAY

}

void deinit_devman(void)
{
	int	i;

	for (i = 0; i < NUM_DRIVER_ENTRIES; ++i)
		driver_entries[i]->deinit();
}

int open_drv(unsigned long drv_id)
{
	errno = 0;
	if (driver_entries[DEV(drv_id)]->open != NULL)
		return	driver_entries[DEV(drv_id)]->open(SUBDEV(drv_id));
	else
	{
		errno = EINVAL;
		return	-1;
	}
}

int read_drv(unsigned long drv_id, void *buffer, unsigned long length)
{
	errno = 0;
	if (driver_entries[DEV(drv_id)]->read != NULL)
		return	driver_entries[DEV(drv_id)] -> read(SUBDEV(drv_id), buffer, length);
	else
	{
		errno = EINVAL;
		return	-1;
	}
}

extern int serial_debug;

int write_drv(unsigned long drv_id, const void *buffer, unsigned long length)
{
	errno = 0;
	if (driver_entries[DEV(drv_id)]->write != NULL)
		return	driver_entries[DEV(drv_id)]->write(SUBDEV(drv_id), buffer, length);
	else
	{
		errno = EINVAL;
		return	-1;
	}
}

int ioctl_drv(unsigned long drv_id, int cmd, ...)
{
	int	rv;
	va_list	argp;

	va_start(argp, cmd);
	errno = 0;
	if (driver_entries[DEV(drv_id)]->ioctl != NULL)
		rv = driver_entries[DEV(drv_id)]->ioctl(SUBDEV(drv_id), cmd, argp);
	else
	{
		errno = EINVAL;
		rv = -1;
	}
	va_end(argp);

	return	rv;
}

int close_drv(unsigned long drv_id)
{
	errno = 0;
	if (driver_entries[DEV(drv_id)]->close != NULL)
		return	driver_entries[DEV(drv_id)]->close(SUBDEV(drv_id));
	else
	{
		errno = EINVAL;
		return	-1;
	}
}

