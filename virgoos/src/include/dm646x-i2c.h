/*
 *	dm646x-i2c.h
 *
 *	Header file for DM646x on-chip I2C controller
 */

#ifndef	DM646x_I2C__H
#define	DM646x_I2C__H

#include "i2c.h"

#define	DM646x_I2C_BASE	0x01C21000

// Registers definition
#define	ICOAR	0x0

#define	ICIMR	0x4
// ICIMR bit values
#define	ICIMR_AAS	0x40
#define	ICIMR_SCD	0x20
#define	ICIMR_ICXRDY	0x10
#define	ICIMR_ICRRDY	0x8
#define	ICIMR_ARDY	0x4
#define	ICIMR_NACK	0x2
#define	ICIMR_AL	0x1

#define	DM646x_I2C_INTR_MASK	(ICIMR_AAS | ICIMR_SCD | ICIMR_ARDY | ICIMR_NACK | ICIMR_AL)

#define	ICSTR	0x8

// ICSTR values 
#define	ICSTR_BB	0x1000
#define	ICSTR_AAS	0x200
#define	ICSTR_SCD	0x20
#define	ICSTR_ICXRDY	0x10
#define	ICSTR_ICRRDY	0x8
#define	ICSTR_ARDY	0x4
#define	ICSTR_NACK	0x2
#define	ICSTR_AL	0x1

#define	ICCLKL	0xC
#define	ICCLKH	0x10
#define	ICCNT	0x14
#define	ICDRR	0x18
#define	ICSAR	0x1C
#define	ICDXR	0x20
#define	ICMDR	0x24

// ICMDR bits
#define	IRS	0x20		// I2C reset (1 - enabled, 0 - in reset)
#define	XA	0x100		// Extended address (1 - 10-bit address, 0 - 7-bit address)
#define	TRX	0x200		// Transmit mode (1 - transmit, 0 - receive)
#define	MST	0x400		// Master mode
#define	STP	0x800		// Stop condition (1 - generate stop condition when counter-down reaches 0)
#define	STT	0x2000		// Start condition 

#define	ICIVR	0x28
#define	ICEMDR	0x2C
#define	ICPSC	0x30
#define	ICPID1	0x34
#define	ICPID2	0x38


#endif	// DM646x_I2C__H

