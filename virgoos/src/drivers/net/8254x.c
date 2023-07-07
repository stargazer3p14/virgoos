/*
 *	8254x.c
 *
 *	Intel 8254x ethernet driver
 */

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "pcihost.h"
#include "8254x.h"

/*
 *	Initialize the ethernet controller (according to Software Developer's Manual's
 *	recommendation)
 */
int	i8254x_init(unsigned  drv_id)
{
	//
	return	0;
}

int	i8254x_deinit(void)
{
	return	0;
}


int	i8254x_open(unsigned  subdev_id)
{
	return	0;
}


int i8254x_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int i8254x_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int i8254x_ioctl(unsigned subdev_id, int cmd, unsigned long arg, ...)
{
	return	0;
}


int i8254x_close(unsigned sub_id)
{
	return	0;
}

struct drv_entry	i8254x = {i8254x_init, i8254x_deinit, i8254x_open, i8254x_read,
	i8254x_write, i8254x_ioctl, i8254x_close};

