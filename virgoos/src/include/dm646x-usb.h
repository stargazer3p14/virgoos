/*
 *	dm646x-usb.h
 *
 *	Definitions for DM646x dual-role USB controller
 */

#ifndef	DM646x_USB__H
#define DM646x_USB__H

// Base address for USB controller on DM6467
#define	DM646x_USB_BASE	0x01C64000

// Registers addresses (offsets)
#define	CTRLR	0x4
#define	STATR	0x8
#define	RNDISR	0x10
#define	AUTOREQ	0x14
#define	INTSRCR	0x20
#define	INTSETR	0x24
#define	INTCLRR	0x28
#define	INTMSKR	0x2C
#define	INTMSKSETR	0x30
#define	INTMSKCLRR	0x34
#define	INTMASKEDR	0x38
#define	EOIR	0x3C
#define	TCPPICR	0x80
#define	TCPPITDR	0x84
#define	CPPIEOIR	0x88
#define	TCPPIIVECTR	0x8C
#define	TCPPIMSKSR	0x90
#define	TCPPIRAWSR	0x94
#define	TCPPIENSETR	0x98
#define	TCPPIENCLRR	0x9C
#define	RCCPICR	0xC0
#define	RCPPIMSKSR	0xD0
#define	RCPPIRAWSR	0xD4
#define	RCPPIENSETR	0xD8
#define	RCPPIENCLRR	0xDC
#define	RBUFCNT0	0xE0
#define	RBUFCNT1	0xE4
#define	RBUFCNT2	0xE8
#define	RBUFCNT3	0xEC

// CPPI DMA state registers (a channel per EP)
#define	TCPPIDMASTATEW0	0x100
#define	TCPPIDMASTATEW1	0x104
#define	TCPPIDMASTATEW2	0x108
#define	TCPPIDMASTATEW3	0x10C
#define	TCPPIDMASTATEW4	0x110
#define	TCPPIDMASTATEW5	0x114
#define	TCPPICOMPPTR	0x11C
#define	RCPPIDMASTATEW0	0x120
#define	RCPPIDMASTATEW1	0x124
#define	RCPPIDMASTATEW2	0x128
#define	RCPPIDMASTATEW3	0x12C
#define	RCPPIDMASTATEW4	0x130
#define	RCPPIDMASTATEW5	0x134
#define	RCPPIDMASTATEW6	0x138
#define	RCPPICOMPPTR	0x13C

// Size of CPPI DMA state block size for one channel. Addresses given above are for channel 0; every channel # is offset by this value * channel number
// There are 4 channels (0-3)
#define	CPPI_CHANNEL_STATE_SIZE	0x40

// Common USB registers
#define	FADDR	0x400
#define	POWER	0x401
#define	INTRTX	0x402
#define	INTRRX	0x404
#define	INTRTXE	0x406
#define	INTRRXE	0x408
#define	INTRUSB	0x40A
#define	INTRUSBE	0x40B
#define	FRAME	0x40C
#define	INDEX	0x40E
#define	TESTMODE	0x40F

// Base for indexed registers (endpoint 0-4 is selected via INDEX register)
#define	EP_INDEX_BASE	0x410

// Base for EP1 offset register (registers for endpoint 1-4 is offset by EP_STATE_SIZE
#define	EP1_OFFSET_BASE	0x510

#define	EP_STATE_SIZE	0x20

// Endpoint state registers
#define	TXMAXP	0x0			// EP's 1-4
#define	PERI_TXCSR	0x2		// All EP's (have own meaning)
#define	PERI_CSR0	0x2
#define	HOST_TXCSR	0x2
#define	HOST_CSR0	0x2
#define	RXMAXP	0x4			// EP's 1-4
#define	PERI_RXCSR	0x6
#define	HOST_RXCSR	0x6		// EP's 1-4
#define	COUNT0	0x8			// All EP's (have own meaning)
#define	RXCOUNT	0x8
#define	HOST_TYPE0	0xA		// All EP's (have own meaning)
#define	HOST_TXTYPE	0xA
#define	HOST_NAKLIMIT0	0xB		// All EP's (have own meaning)
#define	HOST_TXINTERVAL	0xB
#define	HOST_RXTYPE	0xC		// EP's 1-4
#define	HOST_RXINTERVAL	0xD		// EP'a 1-4
#define	CONFIGDATA	0xF		// EP 0 only

// FIFOn. There's one transmit/receive FIFO register per endpoint, each EP is offset by 4 from previous one.
#define	FIFO0	0x420

// OTG device control (DM6467 is not an OTG device, why did they need this?
#define	DEVCTL	0x460

// Dynamic FIFO control. Those registers are indexed (i.e. INDEX register selects which EP's FIFO is programmed)
#define	TXFIFOSZ	0x462
#define	RXFIFOSZ	0x463
#define	TXFIFOADDR	0x464
#define	RXFIFOADDR	0x466

// Target endpoint registers - valid only in host mode.
// Registers for every EP are offset by 8 from the previous EP
#define	TARGET_EP_CTL_BASE	0x480
#define	TARGET_EP_CTL_SIZE	0x8

// Registers addresses
#define	TXFUNCADDR	0x0
#define	TXHUBADDR	0x2
#define	TXHUBPORT	0x3
#define	RXFUNCADDR	0x4
#define	RXHUBADDR	0x6
#define	RXHUBPORT	0x7

// USB mode
#define	USB_MODE_HOST	0
#define	USB_MODE_PERI	0x1

// Interrupts
#define	USB_IRQ	13
#define	USBDMA_IRQ	14

#endif	// DM646x_USB__H

