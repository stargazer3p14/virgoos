/*
 *	drvint.h
 *
 *	Definitions for device drivers interface.
 */

#ifndef	DRVINT__H
 #define	DRVINT__H

#include <stdarg.h>
#include "taskman.h"
 
/*
 *	Definitions & macros
 */
#define	ERROR_CONDITION	0x80000000

#define	DEV_ID(dev, subdev) (((dev << 16) + subdev) & (~ERROR_CONDITION))
#define	DEV(dev_id) (dev_id >> 16)
#define	SUBDEV(dev_id) (dev_id & 0xFFFF)

/*
 *	Device driver entry points.
 */

//	Initialization (power-up)
typedef	int (*drv_init)(unsigned id);

//	Deinitialization (power-down)
typedef int (*drv_deinit)(void);

//	Request to open a device. The driver returns 0 on success or a negative error.
typedef int (*drv_open)(unsigned  sub_id);

//	Request to read from a device. Returns number of bytes read or a negative error.
typedef int (*drv_read)(unsigned sub_id, void *buffer, unsigned long length);

//	Request to write to a device. Returns number of bytes written or a negative error.
typedef int (*drv_write)(unsigned sub_id, const void *buffer, unsigned long length);

//	IOCTL function. Returns non-negative unspecified value on success or a negative error.
typedef int (*drv_ioctl)(unsigned sub_id, int cmd, va_list argp);

//	Request to close the device. Returns 0 on success or a negative error.
typedef int (*drv_close)(unsigned sub_id);

//	Structure of driver interface entry points.
//	The driver must define such an entry.
typedef	struct drv_entry
{
	drv_init	init;
	drv_deinit	deinit;
	drv_open	open;
	drv_read	read;
	drv_write	write;
	drv_ioctl	ioctl;
	drv_close	close;
} drv_entry;

#define	DECLARE_DRIVER(name, irq_priority, dfc_priority)	\
 extern		struct drv_entry	name;	\
 extern	int	name##_irq_priority;	\
 extern	int	name##_dfc_priority;

#endif	// DRVINT__H

