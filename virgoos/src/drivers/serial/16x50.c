/*
 *	16x50.c
 *
 *	8250/16450/16550/16650/16750 UART driver.
 */

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "errno.h"
#include "queues.h"
#include "16x50.h"
#include "taskman.h"

// Prototypes
int	uart_init(unsigned drv_id);
int	uart_deinit(void);
int	uart_open(unsigned subdev_id);
int uart_read(unsigned subdev_id, void *buffer, unsigned long length);
int uart_write(unsigned subdev_id, const void *buffer, unsigned long length);
int uart_ioctl(unsigned subdev_id, int cmd, va_list argp);
int uart_close(unsigned sub_id);

// Base UART port addresses
dword	uart_base[4] = {UART1_BASE, UART2_BASE, UART3_BASE, UART4_BASE};

struct task_q	*rx_wait[4];
struct task_q	*tx_wait[4];

char	serial_hello[] = "Hello, world! SeptOS version 0.1. Copyright (c) Daniel Drubin, 2007-2010\n";

#ifdef evmdm6467

// Timer handler for platforms for which we don't have luck with interrupt handlers for receive
static	void	timer_handler(void *unused)
{
	byte	lsr;

	lsr = ind(UART1_BASE + UART_LINESTAT) & 0xFF;

	if (lsr & UART_LSR_DATAREADY)
	{
		if (rx_wait[0] != NULL)
			wake(&rx_wait[0]);
	}
}

timer_t	uart_timer = {1 /* ticks of timer's resolution */, 0, TICKS_PER_SEC, TF_PERIODIC, 0, timer_handler, NULL};
#endif

/*
 *	UART IRQ handler.
 */
static	int	uart_isr(void)
{
	byte	int_cause;
	byte	lsr;
	// IRQ sharing (ports 0-2, 1-3 later

#ifndef	evmdm6467
	int_cause = inb(UART1_BASE + UART_IIR);
#else
	int_cause = ind(UART1_BASE + UART_IIR) & 0xFF;
#endif

	lsr = ind(UART1_BASE + UART_LINESTAT) & 0xFF;
	// Meanwhile handle only data receive interrupt
//	if ((int_cause & UART_IIR_INTCAUSE) == 0x4)
	if (lsr & UART_LSR_DATAREADY)
	{
serial_printf("%s(): lsr=%02X, data ready\n", __func__, lsr);
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

	word	div;

	// Initialize UART

// On EVMDM6467 meanwhile we trust uboot to initialize UART correctly
// TODO: initialize UART correctly ourselves
#ifndef	evmdm6467
	// TODO: provide different specific initializations for every UART
	// For now, init only UART 0.
	
	// Turn off UART's interrupts
#ifndef evmdm6467
	outb(UART1_BASE + UART_IER, 0);
#else
	outd(UART1_BASE + UART_IER, 0);
#endif
	
#ifndef evmdm6467
	// Divisor
	div = (word)(UART_BASE_BAUDRATE / CFG_UART_BAUDRATE);
	outb(UART1_BASE + UART_LINECTL, UART_LCR_DLAB); 
	outb(UART1_BASE + UART_DIVLSB, div & 0xFF); 
	outb(UART1_BASE + UART_DIVMSB, (div >> 8) & 0xFF);

	outb(UART1_BASE + UART_LINECTL, CFG_UART_LCR_VAL);
	outb(UART1_BASE + UART_FIFOCTL, CFG_UART_FCR_VAL);
	outb(UART1_BASE + UART_MODEMCTL, CFG_UART_MCR_VAL);
	outb(UART1_BASE + UART_IER, CFG_UART_IER_VAL);
#else
	// Divisor
	div = (word)(UART_BASE_BAUDRATE / CFG_UART_BAUDRATE);
	outd(UART1_BASE + UART_LINECTL, UART_LCR_DLAB); 
	outd(UART1_BASE + UART_DIVLSB, div & 0xFF); 
	outd(UART1_BASE + UART_DIVMSB, (div >> 8) & 0xFF);

	outd(UART1_BASE + UART_LINECTL, CFG_UART_LCR_VAL);
	outd(UART1_BASE + UART_FIFOCTL, CFG_UART_FCR_VAL);
	outd(UART1_BASE + UART_MODEMCTL, CFG_UART_MCR_VAL);
	outd(UART1_BASE + UART_IER, CFG_UART_IER_VAL);
#endif
#endif
	
#if CFG_UART_INT
#ifndef evmdm6467
	// Set UART interrupt
	set_int_callback(UART1_IRQ, uart_isr);
#else
//serial_printf("%s(): installing UART poll timer\n", __func__);
	// Set UART poll timer
	install_timer(&uart_timer);
#endif
#endif

	for (i = 0; i < sizeof(serial_hello) - 1; ++i)
		uart_write(0, serial_hello + i, 1);

	serial_printf("<<<%s>>>\n", serial_hello);
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
	if (subdev_id >= 4)
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
	if ( subdev_id >= 4 )
		return	-ENODEV;

	// Meanwhile read only 1 byte
#ifndef evmdm6467
	if ((inb(uart_base[subdev_id] + UART_LINESTAT) & UART_LSR_DATAREADY) == 0)
#else
	if (((ind(uart_base[subdev_id] + UART_LINESTAT) & UART_LSR_DATAREADY) & 0xFF) == 0)
#endif
		nap(&rx_wait[subdev_id]);

//	while ((inb(uart_base[subdev_id] + UART_LINESTAT) & UART_LSR_DATAREADY) == 0)
//		;
	
#ifndef evmdm6467
	*(unsigned char*)buffer = inb(uart_base[subdev_id] + UART_RxBUF);
#else
	*(unsigned char*)buffer = ind(uart_base[subdev_id] + UART_RxBUF) & 0xFF;
#endif
	return	1;
}

/*
 *	Write to a uart 1 character.
 */
int uart_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	if (subdev_id >= 4)
		return	-ENODEV;

while (length)
{
	// Meanwhile send only 1 byte
#ifndef evmdm6467
	while ((inb(uart_base[subdev_id] + UART_LINESTAT) & UART_LSR_TxEMPTY) == 0)
#else
	while (((ind(uart_base[subdev_id] + UART_LINESTAT) & UART_LSR_TxEMPTY) & 0xFF) == 0)
#endif
		;

#if UART_MODE_UNIX
	if ('\n' ==  *(unsigned char*)buffer)
	{
		unsigned char	cr = '\r';
		uart_write(subdev_id, &cr, 1);
	}
#endif
#ifndef evmdm6467
	outb(uart_base[subdev_id] + UART_TxBUF, *(unsigned char*)buffer);
#else
	outd(uart_base[subdev_id] + UART_TxBUF, *(unsigned char*)buffer);
#endif
++buffer;
--length;
}
	return	1;
}

/*
 *	IOCTL - set configuration parameters.
 */
int uart_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	if ( subdev_id >= 4 )
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

