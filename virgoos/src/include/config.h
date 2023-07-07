/*
 *	config.h
 *
 *	This is the main SeptemberOS configuration file. When an application is built, the developer
 *	should edit this file to include the desired configuration.
 */

#ifndef	CONFIG__H
 #define	CONFIG__H

#include "sosdef.h"
#include "_config.h"
#include "config-arch.h"

/*
 *	System timer configuration.
 */
// Timer ticks per second
#define	TICKS_PER_SEC	1000
// Timer ticks per task slice (if same-priority round-robin taskman option is enabled)
#define	TICKS_PER_SLICE	10
// Maximum number of timers (currently not used - timers are allocated dynamically)
#define	MAX_TIMERS		1024

// Define to non-0 in order to run udelay() calibration. Then define CALIBRATED_UDELAY_COUNT1 and CALIBRATED_UDELAY_COUNT2 for your board according to printout
// (!) Don't define for production system
//#define	CALIBRATE_UDELAY	1

#define	DEF_STRATEGY	STRATEGY_FIRST_FIT

/*
 *	Task manager configuration
 */
// Maximum number of tasks
#define	MAX_TASKS	1024
// Number of priority levels (decrease to lower tasks table size)
#define	NUM_PRIORITY_LEVELS	256
// Number of priority levels reserved for system (IRQ bottom-halves etc.). Normally user tasks should have lower priority
// It is absolutely possible to run user task at system's priority levels; it should be noted what system tasks will get blocked
// and it may be sensible to run such tasks with some interrupts disabled
#define	SYS_PRIORITY_LEVELS	16
// Highest user task priority level (convention)
#define	HIGHEST_USER_PRIORITY_LEVEL	SYS_PRIORITY_LEVELS
// Default user tasks priority level
#define	DEF_PRIORITY_LEVEL	(NUM_PRIORITY_LEVELS / 2)
// Idle task priority level (should be the lowest in the system)
#define	IDLE_PRIORITY_LEVEL	(NUM_PRIORITY_LEVELS - 1)
// Define if you want to start app_entry() already in a task
#define	START_APP_IN_TASK	1

#ifdef START_APP_IN_TASK
// Priority level of init task (app_entry() will start with this priority level)
#define	INIT_TASK_PRIORITY	DEF_PRIORITY_LEVEL
#define	INIT_TASK_OPTIONS	OPT_TIMESHARE
#endif	// START_APP_IN_TASK

#define	NUM_TASK_STATES	2
#define	MAX_MESSAGES	1024

//#define	STACK_START		0x80000
#define	DEF_STACK_SIZE	0x80000
//#define	IDLE_STACK		0x88000


/*
 *	IRQ configuration
 */
#define	MAX_IRQ_HANDLERS	10

/*
 *	IRQ BH tasks priority levels. 
 */
#define	IRQ0_BH_LEVEL		0	
#define	IRQ1_BH_LEVEL		1
#define	IRQ2_BH_LEVEL		10
#define	IRQ3_BH_LEVEL		11
#define	IRQ4_BH_LEVEL		12
#define	IRQ5_BH_LEVEL		13
#define	IRQ6_BH_LEVEL		14
#define	IRQ7_BH_LEVEL		15
#define	IRQ8_BH_LEVEL		2
#define	IRQ9_BH_LEVEL		3
#define	IRQ10_BH_LEVEL		4
#define	IRQ11_BH_LEVEL		5
#define	IRQ12_BH_LEVEL		6
#define	IRQ13_BH_LEVEL		7
#define	IRQ14_BH_LEVEL		8
#define	IRQ15_BH_LEVEL		9

#define	NETIF0_IRQ_BH_PRIORITY	IRQ10_BH_LEVEL

// Include POSIX I/O
#ifdef CFG_POSIXIO
#define	POSIX_IO	1	// Don't modify this, modify CFG_POSIXIO in makefile instead
#endif // CFG_POSIXIO

#ifdef CFG_NETWORK
// Include sockets
#define	SOCKETS	1		// Don't modify this, TCPIP doesn't live happily without SOCKETS. Modify CFG_NETWORK in makefile instead

// TCP/IP
#define	TCPIP	1		// Don't modify this, SOCKETS doesn't live happily without TCPIP. Modify CFG_NETWORK in makefile instead
#endif // CFG_NETWORK

/*
 *	Filesystems configuration
 */
// Size of files[] array - also number of file descriptors. Includes files, devices and sockets
#define	MAX_FILES	100

// Ext2 spexifics
//#define	CFG_EXT2_BLOCK_CACHE_SIZE	128	

// Maximum number of disks (any kinds) in the system (disks may be removable!)
#define	MAX_DISKS	10

// Maximum number of fileystems in the system (along with disks, may be removable)
#define	MAX_FILESYSTEMS	10

/*
 *	PCI configuration
 */
// PCI devices address space (256M)
// Things like this and the next (platform-specific) should move to $(ARCH_DIR)/config-arch.h or $(ARCH_DIR)/config-mach.h
#define	PCI_START_ADDR	0xC0000000
#define	PCI_END_ADDR	0xD0000000

// Specific PCI devices that need configuration
#define	AM79970_START_ADDR	0xC0000000
#define	AM79970_END_ADDR	0xC0000020				// 32 bytes

// Maximum number of nextwork interfaces (table size)
#define	MAX_NET_INTERFACES	10

// ARP table size
#define	ARP_TBL_SIZE	256

// Ethernet switch table size
#define	ETH_TBL_SIZE	256

/*
 *	TCP/IP configuration
 */
// Default IP address
// QEMU
//#define	DEF_IP_ADDR	"\xC0\xA8\x1\x32"
//#define	DEF_IP_ADDR_STR	"192.168.1.50"
// VmWare
//#define	DEF_IP_ADDR	"\x0A\x0\x0\x32"
//#define	DEF_IP_ADDR_STR	"10.0.0.50"
// EVMDM6467
#define	DEF_IP_ADDR	"\xAC\x16\xB8\xEA"
#define	DEF_IP_ADDR_STR	"172.22.184.234"

// Maximum number of simultaneously handled fragmented IP packets (if additional fregemented packet arrives, it will be dropped)
#define	IP_FRAG_TBL_SIZE	10

// TCP retransmission definitions
#define	MAX_TCP_RETRANSMISSIONS	3	
#define	DEF_TCP_RETRANSMIT_INTERVAL	(3*TICKS_PER_SEC)

/*
 *	IDE configuration
 */
// Number of IDE controllers on target machine
#if defined (pc)
#define	IDE_NUM_BUSES		2	
#elif defined (evmdm6467)
#define	IDE_NUM_BUSES		2
#endif // pc

// IDE disks are 4 sequential disk_nums, starting from `FIRST_IDE_DISK'
#define	FIRST_IDE_DISK	0


/*
 *	I2C configuration
 */
#ifdef CFG_I2C
// Determines whether use 7-bit (normal) or 10-bit (extended) addresses for transfers
#define	I2C_EXT_ADDR	1
#endif


/*
 *	Video configuration
 */
#ifdef CFG_VIDEO

#define	VIDEO_BUF_SIZE	(720 * 16 / 9 * 720 * 2)		// Define to accomodate application's maximum resolution. Now - 720P, YUV4:2:2

// Check memory limitations before configuring number of video buffers!
#define	NUM_VIDEO_BUFS_IN	0x3		// Input ring length
#define	NUM_VIDEO_BUFS_OUT	0x3		// Output ring length

#endif

// Names for POSIX-style stdin, stdout and stderr devices
#define	STDIN_DEVNAME	"/dev/keyboard"
#define	STDOUT_DEVNAME	"/dev/16x50"
#define	STDERR_DEVNAME	"/dev/16x50"

// Maximum command parameters for programs
#define	MAX_CMD_PARAMS	256

// System interrupt controller configuration (works!)
#define	MASK_UNHANDLED_INTR	1						// Mask all interrupts that don't have callbacks installed


#ifdef	CFG_DEVMAN

#include	"drvint.h"

/*
 *	Drivers configuration.
 *
 *	In order to configure a driver:
 *
 *	1) Compile the driver according to "SeptemberOS Drivers" documentation
 *	2) Decide upon driver's ID. It is recommended that drivers ID is defined, but not necessary.
 *	3) Initialize the drv_entry table with pointers to drv_entry tables. The order in which the entries
 *		appear determine drivers IDs.
 *	4) Define driver_entry structures as extern.
 *	5) Set NUM_DRIVER_ENTRIES as necessary.
 *	6) Link the project with the necessary drivers' object files or libs.
 *
 *	E.g.:
 *
 *		extern	drv_entry	timer_entry;
 *		extern	drv_entry	keyboard_entry;
 *
 *		drv_entry	*driver_entries[NUM_DRIVER_ENTRIES] =
 *		{
 *			timer_entry,
 *			keyboard_entry
 *		};
 */

#if defined (CFG_UART_16x50) || defined (CFG_UART_PL011)
DECLARE_DRIVER(uart, 3, -1)
#endif // CFG_UART_16x50 || CFG_UART_PL011
#ifdef CFG_KEYBOARD
DECLARE_DRIVER(keyboard, 1, -1)
#endif // CFG_KEYBOARD
#ifdef CFG_TERMINAL
DECLARE_DRIVER(terminal, -1, -1)
#endif // CFG_TERMINAL
#ifdef CFG_PCIHOST
DECLARE_DRIVER(pcihost, -1, -1)
#endif // CFG_PCIHOST
#ifdef CFG_NIC_8255x
DECLARE_DRIVER(i8255x, -1, -1)		/* Must be after pcihost. IRQ will be defined later */
#endif // CFG_NIC_8255x
#ifdef CFG_NIC_PCNET32
DECLARE_DRIVER(am79970, -1, -1)		/* Must be after pcihost. IRQ will be defined later */
#endif // CFG_NIC_PCNET32
#ifdef CFG_NIC_DM646x_EMAC
DECLARE_DRIVER(dm646x_emac, -1, -1)
#endif // CFG_NIC_DM646x_EMAC
#ifdef CFG_NIC_SMSC91C111
DECLARE_DRIVER(smsc91c111, -1, -1)
#endif // CFG_NIC_SMSC91C111
#ifdef CFG_EHCI_USBHOST
DECLARE_DRIVER(ehci, -1, -1)		/* Must be after pcihost. IRQ will be defined later */
#endif // CFG_EHCI_USBHOST
//DECLARE_DRIVER(uhci, -1, -1)		/* Must be after pcihost. IRQ will be defined later */
//DECLARE_DRIVER(dm646x_usb, -1, -1)
#ifdef CFG_DM646x_I2C
DECLARE_DRIVER(dm646x_i2c, -1, -1)
#endif
#ifdef CFG_DM646x_VPIF
DECLARE_DRIVER(dm646x_vpif, -1, -1)
#endif // CFG_DM646x_VPIF
#ifdef CFG_VIDEO_DEC_TVP7002
DECLARE_DRIVER(tvp7002, -1, -1)
#endif // CFG_VIDEO_DEC_TVP7002
#ifdef CFG_IDE
DECLARE_DRIVER(ide, -1, -1)
#endif //  CFG_IDE

// (!) Order of enumerations must be the same as order of device structures in `driver_entries' (this enum is used as index)
enum	
{
#if defined (CFG_UART_16x50) || defined (CFG_UART_PL011)
	UART_DEV_ID,
#endif // CFG_UART_16x50 || CFG_UART_PL011
#ifdef CFG_KEYBOARD
	KBD_DEV_ID,
#endif // CFG_KEYBOARD
#ifdef CFG_TERMINAL
	TERM_DEV_ID,
#endif // CFG_TERMINAL
#ifdef CFG_PCIHOST
	PCIHOST_DEV_ID,
#endif // CFG_PCIHOST
#ifdef CFG_IDE
	IDEHOST_DEV_ID,
#endif // CFG_IDE
#ifdef CFG_NIC_8255x
	I8255X_DEV_ID,
#endif // CFG_NIC_8255x
#ifdef CFG_NIC_PCNET32
	AM79970_DEV_ID,
#endif // CFG_NIC_PCNET32
#ifdef CFG_NIC_DM646x_EMAC
	DM646x_EMAC_DEV_ID,
#endif // CFG_NIC_DM646x_EMAC
#ifdef CFG_NIC_SMSC91C111
	SMSC91C111_DEV_ID,
#endif // CFG_NIC_SMSC91C111
#ifdef CFG_DM646x_I2C
	DM646x_I2C_DEV_ID,
#endif // CFG_DM646x_I2C
#ifdef CFG_DM646x_VPIF
	DM646x_VPIF_DEV_ID,
#endif // CFG_DM646x_VPIF
#ifdef CFG_VIDEO_DEC_TVP7002
	TVP7002_DEV_ID,
#endif // CFG_VIDEO_DEC_TVP7002
#ifdef CFG_EHCI_USBHOST
	EHCI_DEV_ID,
#endif // CFG_EHCI_USBHOST
};
#define	INVALID_DEV_ID	0xFFFF

#ifdef	DEVMAN__C
/*
 * (!) Order of the entries defines order of initialization! It is important to place entries dependant drivers
 * AFTER drivers on which they depend. E.g. PCI devices drivers must be places after PCI host driver
 */

drv_entry	*driver_entries[] = 
{
#if defined (CFG_UART_16x50) || defined (CFG_UART_PL011)
	&uart,
#endif // CFG_UART_16x50 || CFG_UART_PL011
#ifdef CFG_KEYBOARD
	&keyboard, 
#endif // CFG_KEYBOARD
#ifdef CFG_TERMINAL
	&terminal,
#endif // CFG_TERMINAL
#ifdef CFG_PCIHOST
	&pcihost,
#endif // CFG_PCIHOST
#ifdef CFG_IDE
	&ide,
#endif // CFG_IDE
#ifdef CFG_NIC_8255x
	&i8255x,
#endif // CFG_NIC_8255x
#ifdef CFG_NIC_PCNET32
	&am79970,
#endif // CFG_NIC_PCNET32
#ifdef CFG_NIC_DM646x_EMAC
	&dm646x_emac, 
#endif // CFG_NIC_DM646x_EMAC
#ifdef CFG_NIC_SMSC91C111
	&smsc91c111, 
#endif // CFG_NIC_SMSC91C111
#ifdef CFG_DM646x_I2C
	&dm646x_i2c, 
#endif // CFG_DM646x_I2C
#ifdef CFG_DM646x_VPIF
	&dm646x_vpif, 
#endif // CFG_DM646x_VPIF
#ifdef CFG_VIDEO_DEC_TVP7002
	&tvp7002, 
#endif // CFG_VIDEO_DEC_TVP7002
#ifdef CFG_EHCI_USBHOST
	&ehci, 
#endif // CFG_EHCI_USBHOST
//	&uhci,
//	&dm646x_usb,
};
#define	NUM_DRIVER_ENTRIES (sizeof(driver_entries) / sizeof(drv_entry*))
const int	num_driver_entries = NUM_DRIVER_ENTRIES;

struct
{
	char	*dev_name;
	unsigned long	dev_id;		
} dev_tbl[] =
{
#if defined (CFG_UART_16x50) || defined (CFG_UART_PL011)
	{"/dev/16x50", DEV_ID(UART_DEV_ID, 0)},
#endif // CFG_UART_16x50 || CFG_UART_PL011
#ifdef CFG_KEYBOARD
	{"/dev/keyboard", DEV_ID(KBD_DEV_ID, 0)},
#endif // CFG_KEYBOARD
#ifdef CFG_TERMINAL
	{"/dev/terminal", DEV_ID(TERM_DEV_ID, 0)},
#endif // CFG_TERMINAL
#ifdef CFG_PCIHOST
	{"/dev/pcihost", DEV_ID(PCIHOST_DEV_ID, 0)},
#endif // CFG_PCIHOST
#ifdef CFG_NIC_8255x
	{"/dev/8255x", 	DEV_ID(I8255X_DEV_ID, 0)},
#endif // CFG_NIC_8255x
#ifdef CFG_IDE
	{"/dev/idehost", DEV_ID(IDEHOST_DEV_ID, 0)},
#endif // CFG_IDE
#ifdef CFG_NIC_PCNET32
	{"/dev/am79970", DEV_ID(AM79970_DEV_ID, 0)},
#endif // CFG_NIC_PCNET32
#ifdef CFG_EHCI_USBHOST
	{"/dev/ehci", DEV_ID(EHCI_DEV_ID, 0)},
#endif // CFG_EHCI_USBHOST
//	{"/dev/uhci", DEV_ID(UHCI_DEV_ID, 0)},
#ifdef CFG_NIC_DM646x_EMAC
	{"/dev/dm646x-emac", DEV_ID(DM646x_EMAC_DEV_ID, 0)},
#endif // CFG_NIC_DM646x_EMAC
#ifdef CFG_NIC_SMSC91C111
	{"/dev/smsc91c111", DEV_ID(SMSC91C111_DEV_ID, 0)},
#endif // CFG_NIC_SMSC91C111
#ifdef CFG_DM646x_I2C
	{"/dev/dm646x-i2c", DEV_ID(DM646x_I2C_DEV_ID, 0)},
#endif // CFG_DM646x_I2C
#ifdef CFG_DM646x_VPIF
	{"/dev/dm646x-vpif", DEV_ID(DM646x_VPIF_DEV_ID, 0)},
#endif // CFG_DM646x_VPIF
#ifdef CFG_VIDEO_DEC_TVP7002
	{"/dev/tvp7002", DEV_ID(TVP7002_DEV_ID, 0)},
#endif // CFG_VIDEO_DEC_TVP7002
//	{"/dev/dm646x-usb", DEV_ID(DM646x_USB_DEV_ID, 0)},
};
#define	NUM_DEV_TBL_ENTRIES	(sizeof(dev_tbl) / sizeof(dev_tbl[0]))
const int	num_dev_tbl_entries = NUM_DEV_TBL_ENTRIES;

#else	// !DEVMAN__C

extern drv_entry       *driver_entries[];
extern const int       num_driver_entries;
extern const int       num_dev_tbl_entries;

#endif	// DEVMAN__C

#endif	// CFG_DEMVAN

/*
 *	System tasks (IRQ bottom-halves and such). If init task is defined to run, they will be started before app_entry().
 *	Otherwise, user entry must start them in order to ensure proper operation of configured system
 */

//#define	SYSTEM_TASKS	1

#ifdef SYSTEM_TASKS
#ifdef SOSBASIC__C

#ifdef CFG_NIC_PCNET32
void	am79970_isr_bh(void *prm);
#endif // CFG_NIC_PCNET32

struct
{
	TASK_ENTRY	entry;
	int	priority;
}
sys_tasks[] =
{
#ifdef CFG_NIC_PCNET32
	{am79970_isr_bh, NETIF0_IRQ_BH_PRIORITY}
#endif // CFG_NIC_PCNET32
};
#define	NUM_SYS_TASKS	(sizeof(sys_tasks) / sizeof(sys_tasks[0]))
const int	num_sys_tasks = NUM_SYS_TASKS;

#else	// SOSBASIC__C

extern const int	num_sys_tasks;

#endif // SOSBASIC__C
#endif // SYSTEM_TASKS

#endif	//CONFIG__H

