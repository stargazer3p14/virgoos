/*
 *	Generic header for I2C interface
 */

#ifndef I2C__H
#define I2C__H

// IOCTLS
#define	IOCTL_DM646x_I2C_READ	1
#define	IOCTL_DM646x_I2C_WRITE	2

// IOCTL interface structure
struct i2c_msg
{
	uint32_t	addr;	// Client address

#define	I2C_MSG_FLAG_STOP	0x1	// Generate stop condition at the end of transfer
#define	I2C_MSG_FLAG_XA	0x2		// use extended address (10-bits), if not set - 7 bits
	uint32_t	flags;
	size_t	len;
	void	*buf;
};

#endif	// I2C__H

