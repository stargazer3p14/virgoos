/*
 *	PCI host driver's definitions
 */

/*
 * PCI_AUTOSCAN defines whether to scan all devices at OS init automatically (1) or
 * configure devices manually (0 or undefined)
 */

#include "sosdef.h"
#include "config.h"

#define	PCI_AUTOSCAN	1

/* PCI configuration space */
struct pci_config
{
	word	vendor_id;
	word	device_id;
	word	command;
	word	status;
	byte	rev_id;
	byte	reg_prog_int;
	byte	sub_class_code;
	byte	class_code;
	byte	cache_line_size;
	byte	lat_timer;
	byte	hdr_type;
	byte	bist;
	dword	bar[6];
	dword	cis_ptr;
	word	subsys_vendor_id;
	word	subsys_dev_id;
	dword	exp_rom_base;
	byte	pcaps;
	byte	reserved[7];
	byte	int_line;
	byte	int_pin;
	byte	min_gnt;
	byte	max_lat;
};

struct pci_dev
{
	byte	bus;			// Bus no.
	byte	dev_fn;			// Device and function no.
	word	device_id;			// Device ID
	word	vendor_id;		// Vendor ID
//	struct pci_config	cfg;	// Configuration space (copy)
};

// Maximum (for autoscan) number of PCI devices
#define	PCI_DEVICES	64

// PC-compatible PCI configuration ports
#define	PCI_CFG_ADDR	0xCF8
#define	PCI_CFG_DATA	0xCFC

// PCI configuration registers offsets
#define	PCI_CFG_VENDORID_OFFS	0
#define	PCI_CFG_DEVICEID_OFFS	2
#define	PCI_CFG_COMMAND_OFFS	0x4

// PCI command register fields
#define	PCICMD_IOEN	0x1
#define	PCICMD_MEMEN	0x2
#define	PCMCMD_MASTER	0x4

#define	PCI_CFG_STATUS_OFFS		0x6
#define	PCI_CFG_REVID_OFFS		0x8
#define	PCI_CFG_CACHELINE_OFFS	0xC
#define	PCI_CFCG_LATTIMER_OFFS	0xD
#define	PCI_CFG_HDRTYPE_OFFS	0xE
#define	PCI_CFG_BIST_OFFS		0xF
#define	PCI_CFG_BAR0_OFFS		0x10
#define	PCI_CFG_BAR1_OFFS		0x14
#define	PCI_CFG_BAR2_OFFS		0x18
#define	PCI_CFG_BAR3_OFFS		0x1C
#define	PCI_CFG_BAR4_OFFS		0x20
#define	PCI_CFG_BAR5_OFFS		0x24
#define	PCI_CFG_CARDBUSCIS_OFFS	0x28
#define	PCI_CFG_SUBVENDOR_OFFS	0x2C
#define	PCI_CFG_SUBSYS_OFFS		0x2E
#define	PCI_CFG_EXPROMBASE_OFFS	0x30
#define	PCI_CFG_CAPSPTR_OFFS	0x34
#define	PCI_CFG_INTLINE_OFFS	0x3C
#define	PCI_CFG_INTPIN_OFFS		0x3D
#define	PCI_CFG_MINGRANT_OFFS	0x3E
#define	PCI_CFG_MAXLAT_OFFS		0x3F

/* IOCTLs */
#define	PCI_IOCTL_FIND_DEV		0x1
#define	PCI_IOCTL_CFG_WRITEB	0x2
#define	PCI_IOCTL_CFG_WRITEW	0x3
#define	PCI_IOCTL_CFG_WRITED	0x4
#define	PCI_IOCTL_CFG_READB		0x5
#define	PCI_IOCTL_CFG_READW		0x6
#define	PCI_IOCTL_CFG_READD		0x7

#define	VENDORID(a) (a & 0xFFFF)
#define	DEVICEID(a) (a >> 16 & 0xFFFF)
#define	PCI_DEV(devfn) (devfn >> 3 & 0xFF)
#define	PCI_FN(devfn) (devfn & 0x7)
#define	PCI_DEVFN(dev, fn) ((dev & 0xFF) << 3 | (fn & 0x7))
