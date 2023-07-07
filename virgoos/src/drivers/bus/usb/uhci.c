/***************************************************
 *
 *	UHCI USB host driver
 *
 ***************************************************/
 
#include	"sosdef.h"
#include	"drvint.h"
#include	"config.h"
#include	"pcihost.h"
#include	"errno.h"
#include	"uhci.h"
#include	"usb.h"

#define	DEBUG_IRQ	1
#define	DEBUG_INIT	1
#define	DEBUG_PCI_CONFIG	1

// Table to indetify EHCI controllers from different vendors
struct uhci_creds
{
	unsigned	vendor_id;
	unsigned	device_id;
} uhci_dev_tbl[] =
{
	{0x8086, 0x7112}		// Intel UHCI controller
};

static int	pci_dev_index = -1;
unsigned	uhci_io_base = 0xFFFFFFFF;
unsigned	num_ports;

static	void	uhci_timer_handler(void *unused);
static	int	uhci_isr(void);

timer_t	uhci_timer = {2000, 0, TICKS_PER_SEC, TF_PERIODIC, 0, uhci_timer_handler, NULL};

static	void	uhci_timer_handler(void *unused)
{
	dword	int_sts, port_sc;
	static unsigned	timer_count;
	
//#ifdef	DEBUG_IRQ
//	printfxy(0, 4, "%s(): Received UHCI timer no. %u", __func__, ++timer_count);
//#endif

	int_sts = inw(uhci_io_base + UHCI_REG_USBSTS);
	if ((int_sts & UHCI_INTR_ENABLE) != 0)
	{
		serial_printf("%s(): UHCI HC interrupt - not received via PIC\r\n", __func__);
		uhci_isr();
	}

	port_sc = inw(uhci_io_base + UHCI_REG_PORTSC1);
	if ((port_sc & UHCI_PORTSC_CONN_CHANGE) != 0)
	{
		serial_printf("%s(): PORT1 changed connected state (%02X) -- device is %s\r\n", __func__, port_sc, (port_sc & UHCI_PORTSC_CONNECTED) != 0 ? "connected" : "disconnected");
		outw(uhci_io_base + UHCI_REG_PORTSC1, port_sc);			// Clear all "changed" indicators
	}
	port_sc = inw(uhci_io_base + UHCI_REG_PORTSC2);
	if ((port_sc & UHCI_PORTSC_CONN_CHANGE) != 0)
	{
		serial_printf("%s(): PORT2 changed connected state (%02X) -- device is %s\r\n", __func__, port_sc, (port_sc & UHCI_PORTSC_CONNECTED) != 0 ? "connected" : "disconnected");
		outw(uhci_io_base + UHCI_REG_PORTSC2, port_sc);			// Clear all "changed" indicators
	}
}

/*
 *	EHCI IRQ handler.
 */
static	int	uhci_isr(void)
{
	int	i;
	static unsigned	irq_count;
	dword	int_sts;

#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received UHCI HC IRQ no. %u\r\n", __func__, ++irq_count);
#endif

	return	0;		/* Allow shared interrupts */
}

/*
 *	Initialize the EHCI host controller
 */
int	uhci_init(unsigned drv_id)
{
	dword	value = -1;
	word	eeprom_val;
	int	i;
	int	int_line;
	int	vendor_id, dev_id;

	/* Open PCI host driver - no need */
	
	/* Find UHCI device */
	for (i = 0; i < sizeof(uhci_dev_tbl) / sizeof(uhci_dev_tbl[0]); ++i)
	{
		ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_FIND_DEV, uhci_dev_tbl[i].vendor_id, uhci_dev_tbl[i].device_id, &pci_dev_index);
		if (pci_dev_index != -1)
			break;
	}
	if (pci_dev_index != -1)
		serial_printf("Found UHCI HC device at index %d\r\n", pci_dev_index);
	else
	{
		serial_printf("UHCI HC device was not found\r\n");
		return	ENODEV;
	}

	/* Device found */

	/* Print its current configuration (by BIOS) */
#ifdef	DEBUG_PCI_CONFIG
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_VENDORID_OFFS, &value);
	serial_printf("VendorID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_DEVICEID_OFFS, &value);
	serial_printf("DeviceID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_COMMAND_OFFS, &value);
	serial_printf("Command = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_STATUS_OFFS, &value);
	serial_printf("Status = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_REVID_OFFS, &value);
	serial_printf("Revision ID = %02X, class code= %08X ", value & 0xFF, value & ~0xFF);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_CACHELINE_OFFS, &value);
	serial_printf("Cache line size = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFCG_LATTIMER_OFFS, &value);
	serial_printf("Latency timer = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_HDRTYPE_OFFS, &value);
	serial_printf("Header type = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_BIST_OFFS, &value);
	serial_printf("BIST = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR0_OFFS, &value);
	serial_printf("BAR0 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR1_OFFS, &value);
	serial_printf("BAR1 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR2_OFFS, &value);
	serial_printf("BAR2 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR3_OFFS, &value);
	serial_printf("BAR3 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR4_OFFS, &value);
	serial_printf("BAR4 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR5_OFFS, &value);
	serial_printf("BAR5 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_SUBVENDOR_OFFS, &value);
	serial_printf("Subsystem VendorID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_SUBSYS_OFFS, &value);
	serial_printf("Subsystem DeviceID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_EXPROMBASE_OFFS, &value);
	serial_printf("Expansion ROM base = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_CAPSPTR_OFFS, &value);
	serial_printf("PCaps = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTLINE_OFFS, &value);
	serial_printf("Int line = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTPIN_OFFS, &value);
	serial_printf("Int pin = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_MINGRANT_OFFS, &value);
	serial_printf("Min. grant = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_MAXLAT_OFFS, &value);
	serial_printf("Max. latency = %02X ", value);
	serial_printf("\r\n");
#endif

	/* Get I/O base address -- UHCI keeps USBBASE at BAR4 */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR4_OFFS, &value);
	uhci_io_base = (value & ~0xF);
	serial_printf("The UHCI HC uses I/O base address at 0x%04X\r\n", uhci_io_base);
	
	/* Setup IRQ */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTLINE_OFFS, &int_line);
	serial_printf("The UHCI HC uses IRQ line %d\r\n", int_line);
	set_int_callback(int_line, uhci_isr);

#if DEBUG_INIT
	serial_printf("%s(): UHCI I/O regs initial state: ", __func__);
	value = inw(uhci_io_base + UHCI_REG_USBCMD);
	serial_printf("USBCMD=%04X ", value);
	value = inw(uhci_io_base + UHCI_REG_USBSTS);
	serial_printf("USBSTS=%04X ", value);
	value = inw(uhci_io_base + UHCI_REG_USBINTR);
	serial_printf("USBINTR=%04X ", value);
	value = inw(uhci_io_base + UHCI_REG_FRNUM);
	serial_printf("FRNUM=%04X ", value);
	value = ind(uhci_io_base + UHCI_REG_FLBASEADD);
	serial_printf("FLBASEADD=%04X ", value);
	value = inb(uhci_io_base + UHCI_REG_SOF_MODIFY);
	serial_printf("SOFMODIFY=%02X ", value);
	value = inw(uhci_io_base + UHCI_REG_PORTSC1);
	serial_printf("PORTSC1=%04X ", value);
	value = inw(uhci_io_base + UHCI_REG_PORTSC2);
	serial_printf("PORTSC2=%04X ", value);
	serial_printf("\r\n");
#endif

	// Linux driver's idea - detect up to 7 ports
	for (num_ports = 0; num_ports < 7; ++num_ports)
	{
		value = inw(uhci_io_base + UHCI_REG_PORTSC1 + i * 2);
		if (!((value & 0x80) != 0 && value != 0xFFFF))
			break;
	}
	if (7 == num_ports)
	{
		serial_printf("%s(): detected 7 ports and counting... defaulting to 2 ports\r\n", __func__);
		num_ports = 2;
	}
	serial_printf("%s(): detected %d USB ports\r\n", __func__, num_ports);

	// Store the frame list base address
	// ...
	// Set the current frame number
	// ...

	// SOF default - 1 ms
	outb(uhci_io_base + UHCI_REG_SOF_MODIFY, 0x40);

	// Enable PIRQ (due to Linux driver)
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, 0xC0, 0x2000);

	// Mark it configured and running with a 64-byte max packet.
	outw(uhci_io_base + UHCI_REG_USBCMD, UHCI_CMD_START_VAL);

	// Enable interrupts
	outw(uhci_io_base + UHCI_REG_USBINTR, UHCI_INTR_ENABLE);
	
	// Install timer "interrupt" pollong callback - interrupts seem not to work
	install_timer(&uhci_timer);

	return	0;
}

int	uhci_deinit(void)
{
	return	0;
}


int	uhci_open(unsigned subdev_id)
{
	return	0;
}


int uhci_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int uhci_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int uhci_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	return	0;
}


int uhci_close(unsigned sub_id)
{
	return	0;
}

struct drv_entry	uhci = {uhci_init, uhci_deinit, uhci_open, uhci_read,
	uhci_write, uhci_ioctl, uhci_close};

 
