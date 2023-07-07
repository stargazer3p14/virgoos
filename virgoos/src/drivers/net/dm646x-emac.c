/***************************************************
 *
 * 	dm646x-emac.c
 *
 *	DM646x EMAC driver	
 *
 ***************************************************/

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "pcihost.h"
#include "errno.h"
#include "dm646x-emac.h"
#include "inet.h"

//#define	DEBUG_IRQ	1
//#define	DEBUG_INIT	1
//#define	DEBUG_TEST	1
//#define	DEBUG_SEND	1
//#define	SEND_TEST_FRAME	1
//#define	DEBUG_RINGS
//#define	DEBUG_REGS


#define	ETH_PAD_SHORT_FRAMES	1

#define	ETH_ADDR	"\x10\x22\x33\x07\x08\x11"

extern	struct net_if	net_interfaces[MAX_NET_INTERFACES];

static void	dm646x_emac_get_send_packet(unsigned char **payload);
/*static*/ int	dm646x_emac_send_packet(unsigned char *dest_addr, word protocol, unsigned size);


struct eth_device	dm646x_emac_eth_device =
{
	ETH_DEV_TYPE_NIC,
	ETH_ADDR,
	dm646x_emac_get_send_packet,
	dm646x_emac_send_packet
};

dword	base_port;				/* For I/O access */
dword	base_addr;				/* For memory access */

dword	frames_count;

#ifdef	SEND_TEST_FRAME
static	void	send_test_frame(void);
static int	fill_test_data = 1;
#endif

static void	dm646x_emac_isr_bh(void);


// Receive and transmit descriptors ring
struct dm646x_emac_descr	*dm646x_emac_recv_desc_ring;
struct dm646x_emac_descr	*dm646x_emac_xmit_desc_ring;

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
		unsigned char	*pkt_data = (unsigned char*)(*(dword*)&dm646x_emac_recv_desc_ring[i].buf);

//		if (i != curr_rd)
//			continue;
		serial_printf("===> curr_rd = %d\n", curr_rd);
		
		// Print suitable for both 16-bit and 32-bit modes
		serial_printf("rd=%d next=%08X buf=%08X buf_len=%04X buf_offs=%04X pkt_len=%04X flags=%04X\n", i,
			(unsigned)dm646x_emac_recv_desc_ring[i].next,
			(unsigned)dm646x_emac_recv_desc_ring[i].buf,
			(unsigned)dm646x_emac_recv_desc_ring[i].buf_len,
			(unsigned)dm646x_emac_recv_desc_ring[i].buf_offs,
			(unsigned)dm646x_emac_recv_desc_ring[i].pkt_len,
			(unsigned)dm646x_emac_recv_desc_ring[i].flags);
		serial_printf("Received packet:\r\n");
		serial_printf("\t");
		pkt_len = dm646x_emac_recv_desc_ring[i].pkt_len;

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
		unsigned char	*pkt_data = (unsigned char*)(*(dword*)&dm646x_emac_xmit_desc_ring[i].buf);

//		if (i != curr_td)
//			continue;
		serial_printf("===> curr_td = %d\n", curr_td);
		
		// Print suitable for both 16-bit and 32-bit modes
		serial_printf("td=%d next=%08X buf=%08X buf_len=%04X buf_offs=%04X pkt_len=%04X flags=%04X\n", i,
			(unsigned)dm646x_emac_xmit_desc_ring[i].next,
			(unsigned)dm646x_emac_xmit_desc_ring[i].buf,
			(unsigned)dm646x_emac_xmit_desc_ring[i].buf_len,
			(unsigned)dm646x_emac_xmit_desc_ring[i].buf_offs,
			(unsigned)dm646x_emac_xmit_desc_ring[i].pkt_len,
			(unsigned)dm646x_emac_xmit_desc_ring[i].flags);
		serial_printf("Transmitted packet:\r\n");
		serial_printf("\t");
		pkt_len = dm646x_emac_xmit_desc_ring[i].pkt_len;

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
		serial_printf("\r\n");
	}
	serial_printf("\n"
		"============================================================\n");
}
#endif	// DEBUG_RINGS

#ifdef	DEBUG_REGS
void	dump_regs(void)
{
	// Dump all configured EMAC / EMAC ctl. registers
	serial_printf("%s(): ----------------------- Dump EMAC registers -------------------------------\n", __func__);
	serial_printf("EMAC regs: EMACTXIDVER=%08X EMACTXCONTROL=%08X EMACTXTEARDOWN=%08X EMACRXIDVER=%08X EMACRXCONTROL=%08X EMACRXTEARDOWN=%08X EMACTXINTSTATRAW=%08X EMACTXINTSTATMASKED=%08X EMACTXINTMASKSET=%08X EMACTXINTMASKCLEAR=%08X EMACINTVECTOR=%08X EMACEOIVECTOR=%08X\n",
		ind(DM646x_EMAC_BASE+EMACTXIDVER), ind(DM646x_EMAC_BASE+EMACTXCONTROL), ind(DM646x_EMAC_BASE+EMACTXTEARDOWN), ind(DM646x_EMAC_BASE+EMACRXIDVER), ind(DM646x_EMAC_BASE+EMACRXCONTROL), ind(DM646x_EMAC_BASE+EMACRXTEARDOWN), ind(DM646x_EMAC_BASE+EMACTXINTSTATRAW), ind(DM646x_EMAC_BASE+EMACTXINTSTATMASKED), ind(DM646x_EMAC_BASE+EMACTXINTMASKSET),
		ind(DM646x_EMAC_BASE+EMACTXINTMASKCLEAR), ind(DM646x_EMAC_BASE+EMACINTVECTOR), ind(DM646x_EMAC_BASE+EMACEOIVECTOR));
	serial_printf("EMACRXINTSTATRAW=%08X EMACRXINTSTATMASKED=%08X EMACRXINTMASKSET=%08X EMACRXINTMASKCLEAR=%08X EMACINTSTATRAW=%08X EMACINTSTATMASKED=%08X EMACINTMASKSET=%08X EMACINTMASKCLEAR=%08X\n",
		ind(DM646x_EMAC_BASE+EMACRXINTSTATRAW), ind(DM646x_EMAC_BASE+EMACRXINTSTATMASKED), ind(DM646x_EMAC_BASE+EMACRXINTMASKSET), ind(DM646x_EMAC_BASE+EMACRXINTMASKCLEAR), ind(DM646x_EMAC_BASE+EMACINTSTATRAW),
		ind(DM646x_EMAC_BASE+EMACINTSTATMASKED), ind(DM646x_EMAC_BASE+EMACINTMASKSET), ind(DM646x_EMAC_BASE+EMACINTMASKCLEAR));
	serial_printf("EMACRXMBPENABLE=%08X EMACUNICASTSET=%08X EMACUNICASTCLEAR=%08X EMACRXMAXLEN=%08X EMACRXBUFFEROFFSET=%08X EMACRX0FLOWTHRESH=%08X EMACRX0FREEBUFFER=%08X EMACCONTROL=%08X\n",
		ind(DM646x_EMAC_BASE+EMACRXMBPENABLE), ind(DM646x_EMAC_BASE+EMACUNICASTSET), ind(DM646x_EMAC_BASE+EMACUNICASTCLEAR), ind(DM646x_EMAC_BASE+EMACRXMAXLEN), ind(DM646x_EMAC_BASE+EMACRXBUFFEROFFSET),
		ind(DM646x_EMAC_BASE+EMACRXFILTERLOWTHRESH), ind(DM646x_EMAC_BASE+EMACRX0FREEBUFFER), ind(DM646x_EMAC_BASE+EMACCONTROL));
	serial_printf("EMACSTATUS=%08X EMACEMCONTROL=%08X EMACFIFOCONTROL=%08X EMACCONFIG=%08X EMACSOFTRESET=%08X EMACSRCADDRLO=%08X EMACSRCADDRHI=%08X EMACHASH1=%08X EMACHASH2=%08X EMACBOFFTEST=%08X EMACTPACETEST=%08X\n",
		ind(DM646x_EMAC_BASE+EMACSTATUS), ind(DM646x_EMAC_BASE+EMACEMCONTROL), ind(DM646x_EMAC_BASE+EMACFIFOCONTROL), ind(DM646x_EMAC_BASE+EMACCONFIG), ind(DM646x_EMAC_BASE+EMACSOFTRESET), ind(DM646x_EMAC_BASE+EMACSRCADDRLO),
		ind(DM646x_EMAC_BASE+EMACSRCADDRHI), ind(DM646x_EMAC_BASE+EMACHASH1), ind(DM646x_EMAC_BASE+EMACHASH2), ind(DM646x_EMAC_BASE+EMACBOFFTEST), ind(DM646x_EMAC_BASE+EMACTPACETEST));
	serial_printf("EMACRXPAUSE=%08X EMACTXPAUSE=%08X EMACADDRLO=%08X EMACADDRHI=%08X EMACINDEX=%08X EMACTX0HDP=%08X EMACRX0HDP=%08X EMACTX0CP=%08X EMACRX0CP=%08X\n",
		ind(DM646x_EMAC_BASE+EMACRXPAUSE), ind(DM646x_EMAC_BASE+EMACTXPAUSE), ind(DM646x_EMAC_BASE+EMACADDRLO), ind(DM646x_EMAC_BASE+EMACADDRHI), ind(DM646x_EMAC_BASE+EMACINDEX), ind(DM646x_EMAC_BASE+EMACTX0HDP), ind(DM646x_EMAC_BASE+EMACRX0HDP),
		ind(DM646x_EMAC_BASE+EMACTX0CP), ind(DM646x_EMAC_BASE+EMACRX0CP));
	serial_printf("\n");
	serial_printf("EMAC stats: EMACTXGOODFRAMES=%08X EMACTXBCASTFRAMES=%08X EMACTXMCASTFRAMES=%08X EMACTXPAUSEFRAMES=%08X EMACTXDEFERRED=%08X EMACTXCOLLISION=%08X EMACTXSINGLECOLL=%08X EMACTXMULTICOLL=%08X EMACTXEXCESSIVECOLL=%08X\n",
		ind(DM646x_EMAC_BASE+EMACTXGOODFRAMES), ind(DM646x_EMAC_BASE+EMACTXBCASTFRAMES), ind(DM646x_EMAC_BASE+EMACTXMCASTFRAMES), ind(DM646x_EMAC_BASE+EMACTXPAUSEFRAMES), ind(DM646x_EMAC_BASE+EMACTXDEFERRED), ind(DM646x_EMAC_BASE+EMACTXCOLLISION),
		ind(DM646x_EMAC_BASE+EMACTXSINGLECOLL), ind(DM646x_EMAC_BASE+EMACTXMULTICOLL), ind(DM646x_EMAC_BASE+EMACTXEXCESSIVECOLL));
	serial_printf("EMACTXLATECOLL=%08X EMACTXUNDERRUN=%08X EMACTXCARRIERSENSE=%08X EMACTXOCTETS=%08X EMACFRAME64=%08X EMACFRAME65T127=%08X EMACFRAME128T255=%08X EMACFRAME256T511=%08X EMACFRAME512T1023=%08X EMACFRAME1024TUP=%08X EMACNETOCTETS=%08X\n",
		ind(DM646x_EMAC_BASE+EMACTXLATECOLL), ind(DM646x_EMAC_BASE+EMACTXUNDERRUN), ind(DM646x_EMAC_BASE+EMACTXCARRIERSENSE), ind(DM646x_EMAC_BASE+EMACTXOCTETS), ind(DM646x_EMAC_BASE+EMACFRAME64), ind(DM646x_EMAC_BASE+EMACFRAME65T127),
		ind(DM646x_EMAC_BASE+EMACFRAME128T255), ind(DM646x_EMAC_BASE+EMACFRAME256T511), ind(DM646x_EMAC_BASE+EMACFRAME512T1023), ind(DM646x_EMAC_BASE+EMACFRAME1024TUP), ind(DM646x_EMAC_BASE+EMACNETOCTETS));
	serial_printf("\n");
	serial_printf("EMAC Control Module regs: CMIDVER=%08X CMSOFTRESET=%08X CMEMCONTROL=%08X CMINTCTRL=%08X CMRXTHRESHINTEN=%08X CMRXINTEN=%08X CMTXINTEN=%08X CMMISCINTEN=%08X CMRXTHRESHINTSTAT=%08X\n",
		ind(DM646x_EMAC_CTRL_BASE+CMIDVER), ind(DM646x_EMAC_CTRL_BASE+CMSOFTRESET), ind(DM646x_EMAC_CTRL_BASE+CMEMCONTROL), ind(DM646x_EMAC_CTRL_BASE+CMINTCTRL), ind(DM646x_EMAC_CTRL_BASE+CMRXTHRESHINTEN),
		ind(DM646x_EMAC_CTRL_BASE+CMRXINTEN), ind(DM646x_EMAC_CTRL_BASE+CMTXINTEN), ind(DM646x_EMAC_CTRL_BASE+CMMISCINTEN), ind(DM646x_EMAC_CTRL_BASE+CMRXTHRESHINTSTAT));
	serial_printf("CMRXINTSTAT=%08X CMTXINTSTAT=%08X CMMISCINTSTAT=%08X\n", ind(DM646x_EMAC_CTRL_BASE+CMRXINTSTAT), ind(DM646x_EMAC_CTRL_BASE+CMTXINTSTAT), ind(DM646x_EMAC_CTRL_BASE+CMMISCINTSTAT));
	serial_printf("\n");
	serial_printf("MDIO Module regs: MDIOVERSION=%08X MDIOCONTROL=%08X MDIOALIVE=%08X MDIOLINK=%08X MDIOLINKINTRAW=%08X MDIOLINKINTMASKED=%08X MDIOUSERINTRAW=%08X MDIOUSERINTMASKED=%08X\n",
		ind(DM646x_MDIO_BASE+MDIOVERSION), ind(DM646x_MDIO_BASE+MDIOCONTROL), ind(DM646x_MDIO_BASE+MDIOALIVE), ind(DM646x_MDIO_BASE+MDIOLINK), ind(DM646x_MDIO_BASE+MDIOLINKINTRAW), ind(DM646x_MDIO_BASE+MDIOLINKINTMASKED),
		ind(DM646x_MDIO_BASE+MDIOUSERINTRAW), ind(DM646x_MDIO_BASE+MDIOUSERINTMASKED));
	serial_printf("MDIOUSERINTMASKSET=%08X MDIOUSERINTMASKCLEAR=%08X MDIOUSERACCESS0=%08X MDIOUSERPHYSEL0=%08X MDIOUSERACCESS1=%08X MDIOUSERPHYSEL1=%08X\n",
		ind(DM646x_MDIO_BASE+MDIOUSERINTMASKSET), ind(DM646x_MDIO_BASE+MDIOUSERINTMASKCLEAR), ind(DM646x_MDIO_BASE+MDIOUSERACCESS0),
		ind(DM646x_MDIO_BASE+MDIOUSERPHYSEL0), ind(DM646x_MDIO_BASE+MDIOUSERACCESS1), ind(DM646x_MDIO_BASE+MDIOUSERPHYSEL1));
	serial_printf("%s(): --------------------- End Dump EMAC registers -----------------------------\n", __func__);

}
#endif

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
static void	dm646x_emac_isr_bh(void)
{
}

static void    dm646x_emac_get_send_packet(unsigned char **payload)
{
	dword	addr;

	if (dm646x_emac_xmit_desc_ring[curr_td].flags & OWNER /* OWN = 1? */)
	{
		serial_printf("%s(): curr_td=%d is owned by EMAC, returning NULL\n", __func__, curr_td);
		*payload = NULL;
		return;
	}

	addr = *(dword*)&dm646x_emac_xmit_desc_ring[curr_td].buf + sizeof(struct eth_frame_hdr);
	*payload = (char*)addr;
}

/*
 *	Interface function that sends a packet
 */
/*static*/ int	dm646x_emac_send_packet(unsigned char *dest_addr, word protocol, unsigned size)
{
	dword	addr;
	struct eth_frame_hdr	*eth_hdr;
	int	i;

#ifdef	DEBUG_SEND
	serial_printf("%s(): entered, dm646x_emac_xmit_desc_ring=%08X curr_td=%d\n", __func__, dm646x_emac_xmit_desc_ring, curr_td);
#endif

	if (dm646x_emac_xmit_desc_ring[curr_td].flags & OWNER)
	{
		serial_printf("curr_td (%d) is owned by EMAC, weird error\n", curr_td);
		return	-1;
	}

#if DEBUG_SEND
	serial_printf("%s(): Submitting frame to send, curr_td = %d size=%u\n", __func__, curr_td, size);
#endif
	addr = *(dword*)&dm646x_emac_xmit_desc_ring[curr_td].buf;
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

	dm646x_emac_xmit_desc_ring[curr_td].pkt_len = size & 0xFFFF;
	dm646x_emac_xmit_desc_ring[curr_td].flags |= OWNER | SOP | EOP;

	// A try
	dm646x_emac_xmit_desc_ring[curr_td].next = 0;
	if (prev_td != curr_td)
		dm646x_emac_xmit_desc_ring[prev_td].next = (uint32_t)(dm646x_emac_xmit_desc_ring + curr_td);
#if DEBUG_SEND
	dump_xmit_ring();
#endif
#ifdef	DEBUG_REGS
	serial_printf("1111111111111111111111111111111111\n");
	dump_regs();
#endif
#if DEBUG_SEND
	serial_printf("dm646x_emac_xmit_desc_ring=%08X dm646x_emac_xmit_desc_ring + curr_td=%08X\n", (dword)dm646x_emac_xmit_desc_ring, (dword)(dm646x_emac_xmit_desc_ring + curr_td));
#endif
	outd(DM646x_EMAC_BASE + EMACTX0HDP, (dword)(dm646x_emac_xmit_desc_ring + curr_td));

	// DM6467's EMAC treats a descriptor inside the active list (between EMACTX0HDP and 0) without OWNER bit set as HOST ERROR.
	// This behavior is not clearly documented. It is only said that active descriptors must be submitted so that the queue head is loaded into EMACTX0HDP and tail has its next pointer == 0.
	// Now we have 2 problems: 1) sent frames don't get sent on wire (reception works OK) and 2) Link up/down indication doesn't generate an interrupt, although it was programmed so.
	//
	//dm646x_emac_xmit_desc_ring[prev_td].flags &= ~OWNER;
#ifdef	DEBUG_REGS
	serial_printf("2222222222222222222222222222222222\n");
	dump_regs();
#endif

	prev_td = curr_td;
	curr_td = (curr_td + 1) % NUM_XMIT_BUFFERS;
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
 *	DM646x EMAC Rx IRQ handler.
 */
static	int	dm646x_emac_rx_isr(void)
{
#ifdef	DEBUG_IRQ
	static int	rx_irq_count;
#endif
	struct eth_frame_hdr	*pdata;
	dword	intstat;

#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received DM646x_EMAC Rx IRQ no. %d\r\n", __func__, ++rx_irq_count);
#endif

	// If channel 0 Rx interrupt is not pending, there's some error in configuration. Don't treat it as receive
	intstat = ind(DM646x_EMAC_BASE + EMACRXINTSTATMASKED);
	if (!(intstat & 0x1))
	{
		serial_printf("%s(): error, channel 0 rx interrupt is not pending\n", __func__);
		goto	ack_int;	
	}

	pdata = (struct eth_frame_hdr*)*(dword*)&dm646x_emac_recv_desc_ring[curr_rd].buf;

#if DEBUG_IRQ
 #ifdef DEBUG_RINGS
	serial_printf("%s(): pdata=%08X dest_addr=%02X%02X%02X%02X%02X%02X src_addr=%02X%02X%02X%02X%02X%02X frame_size=%04X\r\n", __func__, pdata,
		(unsigned)pdata->dest_addr[0], (unsigned)pdata->dest_addr[1], (unsigned)pdata->dest_addr[2],
		(unsigned)pdata->dest_addr[3], (unsigned)pdata->dest_addr[4], (unsigned)pdata->dest_addr[5],
		(unsigned)pdata->src_addr[0], (unsigned)pdata->src_addr[1], (unsigned)pdata->src_addr[2],
		(unsigned)pdata->src_addr[3], (unsigned)pdata->src_addr[4], (unsigned)pdata->src_addr[5],
		(unsigned)pdata->frame_size);

		// DEBUGDEBUGDEBUG -- dump all receive ring
		dump_recv_ring();
 #endif
#endif
	// Receive frames in loop, because we can lose frame interrupts (get 1 IRQ for several frames)
	do
	{
		// Necessary in promiscous mode - check if the frame is ours (or broadcast)
		if (!memcmp(pdata->dest_addr, ETH_ADDR_BROADCAST, 6) || !memcmp(pdata->dest_addr, ETH_ADDR, 6))
			// Parse ethernet frame
			eth_parse_packet(&net_interfaces[0], pdata);            // TODO: invent something more configurable then hardcoded "0" index

		// Release the buffer to NIC
		dm646x_emac_recv_desc_ring[curr_rd].flags |= OWNER;

			// Advance "current receive descriptor" index
		curr_rd = (curr_rd + 1) % NUM_RECV_BUFFERS;

	} while (!(dm646x_emac_recv_desc_ring[curr_rd].flags & OWNER));

ack_int:
	// Acknowledge interrupt by writing back value found in RX0CP, it gets back into DMA state RAM
	outd(DM646x_EMAC_BASE + EMACRX0CP, ind(DM646x_EMAC_BASE + EMACRX0CP));
	// Send EOI
	outd(DM646x_EMAC_BASE + EMACEOIVECTOR, EOI_RXPULSE);

	// this interrupt is not shared
	return	1;
}


/*
 *	DM646x EMAC Rx IRQ handler.
 */
static	int	dm646x_emac_tx_isr(void)
{
#ifdef	DEBUG_IRQ
	static int	tx_irq_count;
#endif
	struct eth_frame_hdr	*pdata;
	dword	intstat;	

#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received DM646x_EMAC Tx IRQ no. %d\r\n", __func__, ++tx_irq_count);
#endif

	// If channel 0 Rx interrupt is not pending, there's some error in configuration. Don't treat it as receive
	intstat = ind(DM646x_EMAC_BASE + EMACTXINTSTATMASKED);
	if (!(intstat & 0x1))
	{
		serial_printf("%s(): error, channel 0 tx interrupt is not pending\n", __func__);
		goto	ack_int;
	}
ack_int:
	// Acknowledge interrupt by writing back value found in TX0CP, it gets back into DMA state RAM
	outd(DM646x_EMAC_BASE + EMACTX0CP, ind(DM646x_EMAC_BASE + EMACTX0CP));
	// Send EOI
	outd(DM646x_EMAC_BASE + EMACEOIVECTOR, EOI_TXPULSE);

	// this interrupt is not shared
	return	1;
}

/*
 *	DM646x EMAC MISC IRQ handler.
 */
static	int	dm646x_emac_misc_isr(void)
{
//#ifdef	DEBUG_IRQ
	static int	misc_irq_count;
//#endif
	int	boguscnt = 2;			// 2 is default for pcnet32 driver, on lance it was 10
	struct eth_frame_hdr	*pdata;
	dword	intstat;	

//#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received DM646x_EMAC Misc IRQ no. %d\r\n", __func__, ++misc_irq_count);
//#endif

	intstat	= ind(DM646x_EMAC_CTRL_BASE + CMMISCINTSTAT);

	// MISC interrupts are determined and acknowledged separately
	if (intstat & USERINT)
	{
		serial_printf("%s(): MDIO User interrupt\n", __func__);
		// Clear link interrupt
		outd(DM646x_MDIO_BASE + MDIOUSERINTMASKED, ind(DM646x_MDIO_BASE + MDIOUSERINTMASKED));
	}
	if (intstat & LINKINT)
	{
		dword	link;
		dword	link_int_masked;
		dword	phy_num;

		// We monitor only first PHY detected during init, actually. If there's something else, we are misconfigured
		phy_num = ind(DM646x_MDIO_BASE + MDIOUSERPHYSEL0) & 0x1F;

		serial_printf("%s(): MDIO Link interrupt\n", __func__);
		link_int_masked = ind(DM646x_MDIO_BASE + MDIOLINKINTMASKED);
		serial_printf("%s(): link_int_masked = %08X\n", __func__, link_int_masked);
		if (link_int_masked & 0x1)
			serial_printf("PHY %u reports link change\n", phy_num);
		else
			serial_printf("PHY %u doesn't reports link change - misconfiguration?\n", phy_num);
			
		link = ind(DM646x_MDIO_BASE + MDIOLINK);
		serial_printf("link = %08X (PHY %d is %s)\n", link, phy_num, (link & 1 << phy_num) ? "up" : "down");
		// Clear link interrupt
		outd(DM646x_MDIO_BASE + MDIOLINKINTMASKED, ind(DM646x_MDIO_BASE + MDIOLINKINTMASKED));
	}
	if (intstat & HOSTINT)
	{
		serial_printf("%s(): Host error interrupt\n", __func__);
		// Host error interrupt can't be acknowledged, only h/w reset helps (PSC)
	}
	if (intstat & STATINT)
	{
		serial_printf("%s(): Stats interrupt\n", __func__);
		// Stats interrupt is cleared by decrementing all outstanding (> 0x80000000) stats values
	}

	// Send EOI
	outd(DM646x_EMAC_BASE + EMACEOIVECTOR, EOI_MISC);

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

	dm646x_emac_get_send_packet(&transmit_data);

	/* Fill packet with test data */
	for (i = 0; i < 1500; ++i)
		transmit_data[i] = i*2+1 & 0xFF;

	dm646x_emac_send_packet(ETH_ADDR_BROADCAST, htons(0x0801), 1400);
}
#endif


/*
 *	Initialize the ethernet controller (according to Software Developer's Manual's
 *	recommendation)
 */
int	dm646x_emac_init(unsigned drv_id)
{
	dword	value = -1;
	word	eeprom_val;
	int	i;
	int	int_line;
	struct dm646x_emac_descr	*rx_descr, *tx_descr;
	uint32_t	macaddr;
	int	first_phy;

	// Set interrupt handlers
	set_int_callback(EMAC_RX_IRQ, dm646x_emac_rx_isr);
	set_int_callback(EMAC_TX_IRQ, dm646x_emac_tx_isr);
	set_int_callback(EMAC_MISC_IRQ, dm646x_emac_misc_isr);
	serial_printf("%s(): set ints: Rx = %d Tx = %d Misc = %d\n", __func__, EMAC_RX_IRQ, EMAC_TX_IRQ, EMAC_MISC_IRQ);

	// Initialize EMAC

#if 1 
	// Step 0 - reset EMAC and take it out of reset to bring it to just-after-reset state
	evmdm6467_reset_module(PSC_MOD_EMAC_MDIO);
	evmdm6467_wake_module(PSC_MOD_EMAC_MDIO);
	serial_printf("EMAC module is reset\n");
#endif

#ifdef DEBUG_INIT
	serial_printf("EMAC regs BEFORE INIT>>>>>>>>>>>>>>>>>>\n");

#ifdef	DEBUG_REGS
	dump_regs();
#endif	// DEBUG_REGS
#endif	// DEBUG_INIT

	// Step 1 - clear bits 20, 21 in VDD3P3V_PWDN register in System Control Module in order to enable power on MII and GMII connections (they are separate) [currently we let u-boot do this]
	outd(SYS_MODULE_BASE + SYSVDD3P3V_PWDN, ind(SYS_MODULE_BASE + SYSVDD3P3V_PWDN) & ~0x00300000);

	// Step 2 - clear EMAC interrupts for the initialization
	outd(DM646x_EMAC_CTRL_BASE + CMRXTHRESHINTEN, 0);
	outd(DM646x_EMAC_CTRL_BASE + CMRXINTEN, 0);
	outd(DM646x_EMAC_CTRL_BASE + CMTXINTEN, 0);
	outd(DM646x_EMAC_CTRL_BASE + CMMISCINTEN, 0);

	// Step 3 - clear MAC control, receive control and transmit control registers
	outd(DM646x_EMAC_BASE + EMACCONTROL, 0);
	outd(DM646x_EMAC_BASE + EMACTXCONTROL, 0);
	outd(DM646x_EMAC_BASE + EMACRXCONTROL, 0);

	// Step 4 - initialize all 16 header descriptor pointers to 0
	for (i = 0; i < DM646x_EMAC_NUM_CHANNELS; ++i)
	{
		outd(DM646x_EMAC_BASE + EMACTX0HDP + i*4, 0);
		outd(DM646x_EMAC_BASE + EMACRX0HDP + i*4, 0);
	}

	// Step 5 - clear all 36 statistic counters to 0
	for (i = 0; i < 36; ++i)
	{
		outd(DM646x_EMAC_BASE + EMACRXGOODFRAMES + i*4, 0);
	}
	
	// Step 6 - initialize receive match addresses
	for (i = 0; i < DM646x_EMAC_NUM_CHANNELS; ++i)
	{

		outd(DM646x_EMAC_BASE + EMACINDEX, i);			// Write index reg
		// Only index 0 address is valid.
		// Change this initialization if enabling more than one "virtual MAC" via channels
		macaddr = (uint32_t)dm646x_emac_eth_device.addr[2] << 24 | (uint32_t)dm646x_emac_eth_device.addr[3] << 16 | (uint32_t)dm646x_emac_eth_device.addr[4] << 8 | (uint32_t)dm646x_emac_eth_device.addr[5];	// Write addresss HIGH first
		outd(DM646x_EMAC_BASE + EMACADDRHI, macaddr);
		macaddr = i << 16 /* channel # */| (uint32_t)dm646x_emac_eth_device.addr[0] << 8 | (uint32_t)dm646x_emac_eth_device.addr[1];
		if (i == 0)
			macaddr |= (MACADDR_VALID | MACADDR_FILT);
		outd(DM646x_EMAC_BASE + EMACADDRLO, macaddr);		// Write address LOW
	}

	// Step 7 - initialize buffer flow control is meanwhile skipped

	// Step 8 - clear MAC address has registers - no multicast at initialization
	outd(DM646x_EMAC_BASE + EMACHASH1, 0);
	outd(DM646x_EMAC_BASE + EMACHASH2, 0);
	
	// Step 9 - clear receive buffer offset register
	outd(DM646x_EMAC_BASE + EMACRXBUFFEROFFSET, 0);

	// Step 10 - enable unicast reception for channel 0, disable for other channels (change if working with multiple channels)
	outd(DM646x_EMAC_BASE + EMACUNICASTCLEAR, 0xFF);
	outd(DM646x_EMAC_BASE + EMACUNICASTSET, 0x1);

	// Step 11 - set promiscous/multicast/broadcast configuration
	// Our configuration: broadcast enabled, copied to channel 0; short frames enabled; packets don't chain multiple buffers;
	// promiscous mode disabled; multicast disabled
	outd(DM646x_EMAC_BASE + EMACRXMBPENABLE, RXBROADEN | RXCSFEN | RXNOCHAIN | RXCAFEN /*promiscous mode*/);

	// Step 12 - set up MAC control (for now, set up only 10/100mbs mode - we'll reconsider to set this to 1 Gbs)
	outd(DM646x_EMAC_BASE + EMACCONTROL, FULLDUPLEX | RXOWNERSHIP | RXOFFLENBLOCK);

	// Step 13 - clear interrupts for unused channels
	outd(DM646x_EMAC_BASE + EMACRXINTMASKCLEAR, 0xFF);
	outd(DM646x_EMAC_BASE + EMACTXINTMASKCLEAR, 0xFF);

	// Step 14 - enable relevant interrupts
	outd(DM646x_EMAC_BASE + EMACRXINTMASKSET, 0x1);
	outd(DM646x_EMAC_BASE + EMACTXINTMASKSET, 0x1);
	outd(DM646x_EMAC_BASE + EMACINTMASKSET, STATMASK | HOSTMASK);

	// Step 15 - prepare receive and transmit descriptor queues
	// For now we use unconditionally internal RAM (and are therefore limited to 512 total descriptors). In future we will extend to unlimited number of
	// descriptors, but they must be placed in external RAM (slower).
#ifdef USE_EMAC_RAM 
	dm646x_emac_recv_desc_ring = (struct dm646x_emac_descr*)DM646x_RAM_BASE;
	rx_descr = dm646x_emac_recv_desc_ring;
	for (i = 0; i < NUM_RECV_BUFFERS; ++i, ++rx_descr)
	{
		int	j;

		rx_descr->next = (uint32_t)(rx_descr + 1);
		rx_descr->buf = (uint32_t)malloc(RECV_BUFFER_SIZE);
		if (rx_descr->buf == 0)
		{
			for (j = 0; j < i; ++j)
				free((void*)((struct dm646x_emac_descr*)DM646x_RAM_BASE)->buf);
			goto	abort;
		}
		rx_descr->buf_len = RECV_BUFFER_SIZE;
		rx_descr->buf_offs = 0;
		rx_descr->pkt_len = 0;
		rx_descr->flags = OWNER;	// Rx queue - owner is EMAC
	}
	--rx_descr;
	// Countrary to EMAC docs recommendation, we set cyclic queue, instead of 0-terminated, and hope to serve every interrupt (or one in a couple).
	// If this strategy fails, we will reconsider using NULL pointers at the end (how to use cyclic buffers then?)
	rx_descr->next = DM646x_RAM_BASE;
	//rx_descr->next = 0;

	//dm646x_emac_xmit_desc_ring = (struct dm646x_emac_descr*)DM646x_RAM_BASE + NUM_RECV_BUFFERS;
	dm646x_emac_xmit_desc_ring = (struct dm646x_emac_descr*)(DM646x_RAM_BASE + 0x1000);
	tx_descr = dm646x_emac_xmit_desc_ring;
	for (i = 0; i < NUM_XMIT_BUFFERS; ++i, ++tx_descr)
	{
		int	j;

		tx_descr->next = (dword)(tx_descr + 1);
		tx_descr->buf = (dword)malloc(XMIT_BUFFER_SIZE);
		if (tx_descr->buf == 0)
		{
			for (j = 0; j < i; ++j)
				free((void*)((struct dm646x_emac_descr*)(DM646x_RAM_BASE + 0x1000))->buf);
			goto	abort;
		}
		tx_descr->buf_len = XMIT_BUFFER_SIZE;
		tx_descr->buf_offs = 0;
		tx_descr->pkt_len = 0;
		tx_descr->flags = SOP | EOP;		// Tx queue - owner is CPU
	}
	--tx_descr;
	tx_descr->next = (dword)((struct dm646x_emac_descr*)(DM646x_RAM_BASE + 0x1000));		// Set Tx last's next pointer to first
	//tx_descr->next = (dword)((struct dm646x_emac_descr*)DM646x_RAM_BASE + NUM_RECV_BUFFERS);		// Set Tx last's next pointer to first
	//tx_descr->next = 0;
#endif

	// Step 16 - set up RX0HDP (prepare for receive) and TX0HDP (prepare for transmit)
	outd(DM646x_EMAC_BASE + EMACRX0HDP, (dword)dm646x_emac_recv_desc_ring);
	// EMACTX0HDP will be set when actually starting to send frames. Everything between Tx queue head and NULL must have OWNER bit set (owned by EMAC)

	// Step 17 - enable receive and transmit DMA controllers and then enable GMII
	outd(DM646x_EMAC_BASE + EMACRXCONTROL, RXEN);
	outd(DM646x_EMAC_BASE + EMACTXCONTROL, TXEN);

	// In order to enable transmission, we need to set up MACSRCADDRLO/MACSRCADDRHI	 (we follow high-first write, but here it doesn't really matter
	macaddr = (uint32_t)dm646x_emac_eth_device.addr[2] << 24 | (uint32_t)dm646x_emac_eth_device.addr[3] << 16 | (uint32_t)dm646x_emac_eth_device.addr[4] << 8 | (uint32_t)dm646x_emac_eth_device.addr[5];	// Write addresss HIGH first
	outd(DM646x_EMAC_BASE + EMACSRCADDRHI, macaddr);
	macaddr = (uint32_t)dm646x_emac_eth_device.addr[0] << 8 | (uint32_t)dm646x_emac_eth_device.addr[1];
	outd(DM646x_EMAC_BASE + EMACSRCADDRLO, macaddr);		// Write address LOW

	// Enable GMII
	outd(DM646x_EMAC_BASE + EMACCONTROL, ind(DM646x_EMAC_BASE + EMACCONTROL) | GMIIEN);

	// Enable and set up MDIO (enable state machine, enable fault detection, preserve clkdiv value)
	outd(DM646x_MDIO_BASE + MDIOCONTROL, ENABLE | FAULTENB | ind(DM646x_MDIO_BASE + MDIOCONTROL) & 0xFFFF);

	while (ind(DM646x_MDIO_BASE + MDIOCONTROL) & IDLE)
		;
	udelay(100000);

	// Find first PHY by polling MDIO ALIVE register
	value = ind(DM646x_MDIO_BASE + MDIOALIVE);
	for (first_phy = 0; first_phy < MAX_PHY; ++first_phy)
		if (value & 1 << first_phy)
			break;

	if (first_phy == MAX_PHY)
		serial_printf("%s(): no PHYs detected alive\n", __func__);
	else
		serial_printf("%s(): first PHY detected alive is %d\n", __func__, first_phy);

	// Additional step - set up MDIO interrupts
	outd(DM646x_MDIO_BASE + MDIOUSERPHYSEL0, first_phy | LINKINTENB);	// Monitor PHY 'first_phy', link change int. enabled, link status determined by MDIO state machine
										// (!) This register may be used as current PHY number indicator for our driver, as EMAC allows at most 1 PHY to be connected at a time
	outd(DM646x_MDIO_BASE + MDIOUSERPHYSEL1, 0);	// USELPHYSEL1 doesn't monitor anything

	// Step 18 - enable interrupts
	outd(DM646x_EMAC_CTRL_BASE + CMRXTHRESHINTEN, 0);		// threshold interrupt is disabled
	outd(DM646x_EMAC_CTRL_BASE + CMRXINTEN, 1);			// Rx interrupt is enabled for channel 0
	outd(DM646x_EMAC_CTRL_BASE + CMTXINTEN, 1);			// Tx interrupt is enabled for channel 0
	outd(DM646x_EMAC_CTRL_BASE + CMMISCINTEN, USERINT | LINKINT | HOSTINT /*| STATINT*/);		// MISC interrupt - currently we don't want stats

#ifdef DEBUG_INIT
	serial_printf("EMAC regs AFTER INIT>>>>>>>>>>>>>>>>>>\n");
	// Dump all configured EMAC / EMAC ctl. registers
#ifdef	DEBUG_REGS
	dump_regs();
#endif	// DEBUG_REGS
#endif	// DEBUG_INIT

#ifdef DEBUG_RINGS
	dump_recv_ring();
	dump_xmit_ring();
#endif
	
	serial_printf("%s(): init done\n", __func__);

serial_printf("%s(): PARANOID -- dm646x_emac_send_packet=%08X (->%08X)\n", __func__, dm646x_emac_send_packet, dm646x_emac_eth_device.send_packet);

#ifdef	SEND_TEST_FRAME
	send_test_frame();
	send_test_frame();
	send_test_frame();
#endif

	return	0;

abort:
	// TODO: bring device to a halt state, may be reset/disable via PSC
	return	-1;
}

int	dm646x_emac_deinit(void)
{
	return	0;
}


int	dm646x_emac_open(unsigned subdev_id)
{
	return	0;
}


int dm646x_emac_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int dm646x_emac_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int dm646x_emac_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	return	0;
}


int dm646x_emac_close(unsigned sub_id)
{
	return	0;
}

struct drv_entry	dm646x_emac = {dm646x_emac_init, dm646x_emac_deinit, dm646x_emac_open, dm646x_emac_read,
	dm646x_emac_write, dm646x_emac_ioctl, dm646x_emac_close};

