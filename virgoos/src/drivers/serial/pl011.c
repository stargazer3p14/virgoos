/*
 *	pl011.c
 *
 *	Driver for PrimeCell PL011 UART found on ARM Versatile boards
 */

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "errno.h"
#include "queues.h"
#include "pl011.h"
#include "taskman.h"

// Prototypes
int	uart_init(unsigned drv_id);
int	uart_deinit(void);
int	uart_open(unsigned subdev_id);
int	uart_read(unsigned subdev_id, void *buffer, unsigned long length);
int	uart_write(unsigned subdev_id, const void *buffer, unsigned long length);
int	uart_ioctl(unsigned subdev_id, int cmd, va_list argp);
int	uart_close(unsigned sub_id);

// Base UART port addresses
dword	uart_base[3] = {UART0_BASE, UART1_BASE, UART2_BASE};

struct task_q	*rx_wait[4];
struct task_q	*tx_wait[4];

char	serial_hello[] = "Hello, world! SeptOS version 0.1. Copyright (c) Daniel Drubin, 2007-2010\r\n";

/*
 *	UART IRQ handler.
 */
static	int	uart_isr(void)
{
	dword	int_cause;

	int_cause = ind(UART0_BASE + UARTMIS);

	// Clear interrupts
	outd(UART0_BASE + UARTICR, int_cause);

serial_printf("%s(), int_cause=%08X\n", __func__, int_cause);
	// Meanwhile handle only data receive interrupt
	if (int_cause & UART_RINT)
	{
		if (rx_wait[0] != NULL)
			wake(&rx_wait[0]);
	}

	return	1;
}


/*
 *	Init uart driver. drv_id - port number
 */
int	uart_init(unsigned drv_id)
{
	int	i;

	// Init - later. Meanwhile trust emulator (QEMU) or boot monitor to initialize UART
#if 0
	word	div;

	// Initialize UART

	// TODO: provide different specific initializations for every UART
	// For now, init only UART 0.
	
	// Turn off UART's interrupts
	outb(UART1_BASE + UART_IER, 0);
	
	// Divisor
	div = (word)(UART_BASE_BAUDRATE / CFG_UART_BAUDRATE);
	outb(UART1_BASE + UART_LINECTL, UART_LCR_DLAB); 
	outb(UART1_BASE + UART_DIVLSB, div & 0xFF); 
	outb(UART1_BASE + UART_DIVMSB, (div >> 8) & 0xFF);

	outb(UART1_BASE + UART_LINECTL, CFG_UART_LCR_VAL);
	outb(UART1_BASE + UART_FIFOCTL, CFG_UART_FCR_VAL);
	outb(UART1_BASE + UART_MODEMCTL, CFG_UART_MCR_VAL);
	outb(UART1_BASE + UART_IER, CFG_UART_IER_VAL);
	
	for (i = 0; i < sizeof(serial_hello); ++i)
		uart_write(0, serial_hello + i, 1);

#endif
	serial_printf("<<<%s>>>", serial_hello);

#if CFG_UART_INT
	outd(UART0_BASE + UARTIMSC, CFG_UART_IMSC_VAL);
	outd(UART1_BASE + UARTIMSC, CFG_UART_IMSC_VAL);
	// Set UART interrupt
	set_int_callback(UART0_IRQ, uart_isr);
#endif

	return	0;
}

/*
 *	Deinit uart.
 */
int	uart_deinit(void)
{
	return	0;
}

/*
 *	Open a uart.
 */
int	uart_open(unsigned subdev_id)
{
	if (subdev_id >= 3)
		return	-ENODEV;

	return	0;
}

/*
 *	Extract an entry from a uart buffer.
 *	Return: sizeof( key_t ) if OK
 *			0 if buffer is empty
 *			error is something is wrong.
 */
int uart_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	if (subdev_id >= 3)
		return	-ENODEV;

	// Meanwhile read only 1 byte
	if (ind(uart_base[subdev_id] + UARTFR) & UARTFR_RXFE)
		nap(&rx_wait[subdev_id]);
//		return	0;

	*(unsigned char*)buffer = inb(uart_base[subdev_id] + UARTDR);
	return	1;
}

/*
 *	Write to a uart 1 character.
 */
int uart_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	if (subdev_id >= 3)
		return	-ENODEV;

	// Meanwhile send only 1 byte
	while (ind(uart_base[subdev_id] + UARTFR) & UARTFR_BUSY)
		;

#if UART_MODE_UNIX
	if ('\n' ==  *(unsigned char*)buffer)
	{
		unsigned char	cr = '\r';
		uart_write(subdev_id, &cr, 1);
	}
#endif
	outb(uart_base[subdev_id] + UARTDR, *(unsigned char*)buffer);
	return	1;
}

/*
 *	IOCTL - set configuration parameters.
 */
int uart_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	if ( subdev_id >= 3 )
		return	-ENODEV;

	return	0;
}

/*
 *	Close a uart.
 */
int uart_close(unsigned sub_id)
{
	return	0;
}


drv_entry	uart = {uart_init, uart_deinit, uart_open, uart_read, 
	uart_write, uart_ioctl, uart_close};

