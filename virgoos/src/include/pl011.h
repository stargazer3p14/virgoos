/*
 *	Definitions for PL011 UART driver (ARM versatile boards)
 */
#ifndef	PL011__H
 #define PL011__H

#define	UART0_BASE	0x101F1000
#define	UART1_BASE	0x101F2000
#define	UART2_BASE	0x101F3000

#define	UARTDR		0x0	// Data register

#define	UARTRSR		0x4	// Receive status register (errors)

#define	UARTFR		0x18	// Flags register
#define	UARTFR_BUSY	0x8	// BUSY transmitting data
#define	UARTFR_RXFE	0x10	// Receive FIFO/register empty
#define	UARTFR_TXFF	0x20	// Transmit FIFO/register full
#define	UARTFR_RXFF	0x40	// Receive FIFO/register full
#define	UARTFR_TXFE	0x80	// Transmit FIFO/register empty

#define	UARTILPR	0x20	// IRDA low-power counter
#define	UARTIBRD	0x24	// Integer baud rate divisor
#define	UARTFBRD	0x28	// Fractional baud rate divisor
#define	UARTLCR_H	0x2C	// Line control register (high?)
#define	UARTCR		0x30	// Control register
#define	UARTIFLS	0x34	// FIFO interrupt level select

#define	UARTIMSC	0x38	// Interrupt mask set/clear register
#define	UARTRIS		0x3C	// Raw interrupt status
#define	UARTMIS		0x40	// Masked interrupt status
#define	UARTICR		0x44	// Interrupt clear register
#define	UARTDMACR	0x48	// DMA control

#define	UART_RINT	0x10	// Receive interrupt
#define	UART_TINT	0x20	// Transmit interrupt

//
// Configurable parameters
//
#define	CFG_UART_BAUDRATE	115200

#define	CFG_UART_INT	1
#if	CFG_UART_INT
#define	CFG_UART_IMSC_VAL	UART_RINT
#else
#define	CFG_UART_IMSC_VAL	0
#endif


// Add CR (\r) to each LF (\n) output
#define	UART_MODE_UNIX	1

#endif // PL011__H

