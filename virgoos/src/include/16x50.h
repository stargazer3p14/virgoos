/*
 *	16x50.h
 *
 *	8250/16450/16550/16650/16750 UART driver header.
 */

//	Ports numbers
#ifdef evmdm6467
// DM6467 has 16x50-compatible UART, but at different addresses
 #define UART1_BASE	0x01C20000
 #define UART2_BASE	0x01C20400
 #define UART3_BASE	0x01C20800
 #define UART4_BASE	0x01C20000		// DM646x has only 3 UARTs, so meanwhile we will make UART4 alias to UART1

 #define UART_16x50_REG_SHIFT	4

 #define UART1_IRQ	UARTINT0_IRQ

#else
 #define UART1_BASE	0x3F8
 #define UART2_BASE	0x2F8
 #define UART3_BASE	0x3E8
 #define UART4_BASE	0x2E8
 #define UART_16x50_REG_SHIFT	1
#endif

// Registers offsets
#define	UART_TxBUF		(0x0*UART_16x50_REG_SHIFT)
#define	UART_RxBUF		(0x0*UART_16x50_REG_SHIFT)
#define	UART_DIVLSB		(0x0*UART_16x50_REG_SHIFT)
#define	UART_IER		(0x1*UART_16x50_REG_SHIFT)
#define	UART_DIVMSB		(0x1*UART_16x50_REG_SHIFT)
#define	UART_IIR		(0x2*UART_16x50_REG_SHIFT)
#define	UART_FIFOCTL	(0x2*UART_16x50_REG_SHIFT)
#define	UART_LINECTL	(0x3*UART_16x50_REG_SHIFT)
#define	UART_MODEMCTL	(0x4*UART_16x50_REG_SHIFT)
#define	UART_LINESTAT	(0x5*UART_16x50_REG_SHIFT)
#define	UART_MODEMSTAT	(0x6*UART_16x50_REG_SHIFT)
#define	UART_SCRATCH	(0x7*UART_16x50_REG_SHIFT)

// Divisor values are this baud rate divided by desired rate
#define	UART_BASE_BAUDRATE	115200

// IER fields
#define	UART_IER_RxDATA		0x1
#define	UART_IER_TxEMPTY	0x2
#define	UART_IER_RxLINE		0x4
#define	UART_IER_MODEMSTAT	0x8

// IIR fields
#define	UART_IIR_NOINT	0x1			// '0' means interrupt pending
#define	UART_IIR_INTCAUSE	0x6		// Check specific value of these bits
#define	UART_IIR_TIMEOUT	0x8		// 16550+
#define	UART_IIR_FIFO64B	0x20	// 16750
#define	UART_IIR_FIFOSTAT	0xC0	// 16550+; check specific value of these bits

// FIFO control fields (16550+)
#define	UART_FCR_ENABLEFIFO	0x1
#define	UART_FCR_RxCLEAR	0x2
#define	UART_FCR_TxCLEAR	0x4
#define	UART_FCR_DMAMODE	0x8
#define	UART_FCR_FIFO64B	0x20	// 16750
#define	UART_FCR_INTTRIGGER	0xC0	// Check specific value of these bits: 1, 4, 8, 14 bytes trigger

// Line control fields
#define	UART_LCR_DATABITS	0x3		// Check specific value of these bits: 5, 6, 7, 8 data bits
#define	UART_LCR_STOPBITS	0x4		// 0 <-> 1 bit, 1 <-> 2 (1.5) bits
#define	UART_LCR_PARITY		0x38	// Check specific value of these bits
#define	UART_LCR_BREAKEN	0x40
#define	UART_LCR_DLAB		0x80	// Select offset 0 access DLA or buffers

// Modem control fields
#define	UART_MCR_FORCEDTR	0x1
#define	UART_MCR_FORCERTS	0x2
#define	UART_MCR_AUXOUT1	0x4
#define	UART_MCR_AUXOUT2	0x8
#define	UART_MCR_LOOPBACK	0x10
#define	UART_MCR_AUTOFLOWEN	0x20	// 16750

// Line status fields
#define	UART_LSR_DATAREADY	0x1
#define	UART_LSR_OVERRUNERR	0x2
#define	UART_LSR_PARITYERR	0x4
#define	UART_LSR_FRAMINGERR	0x8
#define	UART_LSR_BREAK		0x10
#define	UART_LSR_TxEMPTY	0x20
#define	UART_LSR_DATAEMPTY	0x40
#define	UART_LSR_RxFIFOERR	0x80

// Modem status fields
#define	UART_MSR_DELTACTS	0x1
#define	UART_MSR_DELTADSR	0x2
#define	UART_MSR_TREDGERING	0x4
#define	UART_MSR_DELTADCD	0x8
#define	UART_MSR_CTS		0x10
#define	UART_MSR_DSR		0x20
#define	UART_MSR_RING		0x40
#define	UART_MSR_CD			0x80

//
// Configurable parameters
//
#define	CFG_UART_BAUDRATE	115200

#define	CFG_UART_INT	1
#if	CFG_UART_INT
#define	CFG_UART_IER_VAL	UART_IER_RxDATA
#else
#define	CFG_UART_IER_VAL	0
#endif

#define	CFG_UART_FIFO		1
#if CFG_UART_FIFO
#define	CFG_UART_FCR_VAL	(UART_FCR_ENABLEFIFO | UART_FCR_RxCLEAR | UART_FCR_TxCLEAR)			// Interrupt on every byte
#else
#define	CFG_UART_FCR_VAL	0
#endif

#define	CFG_UART_LCR_VAL	(0x3)		// 8 data bits, 1 stop bit, no parity, DLAB = 0

// (!) It's really weird, but PC controller won't generate an Rx INT without AUXOUT2 set in MCR. Why(???)
// This is probably the reason:
// Aux Output 2 maybe connected to external circuitry which controls the UART-CPU interrupt process.
//		(Craig Peacock, http://www.beyondlogic.org/serial/serial.htm#19)
#define	CFG_UART_MCR_VAL	(UART_MCR_FORCEDTR | UART_MCR_FORCERTS | UART_MCR_AUXOUT2)		// Turn on DTR, RTS, OUT2

// Modem control fields is not relevant for now - this configuration is for debugging null-modem

// Convenience definitions

// Add CR (\r) to each LF (\n) output
#define	UART_MODE_UNIX	1


