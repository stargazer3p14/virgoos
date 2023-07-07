/*
 *	pcihost.c
 *
 *	PC-compatible PCI controller driver
 */
#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "pcihost.h"

// PCI devices addresses and vendor_id/device_id
// in order to scan the bus only once (autoscan)
#if	PCI_AUTOSCAN
struct pci_dev	pci_devices[PCI_DEVICES];
int	num_devices = 0;
#endif

// On MIPS with GT64120 system controller we redefine "ind"/"outd" so that they refer to GT64120_BASE instead of normal IO_BASE.
// MIPS follows a path of mimicing PC addresses for "ease of programming"...
// We will follow platform-referring definitions rather than architecture-referring - there are many MIPS platforms with very different mappings
#if defined (malta)
#define	ind(port)	le32_to_cpu(*(volatile uint32_t*)(GT64120_BASE+port))
#define	outd(port, value)	*(volatile uint32_t*)(GT64120_BASE+port) = cpu_to_le32((uint32_t)value)
#endif

/*
 *	Read a DWORD from specified offset in configuration space of
 *	device addressed by bus:dev:fn
 *
 *	Returns: datum read
 */
uint32_t	pcihost_readcfg(int bus, int dev, int fn, int offs)
{
	uint32_t	datum;

	outd(PCI_CFG_ADDR, 0x80000000 | (uint32_t)(bus & 0xFF) << 16 | (uint32_t)(dev & 0x1F) << 11 |
		(uint32_t)(fn & 0x7) << 8 | (uint32_t)(offs & 0xFC));

	datum = ind(PCI_CFG_DATA);
	return	datum;
}


/*
 *	Write a DWORD from specified offset in configuration space of
 *	device addressed by bus:dev:fn
 */
void	pcihost_writecfg(int bus, int dev, int fn, uint32_t offs, uint32_t datum)
{
	outd(PCI_CFG_ADDR, 0x80000000 | (uint32_t)(bus & 0xFF) << 16 | (uint32_t)(dev & 0x1F) << 11 |
		(uint32_t)(fn & 0x7) << 8 | (uint32_t)(offs & 0xFC));

	outd(PCI_CFG_DATA, datum);
}


#ifdef PCI_AUTOSCAN
/*
 *	Try the specified device. If it is present, add it
 *
 *	Returns: 1 - device added, 0 - device not present
 */
static int	pcihost_add_device(int bus, int dev, int fn)
{
	uint32_t	datum;
	uint32_t	class;

	// Read vendor ID
	datum = pcihost_readcfg(bus, dev, fn, PCI_CFG_VENDORID_OFFS);
	if (0xFFFFFFFF == datum)
		return	0;

	// Found a device. Copy its configuration space for later reference
	pci_devices[num_devices].bus = bus;
	pci_devices[num_devices].dev_fn = PCI_DEVFN(dev, fn);
	pci_devices[num_devices].vendor_id = VENDORID(datum);
	pci_devices[num_devices].device_id = DEVICEID(datum);

/*
if (num_devices >= 22)
	printfxy(2, num_devices-22, "%d) bus=%x dev_fv=%x, vendor_id=%04X, device_id=%04X",
		num_devices+1, (unsigned)pci_devices[num_devices].bus, (unsigned)pci_devices[num_devices].dev_fn,
		(unsigned)pci_devices[num_devices].vendor_id, (unsigned)pci_devices[num_devices].device_id);
*/

	class = pcihost_readcfg(bus, dev, fn, PCI_CFG_REVID_OFFS);

	serial_printf("%s(): %d) bus=%02X dev_fv=%02X (%02X:%02X) , vendor_id=%04X, device_id=%04X revision_id=%02X reg_prog_int=%02X sub_class_code=%02X class_code=%02X\r\n",
		__func__, num_devices+1, (unsigned)pci_devices[num_devices].bus, (unsigned)pci_devices[num_devices].dev_fn,
		(unsigned)PCI_DEV(pci_devices[num_devices].dev_fn), (unsigned)PCI_FN(pci_devices[num_devices].dev_fn),
		(unsigned)pci_devices[num_devices].vendor_id, (unsigned)pci_devices[num_devices].device_id,
		class & 0xFF, class >> 8 & 0xFF, class >> 16 & 0xFF, class >> 24 & 0xFF);

/*
	pci_devices[num_devices].cfg.vendor_id = datum & 0xFFFF;
	pci_devices[num_devices].cfg.device_id = datum >> 16 & 0xFFFF;

	datum = pcihost_readcfg(bus, dev, fn, PCI_CFG_COMMAND_OFFS);
	pci_devices[num_devices].cfg.command = datum & 0xFFFF;
	pci_devices[num_devices].cfg.status = datum >> 16 & 0xFFFF;

	datum = pcihost_readcfg(bus, dev, fn, PCI_CFG_REVID_OFFS);
	pci_devices[num_devices].cfg.rev_id = datum & 0xFF;
	pci_devices[num_devices].cfg.class_code = datum >> 8 & 0xFF;
	pci_devices[num_devices].cfg.sub_class_code = datum >> 16 & 0xFF;
	pci_devices[num_devices].cfg.reg_prog_int = datum >> 24 & 0xFF;

	datum = pcihost_readcfg(bus, dev, fn, PCI_CFG_CACHELINE_OFFS);
	pci_devices[num_devices].cfg.cache_line_size = datum & 0xFF;
	pci_devices[num_devices].cfg.lat_timer = datum & 0xFF;
	pci_devices[num_devices].cfg.hdr_type = datum & 0xFF;
	pci_devices[num_devices].cfg.bist = datum & 0xFF;

	pci_devices[num_devices].cfg.bar[0] = pcihost_readcfg(bus, dev, fn, PCI_CFG_BAR0_OFFS);
	pci_devices[num_devices].cfg.bar[1] = pcihost_readcfg(bus, dev, fn, PCI_CFG_BAR1_OFFS);
	pci_devices[num_devices].cfg.bar[2] = pcihost_readcfg(bus, dev, fn, PCI_CFG_BAR2_OFFS);
	pci_devices[num_devices].cfg.bar[3] = pcihost_readcfg(bus, dev, fn, PCI_CFG_BAR3_OFFS);
	pci_devices[num_devices].cfg.bar[4] = pcihost_readcfg(bus, dev, fn, PCI_CFG_BAR4_OFFS);
	pci_devices[num_devices].cfg.bar[5] = pcihost_readcfg(bus, dev, fn, PCI_CFG_BAR5_OFFS);
	pci_devices[num_devices].cfg.cis_ptr = pcihost_readcfg(bus, dev, fn, PCI_CFG_CARDBUSCIS_OFFS);

	datum = pcihost_readcfg(bus, dev, fn, PCI_CFG_SUBVENDOR_OFFS);
	pci_devices[num_devices].cfg.subsys_vendor_id = datum & 0xFFFF;
	pci_devices[num_devices].cfg.subsys_dev_id = datum >> 16 & 0xFFFF;

	pci_devices[num_devices].cfg.exp_rom_base = pcihost_readcfg(bus, dev, fn, PCI_CFG_EXPROMBASE_OFFS);
	
	datum = pcihost_readcfg(bus, dev, fn, PCI_CFG_CAPSPTR_OFFS);
	pci_devices[num_devices].cfg.pcaps = datum & 0xFF;

	datum = pcihost_readcfg(bus, dev, fn, PCI_CFG_INTLINE_OFFS);
	pci_devices[num_devices].cfg.int_line = datum & 0xFF;
	pci_devices[num_devices].cfg.int_pin = datum >> 8 & 0xFF;
	pci_devices[num_devices].cfg.min_gnt = datum >> 16 & 0xFF;
	pci_devices[num_devices].cfg.max_lat = datum >> 24 & 0xFF;
*/
	++num_devices;
}


static struct pci_dev	*pcihost_find_device(int vendor_id, int device_id, int *index)
{
	struct pci_dev	*p = pci_devices;
	int	i;

	for (i = 0; i < num_devices; ++i)
		if (p[i].device_id == device_id && p[i].vendor_id == vendor_id)
		{
			*index = i;
			return	p+i;
		}

	return	NULL;
}
#endif	// PCI_AUTOSCAN


/*
 *	Init PCI host controller.
 *	Scan PCI bus for connected devices
 */
int	pcihost_init(unsigned drv_id)
{
#if PCI_AUTOSCAN
	int	bus, dev, fn;
#endif

#ifdef PCI_AUTOSCAN
	for (bus = 0; bus < 256; ++bus)
		for (dev = 0; dev < 32; ++dev)
			for (fn = 0; fn < 8; ++fn)
			{
				pcihost_add_device(bus, dev, fn);
				if (PCI_DEVICES == num_devices)
					goto scan_done;
			}
scan_done:
#endif
	return	0;
}


int	pcihost_deinit(void)
{
	return	0;
}


int	pcihost_open(unsigned subdev_id)
{
	return	0;
}


int pcihost_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int pcihost_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int pcihost_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	int	vendor_id, device_id, *pdev_index;
	int	dev_index;
	uint32_t	offset;
	uint32_t	value, *pvalue;
	uint32_t	temp;

	switch(cmd)
	{
	default:
		break;
	case PCI_IOCTL_FIND_DEV:
		/*
		 *	find_device: (..., arg, int vendor_id, int device_id, int	*pdev_index)
		 *	return index of device in devices table for further reference
		 */
		vendor_id = va_arg(argp, int);
		device_id = va_arg(argp, int);
		pdev_index = (int*)va_arg(argp, int*);
		if (pcihost_find_device(vendor_id, device_id, pdev_index) == NULL)
			*pdev_index = -1;
		break;

	case PCI_IOCTL_CFG_WRITEB:
		/*
		 *	write_cfg_byte(..., arg, int dev_index, uint32_t offset, uint32_t value)
		 *	Use only bits 0..7 of value
		 */
		dev_index = va_arg(argp, int);
		offset = va_arg(argp, uint32_t);
		value = va_arg(argp, uint32_t);
		value = (value & 0xFF) << 8 * (offset & 0x3);
		temp = 	pcihost_readcfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset & ~0x3);
		temp &= ~(0xFF << 8 * (offset & 0x3));
		temp |= value;
		pcihost_writecfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset & ~0x3, temp);
		break;

	case PCI_IOCTL_CFG_WRITEW:
		/*
		 *	write_cfg_word(..., arg, int dev_index, uint32_t offset, uint32_t value)
		 *	Use only bits 0..15 of value
		 *
		 *	(!) Writes that cross DWORD boundary are discarded.
		 *	There are no PCI config registers that cross DWORD addresses
		 */
		dev_index = va_arg(argp, int);
		offset = va_arg(argp, uint32_t);
		if ((offset & 0x3) > 2)
			break;
		value = va_arg(argp, uint32_t);
		value = (value & 0xFFFF) << 8 * (offset & 0x3);
		temp = 	pcihost_readcfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset & ~0x3);
		temp &= ~(0xFFFF << 8 * (offset & 0x3));
		temp |= value;
		pcihost_writecfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset & ~0x3, temp);

		break;

	case PCI_IOCTL_CFG_WRITED:
		/*
		 *	write_cfg_uint32_t(..., arg, int dev_index, uint32_t offset, uint32_t value)
		 *
		 *	(!) Writes that cross DWORD boundary are discarded.
		 *	There are no PCI config registers that cross DWORD addresses
		 */
		dev_index = va_arg(argp, int);
		offset = va_arg(argp, uint32_t);
		if ((offset & 0x3) != 0)
			break;
		value = va_arg(argp, uint32_t);
		pcihost_writecfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset, value);

		break;

	case PCI_IOCTL_CFG_READB:
		/*
		 *	read_cfg_byte(..., arg, int dev_index, uint32_t offset, uint32_t *pvalue)
		 *	Return only bits 0..7 of value
		 */
		dev_index = va_arg(argp, int);
		offset = va_arg(argp, uint32_t);
		pvalue = va_arg(argp, uint32_t*);
		value = pcihost_readcfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset & ~0x3);
		value = value >> 8 * (offset & 0x3) & 0xFF;
		*pvalue = value;
		break;

	case PCI_IOCTL_CFG_READW:
		/*
		 *	read_cfg_word(..., arg, int dev_index, uint32_t offset, uint32_t *pvalue)
		 *	Return only bits 0..15 of value
		 *
		 *	(!) Reads that cross DWORD boundary are discarded.
		 *	There are no PCI config registers that cross DWORD addresses
		 */
		dev_index = va_arg(argp, int);
		offset = va_arg(argp, uint32_t);
		if ((offset & 0x3) > 2)
			break;
		pvalue = va_arg(argp, uint32_t*);
		value = pcihost_readcfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset & ~0x3);
		value = value >> 8 * (offset & 0x3) & 0xFFFF;
		*pvalue = value;
		break;

	case PCI_IOCTL_CFG_READD:
		/*
		 *	read_cfg_uint32_t(..., arg, int dev_index, uint32_t offset, uint32_t *pvalue)
		 *
		 *	(!) Reads that cross DWORD boundary are discarded.
		 *	There are no PCI config registers that cross DWORD addresses
		 */
		dev_index = va_arg(argp, int);
		offset = va_arg(argp, uint32_t);
		if ((offset & 0x3) != 0)
			break;
		pvalue = va_arg(argp, uint32_t*);
		value = pcihost_readcfg(pci_devices[dev_index].bus, PCI_DEV(pci_devices[dev_index].dev_fn),
			PCI_FN(pci_devices[dev_index].dev_fn), offset);
		*pvalue = value;
		break;
	}

	return	0;
}


int pcihost_close(unsigned sub_id)
{
	return	0;
}


struct drv_entry	pcihost = {pcihost_init, pcihost_deinit, pcihost_open, pcihost_read,
	pcihost_write, pcihost_ioctl, pcihost_close};

