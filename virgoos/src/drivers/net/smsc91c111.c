/***************************************************
 *
 * 	smsc91c111.c
 *
 *	SMSC LAN91C111 driver	
 *
 *	SMSC LAN91C111 is a "bare-bone" embedded NIC. It provides ethernet and PHY interfaces and h/w bus interface but no
 *	master host-memory interface (no integral DMA controller)
 *	Meanwhile we will do CPU s/w copy from internal 8K memory to main memory; later we'll see if we can use DMA
 *	The controller decodes 64K of memory starting from ints base address (0x10010000)
 *
 ***************************************************/

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "pcihost.h"
#include "errno.h"
#include "smsc91c111.h"
#include "inet.h"

//#define DEBUG_IRQ	1
//#define DEBUG_INIT	1
//#define DEBUG_TEST	1
//#define DEBUG_SEND	1
#define SEND_TEST_FRAME	1
//#define DEBUG_RINGS	1
//#define DEBUG_REGS	1


#define	ETH_PAD_SHORT_FRAMES	1

#define	ETH_ADDR	"\x10\x22\x33\x07\x08\x11"

extern	struct net_if	net_interfaces[MAX_NET_INTERFACES];

extern uint32_t	timer_counter;

static void	smsc91c111_get_send_packet(unsigned char **payload);
static int	smsc91c111_send_packet(unsigned char *dest_addr, word protocol, unsigned size);


struct eth_device	smsc91c111_eth_device =
{
	ETH_DEV_TYPE_NIC,
	ETH_ADDR,
	smsc91c111_get_send_packet,
	smsc91c111_send_packet
};

#ifdef	SEND_TEST_FRAME
static	void	send_test_frame(void);
static int	fill_test_data = 1;
#endif

static void	smsc91c111_isr_bh(void);


// Receive and transmit descriptors ring
struct smsc91c111_descr	*smsc91c111_recv_desc_ring;
struct smsc91c111_descr	*smsc91c111_xmit_desc_ring;

int	curr_rd, curr_td, prev_td;			// TODO: those will have to move to per-instance structure


#ifdef	DEBUG_RINGS
void	dump_recv_ring(void)
{
	int	i;
	unsigned	pkt_len;
	
	// DEBUGDEBUGDEBUG -- dump ALL receive ring
	serial_printf("----------------- Dump receive ring ------------------------\n");
	for (i = 0; i < NUM_RECV_BUFFERS; ++i)
	{
		int	j;
		unsigned char	*pkt_data = smsc91c111_recv_desc_ring[i].buf;

//		if (i != curr_rd)
//			continue;
		serial_printf("===> curr_rd = %d\n", curr_rd);
		
		// Print suitable for both 16-bit and 32-bit modes
		serial_printf("rd=%d buf=%08X count=%04X status=%04X\n", i,
			(unsigned)smsc91c111_recv_desc_ring[i].buf,
			(unsigned)smsc91c111_recv_desc_ring[i].count,
			(unsigned)smsc91c111_recv_desc_ring[i].status);
		serial_printf("Received packet:\r\n");
		serial_printf("\t");
		pkt_len = smsc91c111_recv_desc_ring[i].count;

		for(j = 0; j < pkt_len; ++j)
		{
			serial_printf("%02X ", (unsigned)pkt_data[j]);
			if (j % 16 == 15)
			{
				serial_printf("\n");
				if (j < pkt_len - 1)
					serial_printf("\t");
			}
		}
		serial_printf("\n");
	}
	serial_printf("\n"
		"============================================================\n");
}

void	dump_xmit_ring(void)
{
	int	i;
	unsigned	pkt_len;
	
	// DEBUGDEBUGDEBUG -- dump ALL transmit ring
	serial_printf("----------------- Dump transmit ring ------------------------\n");
	for (i = 0; i < NUM_XMIT_BUFFERS; ++i)
	{
		int	j;
		unsigned char	*pkt_data = smsc91c111_xmit_desc_ring[i].buf;

//		if (i != curr_td)
//			continue;
		serial_printf("===> curr_td = %d\n", curr_td);
		
		// Print suitable for both 16-bit and 32-bit modes
		serial_printf("td=%d buf=%08X count=%04X status=%04X\n", i,
			(unsigned)smsc91c111_xmit_desc_ring[i].buf,
			(unsigned)smsc91c111_xmit_desc_ring[i].count,
			(unsigned)smsc91c111_xmit_desc_ring[i].status);
		serial_printf("Transmitted packet:\n");
		serial_printf("\t");
		pkt_len = smsc91c111_xmit_desc_ring[i].count;

		for(j = 0; j < pkt_len; ++j)
		{
			serial_printf("%02X ", (unsigned)pkt_data[j]);
			if (j % 16 == 15)
			{
				serial_printf("\n");
				if (j < pkt_len - 1)
					serial_printf("\t");
			}
		}
		serial_printf("\n");
	}
	serial_printf("\n"
		"============================================================\n");
}
#endif	// DEBUG_RINGS

#ifdef	DEBUG_REGS
void	dump_regs(void)
{
	// Dump all configured LAN91C111 / LAN91C111 ctl. registers
	serial_printf("%s(): ----------------------- Dump LAN91C111 registers -------------------------------\n", __func__);
	serial_printf("%s(): --------------------- End Dump LAN91C111 registers -----------------------------\n", __func__);

}
#endif

static void	smsc91c111_isr_bh(void)
{
}

static void    smsc91c111_get_send_packet(unsigned char **payload)
{
	uint32_t	addr;

	// We don't have buffer ownership issues, since everything is handled by the CPU
	addr = *(uint32_t*)&smsc91c111_xmit_desc_ring[curr_td].buf + sizeof(struct eth_frame_hdr);
	*payload = (char*)addr;
}

/*
 *	Interface function that sends a packet
 */
static int	smsc91c111_send_packet(unsigned char *dest_addr, word protocol, unsigned size)
{
	uint32_t	addr;
	struct eth_frame_hdr	*eth_hdr;
	unsigned	i, j;
	uint32_t	timeout = timer_counter + TICKS_PER_SEC;
	unsigned	frame_num;
	uint32_t	temp;
	uint16_t	byte_count;

#ifdef	DEBUG_SEND
	serial_printf("%s(): entered, smsc91c111_xmit_desc_ring=%08X curr_td=%d\n", __func__, smsc91c111_xmit_desc_ring, curr_td);
#endif

	// Check if not currently transmitting. If yes, return EAGAIN (if the caller is in task context, it will sleep on TX event)
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 2);     // Bank #2
	temp = inw(SMSC91C111_REG_BASE + FIFO_PORTS);
	if (!(temp & FIFO_PORTS_TEMPTY))
	{
		errno = EAGAIN;
		return	-1;
	}

	addr = *(uint32_t*)&smsc91c111_xmit_desc_ring[curr_td].buf;
	eth_hdr = (struct eth_frame_hdr*)addr;
	memcpy(eth_hdr->dest_addr, dest_addr, ETH_ADDR_SIZE);
	memcpy(eth_hdr->src_addr, ETH_ADDR, ETH_ADDR_SIZE); 
	eth_hdr->frame_size = protocol;					// Already in network order
	size += sizeof(struct eth_frame_hdr);

#ifdef	ETH_PAD_SHORT_FRAMES
	// Pad an outgoing packet to 64 bytes minimum manually
	// This works; however, this doesn't solve "missing sent packets" mystery
	if (size < MIN_ETH_PACKET_SIZE)
	{
//		serial_printf("%s(): padding a short frame (size = %u) to a minimum of %u bytes\r\n", __func__, size, MIN_ETH_PACKET_SIZE);
		memset((unsigned char*)addr + size, 0, MIN_ETH_PACKET_SIZE - size);
		size = MIN_ETH_PACKET_SIZE;
	}
#endif

	smsc91c111_xmit_desc_ring[curr_td].count = size & 0xFFFF;
	smsc91c111_xmit_desc_ring[curr_td].status = 0;

	// Typical flow of events for transmit (Auto Release = 1)

	// Issue "allocate memory for TX - n bytes"
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 2);	// Bank #2
	outw(SMSC91C111_REG_BASE + MMU_CMD, MMU_CMD_ALLOC_MEM_TX);
	
	// Wait for successful completion code (with timeout 1(s))
	while (!(inw(SMSC91C111_REG_BASE + INT_STATUS) & INT_STATUS_ALLOC_INT))
		if (timeout <= timer_counter)
		{
			errno = ETIMEDOUT;
			return	-1;
		}
	frame_num = inw(SMSC91C111_REG_BASE + PACKET_NUM) & 0xFF;
	if (frame_num & PACKET_NUM_ALLOC_FAILED)
	{
		errno = EIO;
		return	-1;
	}

	// Load transmit data (copy and set control byte as necessary)
	outw(SMSC91C111_REG_BASE + PACKET_NUM, frame_num << 8);		// Set "packet number"
	outw(SMSC91C111_REG_BASE + POINTER, POINTER_AUTO_INCR);		// Auto-increment, transmit FIFO write access, pointer initialized to 0

	outw(SMSC91C111_REG_BASE + DATA, 0);				// Status word

	// Byte count
	byte_count = size + 6;
	if (size & 1)
		--byte_count;
	
	outw(SMSC91C111_REG_BASE + DATA, byte_count);				// Size

	// Copy data area
	j = size & ~0x3;
	for (i = 0; i < j; i += 4)
	{
		temp = *(uint32_t*)(addr + i);
		outd(SMSC91C111_REG_BASE + DATA, temp);
	}

	if (size & 2)
	{
		temp = *(uint16_t*)(addr + i);
		outw(SMSC91C111_REG_BASE + DATA, temp);
		i += 2;
	}
	if (size & 1)
	{
		temp = *(uint8_t*)(addr + i);
		outb(SMSC91C111_REG_BASE + DATA, temp);
		temp = 0x20;	// Odd
	}
	else
	{
		outb(SMSC91C111_REG_BASE + DATA, 0);			// Pad byte
		temp = 0;	// Even
	}
	++i;		// Now i == size or size + 1 if size is even
	temp |= 0x10;
	outb(SMSC91C111_REG_BASE + DATA, temp);			// Store control byte at odd offset (ODD? + CRC)

	// Issue "enqueue packet number to Tx FIFO" ("go")
	outw(SMSC91C111_REG_BASE + MMU_CMD, MMU_CMD_ENQ_PACKET_TX);

	prev_td = curr_td;
	curr_td = (curr_td + 1) % NUM_XMIT_BUFFERS;

	outb(SMSC91C111_REG_BASE + INT_STATUS + 1, inb(INT_STATUS + 1) | (INT_STATUS_TX_INT /*| INT_STATUS_TX_EMPTY_INT*/));

#if DEBUG_SEND
//serial_printf("+++++++++++ Adjust curr_td\r\n");
//serial_printf("+++++++++++ curr_td = %d\r\n", curr_td);
#endif

#if DEBUG_SEND
	// DEBUGDEBUGDEBUG -- dump all xmit ring
	dump_xmit_ring();
#endif
	return	0;
}


/*
 *	SMSC91C111 IRQ handler.
 */
static	int	smsc91c111_isr(void)
{
#ifdef	DEBUG_IRQ
	static int	irq_count;
#endif
	struct eth_frame_hdr	*pdata;
	uint32_t	intstat;
	uint32_t	intmask, pointer;

#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received SMSC91C111 IRQ no. %d\r\n", __func__, ++irq_count);
#endif

	// Save interrupt mask and pointer, later restore (who knows what may happen during this interrupt)
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 2);     // Bank #2	

	intmask = inb(SMSC91C111_REG_BASE + INT_STATUS + 1);
	pointer = inw(SMSC91C111_REG_BASE + POINTER); 
	outb(SMSC91C111_REG_BASE + INT_STATUS + 1, 0);

	intstat = inw(SMSC91C111_REG_BASE + INT_STATUS) & 0xFF;

#ifdef DEBUG_IRQ
	serial_printf("%s(): intstat=%08X\n", __func__, intstat);
#endif
	if (intstat & INT_STATUS_MD_INT)
	{
#ifdef DEBUG_IRQ
		serial_printf("%s(): MD_INT\n", __func__);
#endif
	}
	if (intstat & INT_STATUS_TX_INT)
	{
#ifdef DEBUG_IRQ
		serial_printf("%s(): TX_INT\n", __func__);
#endif
		// TODO: wake if somebody waits on Tx interrupt
	}
	if (intstat & INT_STATUS_RX_INT)
	{
		uint32_t	temp;
		uint16_t	pkt_num;
#ifdef DEBUG_IRQ
		serial_printf("%s(): RX_INT\n", __func__);
#endif
		// Receive all frames in Rx FIFO (may be more than 1). This will clear RX_INT bit
		do
		{
			uint16_t	status, byte_count;
			unsigned	i, j, pkt_len;
			uint8_t	*rbuf;
			uint8_t	control;

			//
			// Typical flow of events for receive
			//

			// Read rx packet number from FIFO ports (do we really need this number?)
//			pkt_num = inw(SMSC91C111_REG_BASE + FIFO_PORTS) >> 8 & 0x3F;

			// Copy packet to Rx ring, abandon CRC and control byte
			outw(SMSC91C111_REG_BASE + POINTER, (POINTER_RCV | POINTER_AUTO_INCR | POINTER_READ));		// Pointer set to 0, RCV FIFO, auto-increment, read access
			status = inw(SMSC91C111_REG_BASE + DATA);
			byte_count = inw(SMSC91C111_REG_BASE + DATA);
			pkt_len = byte_count - 4;
			rbuf = smsc91c111_recv_desc_ring[curr_rd].buf;

			for (i = 0, j = pkt_len & ~0x3; i < j; i += 4)
				*(uint32_t*)(rbuf + i) = ind(SMSC91C111_REG_BASE + DATA);

			if (j & 0x2)
			{
				*(uint16_t*)(rbuf + i) = inw(SMSC91C111_REG_BASE + DATA);
				i += 2;
			}
			control = rbuf[byte_count - 1];
			if (control & 0x20)
				--byte_count;
			else
				byte_count -= 2;
			smsc91c111_recv_desc_ring[curr_rd].count = byte_count;
			smsc91c111_recv_desc_ring[curr_rd].status = status;
#ifdef DEBUG_IRQ
			serial_printf("%s(): byte_count=%hu status=%04X\n", __func__, byte_count, status);
#endif
			// Remove packet from top of Rx
			outw(SMSC91C111_REG_BASE + MMU_CMD, MMU_CMD_REMOVE_REL_RX);

			// Parse ethernet frame
			pdata = (struct eth_frame_hdr*)smsc91c111_recv_desc_ring[curr_rd].buf;
#if DEBUG_IRQ
 #ifdef DEBUG_RINGS
	serial_printf("%s(): pdata=%08X dest_addr=%02X%02X%02X%02X%02X%02X src_addr=%02X%02X%02X%02X%02X%02X frame_size=%04X\r\n", __func__, pdata,
		(unsigned)pdata->dest_addr[0], (unsigned)pdata->dest_addr[1], (unsigned)pdata->dest_addr[2],
		(unsigned)pdata->dest_addr[3], (unsigned)pdata->dest_addr[4], (unsigned)pdata->dest_addr[5],
		(unsigned)pdata->src_addr[0], (unsigned)pdata->src_addr[1], (unsigned)pdata->src_addr[2],
		(unsigned)pdata->src_addr[3], (unsigned)pdata->src_addr[4], (unsigned)pdata->src_addr[5],
		(unsigned)pdata->frame_size);

		// DEBUGDEBUGDEBUG -- dump all receive ring
//		dump_recv_ring();
 #endif
#endif

			// if() - necessary for promiscous mode
			if (!memcmp(pdata->dest_addr, ETH_ADDR, 6) || !memcmp(pdata->dest_addr, ETH_ADDR_BROADCAST, 6))
			{
				eth_parse_packet(&net_interfaces[0], pdata);            // TODO: invent something more configurable then hardcoded "0" index
#ifdef DEBUG_IRQ
				serial_printf("%s(): eth_parse_frame() done\n", __func__);
#endif
			}

			// Advance "current receive descriptor" index
			curr_rd = (curr_rd + 1) % NUM_RECV_BUFFERS;
#ifdef DEBUG_IRQ
			serial_printf("%s(): curr_rd=%u\n", __func__, curr_rd);
#endif

			// Check if there are more frames
			temp = inw(SMSC91C111_REG_BASE + INT_STATUS) & 0xFF;
#ifdef DEBUG_IRQ
			serial_printf("%s(): temp=%08X\n", __func__, temp);
#endif
		} while (temp & INT_STATUS_RX_INT);
	}

	// Acknowledge (clear) interrupts
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 2);     // Bank #2	
	outb(SMSC91C111_REG_BASE + INT_STATUS, intstat);

#ifdef DEBUG_IRQ
	serial_printf("%s(): acknowledged interrupts. Completing\n", __func__);
#endif

	// Restore pointer and interrupt mask
	outb(SMSC91C111_REG_BASE + INT_STATUS + 1, intmask);
	outw(SMSC91C111_REG_BASE + POINTER, pointer);

	// this interrupt is not shared
	return	1;
}


#ifdef	SEND_TEST_FRAME
/*
 *	Send a test frame to broadcast address
 */
static	void	send_test_frame(void)
{
	int	i;
	unsigned	len = 1500;
	unsigned char	*transmit_data;

	smsc91c111_get_send_packet(&transmit_data);

	/* Fill packet with test data */
	for (i = 0; i < 1500; ++i)
		transmit_data[i] = i*2+1 & 0xFF;

	smsc91c111_send_packet(ETH_ADDR_BROADCAST, htons(0x0801), 1400);
	udelay(1000);
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 2);     // Bank #2
	outb(SMSC91C111_REG_BASE + INT_STATUS, 0xFF);
}
#endif


/*
 *	Initialize the ethernet controller (according to Software Developer's Manual's
 *	recommendation)
 */
int	smsc91c111_init(unsigned drv_id)
{
	uint32_t	value = -1;
	word	eeprom_val;
	int	i;
	int	int_line;
	struct smsc91c111_descr	*rx_descr, *tx_descr;
	uint32_t	macaddr;
	int	first_phy;

	// For indentification (and to see that we have addresses right): read revision register
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 3);	// Set bank #3
	serial_printf("%s(): revision register = %08X\n", __func__, (unsigned)inw(SMSC91C111_REG_BASE + REVISION));
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 1);	// Set bank #1
	serial_printf("%s(): base address register = %08X\n", __func__, (unsigned)inw(SMSC91C111_REG_BASE + BASE_ADDR));

	// Set interrupt handlers
	set_int_callback(SMSC91C111_IRQ, smsc91c111_isr);
	serial_printf("%s(): SMSC91C111 uses interrupt %d\n", __func__, SMSC91C111_IRQ);

	//
	// Initialize 91C111
	//

	// Allocate driver's ring buffers
	smsc91c111_recv_desc_ring = malloc(sizeof(struct smsc91c111_descr) * NUM_RECV_BUFFERS);
	if (!smsc91c111_recv_desc_ring)
	{
		errno = ENOMEM;
		return	-1;
	}
	smsc91c111_xmit_desc_ring = malloc(sizeof(struct smsc91c111_descr) * NUM_XMIT_BUFFERS);
	if (!smsc91c111_xmit_desc_ring)
	{
		errno = ENOMEM;
		return	-1;
	}

#ifdef DEBUG_INIT
	serial_printf("%s(): Buffers allocated\n", __func__);
#ifdef	DEBUG_REGS
	dump_regs();
#endif	// DEBUG_REGS
#endif	// DEBUG_INIT

	// Set control options
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 0);	// Bank #0
	outw(SMSC91C111_REG_BASE + TRANSMIT_CTL, (XMIT_CTL_FDUPLX | XMIT_CTL_TXEN));	// Enable transmit, full duplex mode
	outw(SMSC91C111_REG_BASE + RECV_CTL, (RECV_CTL_RXEN | RECV_CTL_STRIP_CRC /*| RECV_CTL_PRMS*/));	// Enable receive, strip CRC from input frames; promiscous mode as temporary measure while we figure out why comparing addresses doesn't work
	outw(SMSC91C111_REG_BASE + RX_PHY_CTL, (RX_PHY_CTL_ANEG));	// Auto-negotiation mode (enable ANEG_EN in PHY control MII reg. 0 too)

#ifdef DEBUG_INIT
	serial_printf("%s(): bank #0 done\n", __func__);
#endif

	outw(SMSC91C111_REG_BASE + BANK_SELECT, 1);	// Bank #1
	memcpy(SMSC91C111_REG_BASE + INDIVID_ADDR, ETH_ADDR, 6);	// Set "individual" (ethernet) address
	outw(SMSC91C111_REG_BASE + CONTROL, (CONTROL_AUTO_RELEASE | CONTROL_LE_ENABLE | CONTROL_TE_ENABLE));	// Link error enable, transmit error enable (for interrupt), auto-release memory on Tx completion

#ifdef DEBUG_INIT
	serial_printf("%s(): bank #1 done\n", __func__);
#endif


	// TODO: set MII control reg (#0) - it invokes bit-banging of MII interface register

	// Set interrupt enable mask
	outw(SMSC91C111_REG_BASE + BANK_SELECT, 2);     // Bank #2
	outb(SMSC91C111_REG_BASE + INT_STATUS + 1, (INT_STATUS_MD_INT /*| INT_STATUS_TX_INT | INT_STATUS_TX_EMPTY_INT*/ | INT_STATUS_RX_INT));

	// Do we need to do anything else?

#ifdef	SEND_TEST_FRAME
	send_test_frame();
	send_test_frame();
	send_test_frame();
#endif

	serial_printf("%s(): init done\n", __func__);

	return	0;
}

int	smsc91c111_deinit(void)
{
	return	0;
}


int	smsc91c111_open(unsigned subdev_id)
{
	return	0;
}


int smsc91c111_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int smsc91c111_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int smsc91c111_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	return	0;
}


int smsc91c111_close(unsigned sub_id)
{
	return	0;
}

struct drv_entry	smsc91c111 = {smsc91c111_init, smsc91c111_deinit, smsc91c111_open, smsc91c111_read,
	smsc91c111_write, smsc91c111_ioctl, smsc91c111_close};

