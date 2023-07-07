/*
 *	UHCI HCD definitions
 */

#ifndef	UHCI__H
 #define UHCI__H

#include "sosdef.h"

// Register offsets from I/O base
#define	UHCI_REG_USBCMD			0x0		// USBCMD
#define	UHCI_REG_USBSTS			0x2		// USBSTS
#define	UHCI_REG_USBINTR		0x4		// USBINTR
#define	UHCI_REG_FRNUM			0x6		// FRNUM
#define	UHCI_REG_FLBASEADD		0x8		// FLBASEADD
#define	UHCI_REG_SOF_MODIFY		0xC		// SOF_MODIFY
#define	UHCI_REG_PORTSC1		0x10	// 1st port
#define	UHCI_REG_PORTSC2		0x12	// 2nd port

// USBCMD fields
#define	UHCI_CMD_MAXP			0x80
#define	UHCI_CMD_CF				0x40
#define	UHCI_CMD_SWDBG			0x20
#define	UHCI_CMD_FGR			0x10
#define	UHCI_CMD_EGSM			0x8
#define	UHCI_CMD_GRESET			0x4
#define	UHCI_CMD_HCRESET		0x2
#define	UHCI_CMD_RUN			0x1

#define	UHCI_CMD_START_VAL	(UHCI_CMD_RUN | UHCI_CMD_CF | UHCI_CMD_MAXP)

// USBSTS fields
#define	UHCI_STS_HCHALTED		0x20
#define	UHCI_STS_HC_PERROR		0x10
#define	UHCI_STS_SYS_ERROR		0x8
#define	UHCI_STS_RESUME_DETECT	0x4
#define	UHCI_STS_ERROR			0x2
#define	UHCI_STS_INT			0x1

// USBINTR fields
#define	UHCI_INTR_SHORT_PKT		0x8
#define	UHCI_INTR_IOC			0x4
#define	UHCI_INTR_RESUME		0x2
#define	UHCI_INTR_TMO_CRC		0x1

#define	UHCI_INTR_ENABLE	(UHCI_INTR_TMO_CRC | UHCI_INTR_RESUME | UHCI_INTR_IOC | UHCI_INTR_SHORT_PKT)

// PORTSC values
#define	UHCI_PORTSC_CONNECTED	0x1		// Device is connected
#define	UHCI_PORTSC_CONN_CHANGE	0x2		// Connect status change
#define	UHCI_PORTSC_PORT_EN	0x4		// Port enable/disable
#define	UHCI_PORTSC_PORT_EN_CHANGE	0x8		// Port enable/disable status changed
#define	UHCI_PORTSC_LOWSPEED_DEV	0x100	// Low-speed device is attached
#define	UHCI_PORTSC_PORT_RESET	0x200	// Port reset

#endif	// UHCI__H
