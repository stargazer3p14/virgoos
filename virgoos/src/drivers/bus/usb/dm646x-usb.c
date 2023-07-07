/*
 *	dm646x-usb.c
 *
 *	Device driver for DM646x dual-role USB controller
 *
 *	(!) NOTE: DMA for USB controller is essentially the same as for EMAC; it even uses very similar (if not the same) buffer descriptor format.
 *	Therefore we may use the same BD management code as with EMAC (only suit it to 5 EPs etc.)
 */

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "errno.h"
#include "dm646x-usb.h"
#include "usb.h"

int	usb_mode = USB_MODE_HOST;
int	high_speed = 1;
int	rndis_mode = 0;

static	int	dm646x_usb_isr(void)
{
serial_printf("%s()\n");
	// This interrupt is not shared
	return	1;
}

static	int	dm646x_usb_dma_isr(void)
{
serial_printf("%s()\n");
	// This interrupt is not shared
	return	1;
}

int	dm646x_usb_init(unsigned drv_id)
{
	int	i;

	// Set ISRs
	set_int_callback(USB_IRQ, dm646x_usb_isr);
	set_int_callback(USBDMA_IRQ, dm646x_usb_dma_isr);

	// Set USB PHY to operate as host or peripheral
	outd(SYS_MODULE_BASE+SYSUSBCTL, ind(SYS_MODULE_BASE+SYSUSBCTL) & ~0x00010000 | (usb_mode & 0x1) << 16);

	// Enable all USB interrupts at controller level
	outb(DM646x_USB_BASE + INTRUSBE, 0xFF);
	outd(DM646x_USB_BASE + INTMSKSETR, 0xFFFFFFFF);

	// Enable interrupts at core level
	outw(DM646x_USB_BASE + INTRTXE, 0x1F);	// All core Tx EP interrupts + EP0 Rx/Tx interrupt
	outw(DM646x_USB_BASE + INTRRXE, 0x1E);	// All core Rx EP interrupts

	// Enable high-speed (does this apply to host or to device only?)
	//outb(DM646x_USB_BASE + POWER, inb(DM646x_USB_BASE + POWER) & ~0x20 | high_speed << 5);

	if (usb_mode == USB_MODE_HOST)
		outb(DM646x_USB_BASE + POWER, inb(DM646x_USB_BASE + POWER) & ~0x2 | 0x2);

	// Clear all pending interrupts
	outd(DM646x_USB_BASE + INTCLRR, ind(DM646x_USB_BASE + INTSRCR));

	// Initialize FUNCADDR to 0. Meaningful only when operating in host mode
	for (i = 0; i < 4; ++i)
	{
		outb(DM646x_USB_BASE + TARGET_EP_CTL_BASE + i * TARGET_EP_CTL_SIZE + TXFUNCADDR, 0);
		outb(DM646x_USB_BASE + TARGET_EP_CTL_BASE + i * TARGET_EP_CTL_SIZE + RXFUNCADDR, 0);
	}

	// Initialize CPPI DMA
	outd(DM646x_USB_BASE + RCCPICR, 0);
	outd(DM646x_USB_BASE + TCPPICR, 0);

	for (i = 0; i < 4; ++i)
	{
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + TCPPIDMASTATEW0, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + TCPPIDMASTATEW1, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + TCPPIDMASTATEW2, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + TCPPIDMASTATEW3, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + TCPPIDMASTATEW4, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + TCPPIDMASTATEW5, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + RCPPIDMASTATEW0, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + RCPPIDMASTATEW1, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + RCPPIDMASTATEW2, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + RCPPIDMASTATEW3, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + RCPPIDMASTATEW4, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + RCPPIDMASTATEW5, 0);
		outd(DM646x_USB_BASE + i * CPPI_CHANNEL_STATE_SIZE + RCPPIDMASTATEW6, 0);
	}

	// RNDIS mode selection
	outd(DM646x_USB_BASE + CTRLR, ind(DM646x_USB_BASE + CTRLR) & ~0x10 | rndis_mode << 4);

	// Start a session
	outb(DM646x_USB_BASE + DEVCTL, 1);

	serial_printf("%s(): completed (CTRLR=%08X INTMSKR=%08X INTRUSBE=%08X POWER=%08X)\n", __func__, (unsigned)ind(DM646x_USB_BASE + CTRLR), (unsigned)ind(DM646x_USB_BASE + INTMSKR), (unsigned)inb(DM646x_USB_BASE + INTRUSBE), (unsigned)inb(DM646x_USB_BASE + POWER));
	return	0;
}

int	dm646x_usb_deinit(void)
{
	return	0;
}


int	dm646x_usb_open(unsigned subdev_id)
{
	return	0;
}


int dm646x_usb_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int dm646x_usb_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int dm646x_usb_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	return	0;
}


int dm646x_usb_close(unsigned sub_id)
{
	return	0;
}

struct drv_entry	dm646x_usb = {dm646x_usb_init, dm646x_usb_deinit, dm646x_usb_open, dm646x_usb_read,
	dm646x_usb_write, dm646x_usb_ioctl, dm646x_usb_close};

