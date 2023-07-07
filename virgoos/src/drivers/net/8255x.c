/***************************************************
 *
 * 	8255x.c
 *
 *	Intel 8255x (e100) family ethernet driver
 *
 ***************************************************/

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "pcihost.h"
#include "errno.h"
#include "8255x.h"
#include "inet.h"


#define	DEBUG_IRQ	1
#define	DEBUG_INIT	1
//#define	DEBUG_TEST	1
#define	DEBUG_PCI_CONFIG	1
#define	SEND_TEST_FRAME	1

#define	ETH_ADDR	"\x10\x22\x33\x0B\x0C\x0D"

extern	struct net_if	net_interfaces[MAX_NET_INTERFACES];

static void	i8255x_get_send_packet(unsigned char **payload);
static int	i8255x_send_packet(unsigned char *dest_addr, word protocol, unsigned size);

unsigned char	remote_ip_addr[4];

struct eth_device	i8255x_eth_device =
{
	ETH_DEV_TYPE_NIC,
	ETH_ADDR,
	i8255x_get_send_packet,
	i8255x_send_packet
};

int	pci_dev_index = -1;
volatile struct	scb	*scb;		/* For memory access */
dword	scb_port;				/* For I/O access */

/* Test transmit control block */
struct transmit_cb	test_tcb;
static char	*transmit_data = test_tcb.frame_data;

struct	addr_setup_cb	addr_setup_cb;

//unsigned char	eth_addr[6] = "\x11\x22\x33\x0B\x0C\x0D";
//unsigned char	remote_eth_addr[6];

/* Receive frame descriptors */
struct	rfd	work_rfd[NUM_RECV_BUFFERS];
struct	rfd	*head_rfd, *curr_rfd;
dword	frames_count;

#define	EEPROM_STUFF		0x4800	/* We'll see if this is necessary */
#define	EEPROM_CMD_READ		0x0002
#define	EEPROM_CMD_WRITE	0x0001
#define	EEPROM_CMD_ERASE	0x0003
#define	EEPROM_CMD_ERASE_EX	0


static void	init_reception(void);
#ifdef	SEND_TEST_FRAME
static	void	send_test_frame(void);
#endif
static void	i8255x_isr_bh(void);

static int	fill_test_data = 1;

/*
 *	IRQ bottom-half. Handles protocol parsing.
 *
 *	Idea:
 *		1) Interrupt handler picks a buffer that received data off the buffers queue for level 3 protocol parsing
 *		and level 4 processing.
 *
 *		2) When buffers are below watermark (not available), the receive operation is not initiated and the
 *		forthcomming data are lost.
 *
 *	TODO: define a common interface for all network cards.
 */
static void	i8255x_isr_bh(void)
{
}



/*
 *	Sets the 8255x to transmit one packet
 *	Buffer used is test_tcb. The caller is responsible to fill frame_data, frame_size (protocol type)
 *	
 */
static void	i8255x_transmit_packet(char *dest_addr, char *src_addr, unsigned size)
{
	// Wait until the command is accepted, and a new command may be issued
	while ((scb->command_word & 0xFF) != 0)
		;

	memcpy(&test_tcb.frame_hdr.dest_addr, dest_addr, 6);
	memcpy(&test_tcb.frame_hdr.src_addr, src_addr, 6);

	/* Set-up TCB for one transmit command */
	test_tcb.status = 0;			/* We'll check for status later */
	test_tcb.command = CB_CMD_TRANSMIT | CB_CMD_ENDOFLIST | CB_CMD_COMPLETEINT;
	test_tcb.link_addr = 0xFFFFFFFF;	/* No need - this block is the last */
	test_tcb.tbd_array_addr = 0xFFFFFFFF;	/* No need - simplified mode */
	test_tcb.byte_count = 0x8000 | size + sizeof(struct eth_frame_hdr);	/* size+18 bytes + EOF is set */
	test_tcb.transmit_threshold = 1;		/* Start transmitting when 8 bytes are ready */
	test_tcb.tbd_num = 0;					/* No need - simplified mode */

	/* Start transmit command */
	scb->gen_ptr = (dword)&test_tcb;
	scb->command_word = SCB_CMD_CUC_START;
}

static void    i8255x_get_send_packet(unsigned char **payload)
{
	*payload = transmit_data + sizeof(struct eth_frame_hdr);
}

/*
 *	Interface function that sends a packet
 */
static int	i8255x_send_packet(unsigned char *dest_addr, word protocol, unsigned size)
{
	test_tcb.frame_crc = 0;
	test_tcb.frame_hdr.frame_size = protocol;
	i8255x_transmit_packet(dest_addr, ETH_ADDR, size);
	return	0;
}

/*
 *	8255x IRQ handler.
 */
static	int	i8255x_isr( void )
{
	int	i;
	static int	irq_count;
	struct	llc_hdr	*llc_hdr;
	struct	snap_hdr *snap_hdr;
	word	proto_type;
	unsigned char	*pdata;

#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received 8255x IRQ no. %d\r\n", __func__, ++irq_count);
#endif

	/* Print the interrupt cause: SCB status register */
#ifdef	DEBUG_IRQ
	serial_printf("%s(): status_word=%04X\r\n", __func__, scb->status_word);
#endif

	/* Received a frame */
	if ((scb->status_word & SCB_STS_FRAMERECV) != 0)
	{
#ifdef	DEBUG_IRQ
		static int	recv_irq_count;

		// Dump dest::source addresses
		for (i = 0; i < 6; ++i)
			serial_printf("%02X:", curr_rfd->frame_hdr.dest_addr[i]);
		for (i = 0; i < 6; ++i)
			serial_printf(":%02X", curr_rfd->frame_hdr.src_addr[i]);
		serial_printf("\r\n");
		serial_printf("Received 8255x SCB_STS_FRAMERECV IRQ no. %d\r\n", ++recv_irq_count);
#endif
		curr_rfd->frame_hdr.frame_size = htons(curr_rfd->frame_hdr.frame_size);
		memcpy(i8255x_eth_device.remote_eth_addr, curr_rfd->frame_hdr.src_addr, 6);

		pdata = (unsigned char*)&curr_rfd->frame_hdr;
		eth_parse_packet(&net_interfaces[1], pdata);		// TODO: invent something more configurable then hardcoded "1" index
		

#if 0
		// TODO: shouldn't this move out to generic ethernet frame parsing?
		if ((curr_rfd->status & RFD_STS_TYPEORLEN) != 0)
		{
#ifdef	DEBUG_IRQ
			serial_printf("Ethernet-II frame: eth. type=%04X\r\n", 
				curr_rfd->frame_hdr.frame_size
			);
#endif
			proto_type = curr_rfd->frame_hdr.frame_size;
			pdata = curr_rfd->frame_data;
		}
		else
		{
			llc_hdr = (struct llc_hdr*)curr_rfd->frame_data;
#ifdef	DEBUG_IRQ
			serial_printf("802.3 frame: length=%04X. LLC header: DSAP=%c, %d SSAP=%c, %d Control = %02X\r\n",
				curr_rfd->frame_hdr.frame_size,
				(llc_hdr->dsap & 1) != 0 ? 'G' : 'I',
				llc_hdr->dsap & ~1,
				(llc_hdr->ssap & 1) != 0 ? 'C' : 'R',
				llc_hdr->ssap & ~1,
				llc_hdr->control
			);
#endif

			/* SNAP */
			if ((llc_hdr->dsap & ~1) == 0xAA && (llc_hdr->ssap & ~1) == 0xAA)
			{
				snap_hdr = (struct snap_hdr*)(curr_rfd->frame_data + 3);
				snap_hdr->type = htons(snap_hdr->type);
				serial_printf("SNAP: OUI=%02X%02X%02X, type = %04X\r\n",
					snap_hdr->oui[0], snap_hdr->oui[1], snap_hdr->oui[2],
					snap_hdr->type
				);

				proto_type = snap_hdr->type;
				pdata = curr_rfd->frame_data + 3 + 5;
			}
		}

		// Handle ARP
		if (PROTO_TYPE_ARP == proto_type)
		{
			parse_arp(&i8255x_eth_device, pdata);
		}
		// Handle IP
		else if (PROTO_TYPE_IP == proto_type)
		{
			struct	ip_hdr	*ip_hdr = (struct ip_hdr*)pdata;
			
			// Save remote IP address (header is better?..)
			//memmove(remote_ip_addr, &ip_hdr->src_ip, 4);
			*(unsigned long*)remote_ip_addr = ip_hdr->src_ip;

			// Update ARP tables with the newcomming address
			update_arp_tbl(remote_ip_addr, &curr_rfd->frame_hdr);
			parse_ip(&i8255x_eth_device, pdata);
		}

		///////////////////////////
		//udelay(50);
		//init_reception();
		///////////////////////////
#endif	// 0

		curr_rfd->status = 0;
		curr_rfd->command = 0;
		curr_rfd->actual_count = 0;
		curr_rfd->size = 1518;
		
		++curr_rfd;
		if (&work_rfd[NUM_RECV_BUFFERS] == curr_rfd)
			curr_rfd = &work_rfd[0];
	}

	/* Acknowledge all interrupts... */
	scb->status_word = scb->status_word & 0xFF00;

	return	1;		/* Interrupt was handled */
}

/*
 *	Issues cmd to EEPROM. cmd contains cmd_len bits (3 or 5), NOT including the start bit
 *	ERASE_EX commands should call with cmd_len = 4 and addr_len < 0
 *	Other commands should call with cmd_len = 2 and addr_len = 6
 *
 *	udelay()s will be shorter when they will be more precise
 */
static	void	eeprom_cmd(int cmd, int addr)
{
	word	dummy;
	int	cmd_len, addr_len;

	if (EEPROM_CMD_ERASE_EX == cmd)
	{
		cmd_len = 3;
		addr_len = -1;
	}
	else
	{
		cmd_len = 1;
		addr_len = 13;
	}

	/* Memory-mapped access */

	/* Enable EEPROM */
	scb->eeprom_ctl = (EEPROM_STUFF | EECS);
	udelay(10);

	/* Start bit */
	scb->eeprom_ctl = (EEDI | EEPROM_STUFF | EECS | EESK);
	udelay(10);
	scb->eeprom_ctl = (EEDI | EEPROM_STUFF | EECS);
	udelay(10);
	while (cmd_len-- >= 0)
	{
		/* Write bit with clock == 1 */
		scb->eeprom_ctl = ((1 << cmd_len) & cmd) != 0 ? (EEDI | EEPROM_STUFF | EECS | EESK) : (EEPROM_STUFF | EECS | EESK);
		udelay(10);
		/* Write bit with clock == 0 */
		scb->eeprom_ctl = ((1 << cmd_len) & cmd) != 0 ? (EEDI | EEPROM_STUFF | EECS) : (EEPROM_STUFF | EECS);
		udelay(10);
	}
	while (addr_len-- >= 0)
	{
		/* Write bit with clock == 1 */
		scb->eeprom_ctl = ((1 << addr_len) & addr) != 0 ? (EEDI | EEPROM_STUFF | EECS | EESK) : (EEPROM_STUFF | EECS | EESK);
		udelay(10);
		/* Write bit with clock == 0 */
		scb->eeprom_ctl = ((1 << addr_len) & addr) != 0 ? (EEDI | EEPROM_STUFF | EECS) : (EEPROM_STUFF | EECS);
		udelay(10);

		/* Dummy-read */
		scb->eeprom_ctl = (EEPROM_STUFF | EECS | EESK);
		udelay(10);
		dummy = scb->eeprom_ctl & EEDO;
		scb->eeprom_ctl = (EEPROM_STUFF | EECS);
		udelay(10);

		/* Is it correct ??? Intel's manual is not very clear on this */
		if (dummy == 0)
			break;
	}

/*
	while (dummy != 0)
		dummy = scb->eeprom_ctl & EEDO;
*/
}


static void	eeprom_endcmd(void)
{
	scb->eeprom_ctl = (EEPROM_STUFF);
	udelay(10);
}


word	eeprom_read(int addr)
{
	word	res = 0;
	int	i;

	eeprom_cmd(EEPROM_CMD_READ, addr);
	for (i = 15; i >= 0; --i)
	{
		scb->eeprom_ctl = (EEPROM_STUFF | EECS | EESK);
		udelay(10);
		res |= ((scb->eeprom_ctl & EEDO) >> 3) << i;
		scb->eeprom_ctl = (EEPROM_STUFF | EECS);
		udelay(10);
	}
	eeprom_endcmd();
}


/*
 *	Initialize RFDs
 */
static void	init_reception(void)
{
	int	i;

	// Wait until the command is accepted, and a new command may be issued
	while ((scb->command_word & 0xFF) != 0)
		;

	/* Set-up receive descriptors in cyclic queue */
	for (i = 0; i < NUM_RECV_BUFFERS; ++i)
	{
		work_rfd[i].status = 0;
		work_rfd[i].command = 0;
		if (NUM_RECV_BUFFERS - 1 == i)
			work_rfd[i].link_addr = (dword)&work_rfd[0];
		else
			work_rfd[i].link_addr = (dword)&work_rfd[i+1];
		work_rfd[i].actual_count = 0;
		work_rfd[i].size = 1518;
	}

	// Init curr_rfd
	curr_rfd = &work_rfd[0];

#if 0
	work_rfd[1].status = 0;
	work_rfd[1].command = RFD_CMD_ENDOFLIST;
	work_rfd[1].link_addr = 0xFFFFFFFF;	/* No need - this block is the last */
	work_rfd[1].actual_count = 0;
	work_rfd[1].size = 1518;
#endif

	/* Enable reception */
	scb->gen_ptr = (dword)&work_rfd[0];
	scb->command_word = SCB_CMD_RUC_START;
	//udelay(1);
	
#ifdef	DEBUG_INIT
	serial_printf("(RUC_START) command_word=%04X, status_word=%04X, RFD status=%04X\r\n", 
		scb->command_word, scb->status_word, curr_rfd->status);
#endif
}


/*
 *	Initialize SCB
 */
static void	init_scb(void)
{
	int	i;

	// Wait until the command is accepted, and a new command may be issued
	while ((scb->command_word & 0xFF) != 0)
		;
	
	/* Load CU base address reg. with 0 (we use offsets as direct addresses) */
	scb->status_word = 0;
	scb->gen_ptr = 0;
	scb->command_word = SCB_CMD_CUC_LOADCUBASE;
	//udelay(1);
#ifdef	DEBUG_INIT
	serial_printf("(LOADCUBASE) command_word=%04X\r\n", scb->command_word);
#endif

	// Wait until the command is accepted, and a new command may be issued
	while ((scb->command_word & 0xFF) != 0)
		;

	/* Load RU base address reg. with 0 (we use offsets as direct addresses) */
	scb->gen_ptr = 0;
	scb->command_word = SCB_CMD_RUC_LOADRUBASE;
	//udelay(1);
#ifdef	DEBUG_INIT
	serial_printf("(LOADRUBASE) command_word=%04X\r\n", scb->command_word);
#endif

	/* (!) Needed a delay here... how much? */
	//udelay(1000);
	// Wait until the command is accepted
	// Wait until the command is accepted, and a new command may be issued
	while ((scb->command_word & 0xFF) != 0)
		;

	/* Set unique address (ethernet address of the card) */
	addr_setup_cb.status = 0;
	addr_setup_cb.command = CB_CMD_ADDRSETUP | CB_CMD_ENDOFLIST;
	addr_setup_cb.link_addr = 0xFFFFFFFF;
	for (i = 0; i < 6; ++i)
		addr_setup_cb.address[i] = i8255x_eth_device.addr[i];

	scb->gen_ptr = (dword)&addr_setup_cb;
	scb->command_word = SCB_CMD_CUC_START;
	
	//udelay(1000);

#ifdef	DEBUG_INIT
	/* Print TCB status */
	serial_printf("(ADDRSETUP) command_word=%04X, status_word=%04X, TCB status=%04X\r\n",
		scb->command_word, scb->status_word, addr_setup_cb.status);
#endif

	/* Init RFDs */
	init_reception();
}

#ifdef	SEND_TEST_FRAME
/*
 *	Send a test frame to broadcast address
 */
static	void	send_test_frame(void)
{
	int	i;

	if (fill_test_data)
	{
		/* Set-up correct ethernet header */
		/* ... */
		/* Fill destination address */
		for (i = 0; i < 6; ++i)
			test_tcb.frame_hdr.dest_addr[i] = 0xFF;

		/* Fill source address */
		for (i = 0; i < 6; ++i)
			test_tcb.frame_hdr.src_addr[i] = i8255x_eth_device.addr[i];
		test_tcb.frame_crc = 0;					/* Device will put it */


		/* Fill packet with test data */
		for (i = 0; i < 1500; ++i)
			test_tcb.frame_data[i] = i*2+1 & 0xFF;
	}

	/* Set-up TCB for one transmit command */
	test_tcb.status = 0;			/* We'll check for status later */
	test_tcb.command = CB_CMD_TRANSMIT | CB_CMD_ENDOFLIST | CB_CMD_COMPLETEINT;
	test_tcb.link_addr = 0xFFFFFFFF;	/* No need - this block is the last */
	test_tcb.tbd_array_addr = 0xFFFFFFFF;	/* No need - simplified mode */
	test_tcb.byte_count = 0x8000 | 1518;	/* 1500 bytes + EOF is set */
	test_tcb.transmit_threshold = 1;		/* Start transmitting when 8 bytes are ready */
	test_tcb.tbd_num = 0;					/* No need - simplified mode */


#ifdef	DEBUG_TEST
	/* Dump the RFD data area BEFORE sending broadcast */
	for (i = 0; i < 6; ++i)
		serial_printf("%02X:", work_rfd[0].frame_hdr.dest_addr[i]);
	for (i = 0; i < 6; ++i)
		serial_printf(":%02X", work_rfd[0].frame_hdr.src_addr[i]);
	serial_printf(" -- data -- ");
	for (i = 0; i < 10; ++i)
		serial_printf(" %02X", work_rfd[0].frame_data[i]);
	serial_printf("\r\n");
#endif

	/* Load the 1-command command list to the SCB */

	/* Start transmit command */
	scb->gen_ptr = (dword)&test_tcb;
	scb->command_word = SCB_CMD_CUC_START;
	
	udelay(1000);

#ifdef	DEBUG_TEST
	/* Print TCB status */
	serial_printf("command_word=%04X, status_word=%04X, TCB status=%04X\r\n", 
		scb->command_word, scb->status_word, test_tcb.status);

	/* Print RFD status */
	serial_printf("RFD status=%04X, RFD actual count=%04X\r\n", 
		work_rfd[0].status, work_rfd[0].actual_count);
#endif

	/* Dump the RFD data area AFTER sending broadcast (test that we received it) */
/*
	for (i = 0; i < 6; ++i)
		printfxy(i*3, 14, "%02X:", work_rfd[0].frame_hdr.dest_addr[i]);
	for (i = 0; i < 6; ++i)
		printfxy(i*3 + 18, 14, ":%02X", work_rfd[0].frame_hdr.src_addr[i]);
	for (i = 0; i < 30; ++i)
		printfxy(i*3 + 40, 14, " %02X", work_rfd[0].frame_data[i]);

	for (i = 0; i < 6; ++i)
		printfxy(i*3, 16, "%02X:", work_rfd[1].frame_hdr.dest_addr[i]);
	for (i = 0; i < 6; ++i)
		printfxy(i*3 + 18, 16, ":%02X", work_rfd[1].frame_hdr.src_addr[i]);
	for (i = 0; i < 30; ++i)
		printfxy(i*3 + 40, 16, " %02X", work_rfd[1].frame_data[i]);
*/

	// Disable PCI interrupt
/*
	scb->command_word = SCB_CMD_MASK_INTA | SCB_CMD_CUC_NOP;
	udelay(10000);
*/
}
#endif


/*
 *	Initialize the ethernet controller (according to Software Developer's Manual's
 *	recommendation)
 */
int	i8255x_init(unsigned drv_id)
{
	dword	value = -1;
	word	eeprom_val;
	int	i;
	int	int_line;

	/* Open PCI host driver - no need */

	/* Find 8255x device */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_FIND_DEV, VENDOR_ID, DEVICE_ID, &pci_dev_index);
	if (pci_dev_index != -1)
		serial_printf("Found Intel 8255x device at index %d\r\n", pci_dev_index);
	else
	{
		serial_printf("Intel 8255x device was not found\r\n", pci_dev_index);
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

	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR0_OFFS, &value);
	scb = (struct scb*)(value & ~0xF);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR1_OFFS, &value);
	scb_port = (value & ~0x3);

	/* Setup IRQ */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTLINE_OFFS, &int_line);
	serial_printf("The 8255x uses IRQ line %d\r\n", int_line);
	set_int_callback(int_line, i8255x_isr);

#ifdef	DEBUG_PCI_CONFIG	
	serial_printf(0, 1, "%s(): scb=%08X scb_port=%08X\r\n", __func__, scb, scb_port);
#endif

	/* Dump SCB registers */
#ifdef	DEBUG_PCI_CONFIG
	serial_printf("Status=%04X, command=%04X, gen_ptr=%08X, port=%08X\r\n", 
		scb->status_word, scb->command_word, scb->gen_ptr, scb->port);
	serial_printf("eeprom_ctl=%04X, mdi_ctl=%08X, rx_byte_count=%08X, flow_ctl=%04X\r\n",
		scb->eeprom_ctl, scb->mdi_ctl, scb->rx_byte_count, scb->flow_ctl);
	serial_printf("rmdr=%02X, gen_ctl=%02X, gen_status=%02X\r\n", scb->rmdr, scb->gen_ctl, scb->gen_status);
	serial_printf("func_event=%08X event_mask=%08X present_state=%08X force_event=%08X\r\n",
		scb->func_event, scb->event_mask, scb->present_state, scb->force_event);
#endif


	/* Test udelay */
	/* Need more precision: it's about 3 times slower	*/
/*
	for (i = 0; i < 10; ++i)
	{
		udelay(1000000);
		printfxy(0, 22, "%d", i);
	}
*/

	/* Print the Ethernet address from EEPROM */
/*
	for (i = 0; i < 256; ++i)
	{
		eeprom_val = eeprom_read(i);
		printfxy(i*20, 0, "[%d]=%04X", i, eeprom_val);
	}
*/

	//--------- ????????????? -----------
	// Need to find out why we need it here
	// Init curr_rfd
//	curr_rfd = &work_rfd[0];

	/* Initialize SCB */
	init_scb();

#ifdef	SEND_TEST_FRAME
	/* Send 1 frame to broadcast address */
	send_test_frame();
	send_test_frame();
	send_test_frame();
#endif
	
	return	0;
}

int	i8255x_deinit(void)
{
	return	0;
}


int	i8255x_open(unsigned subdev_id)
{
	return	0;
}


int i8255x_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int i8255x_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int i8255x_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	return	0;
}


int i8255x_close(unsigned sub_id)
{
	return	0;
}

struct drv_entry	i8255x = {i8255x_init, i8255x_deinit, i8255x_open, i8255x_read,
	i8255x_write, i8255x_ioctl, i8255x_close};

