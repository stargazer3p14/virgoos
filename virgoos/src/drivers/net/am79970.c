/***************************************************
 *
 *	am79970.c
 *
 *	AMD Lance PCI (79970) ethernet driver
 *
 ***************************************************/

#include "sosdef.h"
#include "drvint.h"
#include "config.h"
#include "pcihost.h"
#include "errno.h"
#include "am79970.h"
#include "inet.h"

//#define	AM79970C_MODE_16BIT	1
#define	AM79970C_MODE_32BIT	1

//#define	DEBUG_IRQ	1
//#define	DEBUG_INIT	1
//#define	DEBUG_TEST	1
//#define	DEBUG_SEND	1
//#define	DEBUG_PCI_CONFIG	1
//#define	SEND_TEST_FRAME	1
//#define	DEBUG_RINGS

// (!) Ethernet address for the card must be unicast (bit 0 arrived should be 0).
// Countrary to IP fields definitions, ethernet addresses consider bit 0 to be an LSB of an octet
#define	ETH_ADDR	"\x10\x22\x33\x07\x08\x09"

extern	struct net_if	net_interfaces[MAX_NET_INTERFACES];

static void	am79970_get_send_packet(unsigned char **payload);
static int	am79970_send_packet(unsigned char *dest_addr, word protocol, unsigned size);
static void	am79970_get_recv_packet(struct net_if **pnet_if, void**ppkt);
static void	am79970_put_recv_descr(void *buf_descr);
void	am79970_isr_bh(void *prm);

struct eth_device	am79970_eth_device =
{
	ETH_DEV_TYPE_NIC,
	ETH_ADDR,
	am79970_get_send_packet,
	am79970_send_packet
};

static int	pci_dev_index = -1;
dword	base_port;				/* For I/O access */
dword	base_addr;				/* For memory access */

dword	frames_count;

#ifdef	SEND_TEST_FRAME
static	void	send_test_frame(void);
int	fill_test_data = 1;
#endif


// Initialization block
#if AM79970C_MODE_32BIT
// 32-bit
struct am79970_init_block	am79970_init_block;
#else
// 16-bit
struct am79970_init_block_mode0	am79970_init_block;
#endif

// Receive and transmit descriptors ring
#if AM79970C_MODE_32BIT
// 32-bit
struct am79970_recv_desc	*am79970_recv_desc_ring;
struct am79970_xmit_desc	*am79970_xmit_desc_ring;
#else
// 16-bit
struct am79970_recv_desc_mode0	*am79970_recv_desc_ring;
struct am79970_xmit_desc_mode0	*am79970_xmit_desc_ring;
#endif

unsigned	curr_rd, curr_td;
unsigned	prior_rd;	
int	received_frames = 0;
TASK_Q	*isr_bh_task_q = NULL;
// TODO: all those static vars will have to move to per-instance structure



void	dump_recv_ring(void)
{
	int	i;
	unsigned	pkt_len;
	
	// DEBUGDEBUGDEBUG -- dump ALL receive ring
	serial_printf("----------------- Dump receive ring ------------------------\r\n");
	for (i = 0; i < NUM_RECV_BUFFERS; ++i)
	{
		int	j;
#if AM79970C_MODE_32BIT
		unsigned char	*pkt_data = (unsigned char*)(*(dword*)&am79970_recv_desc_ring[i].rmd0);
#else
		unsigned char	*pkt_data = (unsigned char*)(*(dword*)&am79970_recv_desc_ring[i].rmd0 & 0xFFFFFF);
#endif

		if (i != curr_rd)
			continue;
		
		// Print suitable for both 16-bit and 32-bit modes
		serial_printf("rd=%d rmd0=%08X rmd1=%08X rmd2=%08X rmd3=%08X\r\n", i,
			(unsigned)am79970_recv_desc_ring[i].rmd0,
			(unsigned)am79970_recv_desc_ring[i].rmd1,
			(unsigned)am79970_recv_desc_ring[i].rmd2,
			(unsigned)am79970_recv_desc_ring[i].rmd3);
		serial_printf("Received packet:\r\n");
		serial_printf("\t");
#if AM79970C_MODE_32BIT
		pkt_len = am79970_recv_desc_ring[i].rmd2 & 0xFFF;
#else
		pkt_len = am79970_recv_desc_ring[i].rmd3 & 0xFFF;
#endif
		for(j = 0; j < pkt_len; ++j)
		{
			serial_printf("%02X ", (unsigned)pkt_data[j]);
			if (j % 16 == 15)
			{
				serial_printf("\r\n");
				if (j < pkt_len - 1)
					serial_printf("\t");
			}
		}
		serial_printf("\r\n");
	}
	serial_printf("\r\n"
		"============================================================\r\n");
}

void	dump_xmit_ring(void)
{
	int	i;
	
	// DEBUGDEBUGDEBUG -- dump ALL transmit ring
	serial_printf("----------------- Dump transmit ring ------------------------\r\n");
	for (i = 0; i < NUM_XMIT_BUFFERS; ++i)
	{
		int	j;
#if AM79970C_MODE_32BIT
		unsigned char	*pkt_data = (unsigned char*)(*(dword*)&am79970_xmit_desc_ring[i].tmd0);
#else
		unsigned char	*pkt_data = (unsigned char*)(*(dword*)&am79970_xmit_desc_ring[i].tmd0 & 0xFFFFFF);
#endif
		unsigned	n;
		
//		if (i != curr_td)
//			continue;

#if AM79970C_MODE_32BIT
		n = ~am79970_xmit_desc_ring[i].tmd1 + 1 & 0xFFFF;
#else
		n = ~am79970_xmit_desc_ring[i].tmd2 + 1 & 0xFFFF;
#endif
		// Print suitable for both 16-bit and 32-bit modes
		serial_printf("td=%d tmd0=%08X tmd1=%08X tmd2=%08X tmd3=%08X n = %d\r\n", i,
			(unsigned)am79970_xmit_desc_ring[i].tmd0,
			(unsigned)am79970_xmit_desc_ring[i].tmd1,
			(unsigned)am79970_xmit_desc_ring[i].tmd2,
			(unsigned)am79970_xmit_desc_ring[i].tmd3, n);
		serial_printf("Transmit packet:\r\n");
		serial_printf("\t");
		for(j = 0; j < n; ++j)
		{
			serial_printf("%02X ", (unsigned)pkt_data[j]);
			if (j % 16 == 15)
			{
				serial_printf("\r\n");
				if (j < n - 1)
					serial_printf("\t");
			}
		}
		serial_printf("\r\n");
	}
	serial_printf("\r\n"
		"============================================================\r\n");
}

void	dump_regs(void)
{
	int	i;
	dword	value;

	serial_printf("==============================================================\r\n");
	// Some registers need STOP = 1 to be accessed
	// Under VmWare this doesn't work (doesn't affect anything) too
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x4);		// STOP = 1
	udelay(100000);
	
	// VmWare stores in csr76:csr77 exact values instead of 2's complements in csr76:csr78
	for (i = 0; i <= 124; ++i)
	{
		outw(base_port + RAP, i);
		value = inw(base_port + RDP);
		serial_printf("CSR%d = %04X\r\n", i, value);
	}
	serial_printf("\r\n");
	serial_printf("Init state BCR registers:\r\n");
	for (i = 0; i <= 22; ++i)
	{
		outw(base_port + RAP, i);
		value = inw(base_port + BDP);
		serial_printf("BCR%d = %04X\r\n", i, value);
	}
	serial_printf("--------------------------------------------------------------\r\n");
}

static void    am79970_get_send_packet(unsigned char **payload)
{
	dword	addr;
#if 0
	word	csr20, csr21, csr30, csr31, csr32, csr33, csr34, csr35, csr38, csr39, csr42, csr43;
	
	outw(base_port + RAP, 20);
	csr20 = inw(base_port + RDP);	
	outw(base_port + RAP, 21);
	csr21 = inw(base_port + RDP);	
	outw(base_port + RAP, 30);
	csr30 = inw(base_port + RDP);	
	outw(base_port + RAP, 31);
	csr31 = inw(base_port + RDP);	
	outw(base_port + RAP, 32);
	csr32 = inw(base_port + RDP);	
	outw(base_port + RAP, 33);
	csr33 = inw(base_port + RDP);	
	outw(base_port + RAP, 34);
	csr34 = inw(base_port + RDP);	
	outw(base_port + RAP, 35);
	csr35 = inw(base_port + RDP);	
	outw(base_port + RAP, 38);
	csr38 = inw(base_port + RDP);	
	outw(base_port + RAP, 39);
	csr39 = inw(base_port + RDP);	
	outw(base_port + RAP, 42);
	csr42 = inw(base_port + RDP);	
	outw(base_port + RAP, 43);
	csr43 = inw(base_port + RDP);	


	serial_printf("%s(): curr_td=%d csr20=%04X csr21=%04X csr30=%04X csr31=%04X csr32=%04X csr33=%04X csr 34=%04X csr35=%04X csr38=%04X csr39=%04X csr42=%04X cwr43=%04X\r\n", __func__, curr_td,
		csr20, csr21, csr30, csr31, csr32, csr33, csr34, csr35, csr38, csr39, csr42, csr43);
#endif
	
#if AM79970C_MODE_32BIT
	if (am79970_xmit_desc_ring[curr_td].tmd1 & 0x80000000 /* OWN = 1? */)
#else
	if (am79970_xmit_desc_ring[curr_td].tmd1 & 0x8000 /* OWN = 1? */)
#endif
	{
		*payload = NULL;
		return;
	}

#if AM79970C_MODE_32BIT
	addr = *(dword*)&am79970_xmit_desc_ring[curr_td].tmd0 + sizeof(struct eth_frame_hdr);
#else
	addr = (*(dword*)&am79970_xmit_desc_ring[curr_td].tmd0 & 0xFFFFFF) + sizeof(struct eth_frame_hdr);
#endif
	*payload = (char*)addr;
}

/*
 *	Interface function that sends a packet
 */
static int	am79970_send_packet(unsigned char *dest_addr, word protocol, unsigned size)
{
	dword	addr;
	struct eth_frame_hdr	*eth_hdr;
	int	i;

#if AM79970C_MODE_32BIT
	if (am79970_xmit_desc_ring[curr_td].tmd1 & 0x80000000 /* OWN = 1? */)
#else
	if (am79970_xmit_desc_ring[curr_td].tmd1 & 0x8000 /* OWN = 1? */)
#endif
	{
		serial_printf("curr_td (%d) is owned by PCNET, weird error\r\n", curr_td);
		return -1;
	}

#if DEBUG_SEND
	serial_printf("%s(): Submitting frame to send, curr_td = %d size=%u\r\n", __func__, curr_td, size);
#endif
#if AM79970C_MODE_32BIT
		addr = *(dword*)&am79970_xmit_desc_ring[curr_td].tmd0;
#else
		addr = *(dword*)&am79970_xmit_desc_ring[curr_td].tmd0 & 0xFFFFFF;
#endif
	eth_hdr = (struct eth_frame_hdr*)addr;
	memcpy(eth_hdr->dest_addr, dest_addr, ETH_ADDR_SIZE);
	memcpy(eth_hdr->src_addr, ETH_ADDR, ETH_ADDR_SIZE); 
	eth_hdr->frame_size = protocol;					// Already in network order
	size += sizeof(struct eth_frame_hdr);

#if 0
	// According to Wireshark's capture, pcnet emulation on qemu doesn't auto-pad packets.
	// Pad an outgoing packet to 64 bytes minimum manually
	// This works; however, this doesn't solve "missing sent packets" mystery
	if (size < MIN_ETH_PACKET_SIZE)
	{
		serial_printf("%s(): padding a short frame (size = %u) to a minimum of %u bytes\r\n", __func__, size, MIN_ETH_PACKET_SIZE);
		memset((unsigned char*)addr + size, 0, MIN_ETH_PACKET_SIZE - size);
		size = MIN_ETH_PACKET_SIZE;
	}
#endif

#if AM79970C_MODE_32BIT
		am79970_xmit_desc_ring[curr_td].tmd1 = (~(size & 0xFFF) + 1 & 0xFFFF);	// Length
		am79970_xmit_desc_ring[curr_td].tmd2 = 0;			// Misc
		am79970_xmit_desc_ring[curr_td].tmd0 = addr;			// Base
		udelay(10);
		am79970_xmit_desc_ring[curr_td].tmd1 |= 0x83000000;		// Status : OWN, STP, ENP = 1. OWN = 1 effectively submits
#else
	am79970_xmit_desc_ring[curr_td].tmd2 = ~(size & 0xFFF) + 1;
	am79970_xmit_desc_ring[curr_td].tmd3 = 0;
	am79970_xmit_desc_ring[curr_td].tmd1 &= 0xFF;
	am79970_xmit_desc_ring[curr_td].tmd1 |= 0x8300;			// OWN, STP, ENP = 1. OWN = 1 effectively submits for transmission
#endif

	curr_td = (curr_td + 1) % NUM_XMIT_BUFFERS;
#if DEBUG_SEND
//serial_printf("+++++++++++ Adjust curr_td\r\n");
//serial_printf("+++++++++++ curr_td = %d\r\n", curr_td);
#endif

	// Trigger immediate send poll
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x48);

#if DEBUG_SEND
	// DEBUGDEBUGDEBUG -- dump all xmit ring
	dump_xmit_ring();
#endif
	return	0;
}


static void	am79970_put_recv_descr(void *buf_descr)
{
#if AM79970C_MODE_32BIT
// 32-bit
	struct am79970_recv_desc	*desc = buf_descr;
#else
// 16-bit
	struct am79970_recv_desc_mode0	*desc = buf_descr;
#endif

#if AM79970C_MODE_32BIT
	desc->rmd1 |= 0x80000000;
#else
	desc->rmd1 |= 0x8000;
#endif
}


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
void	am79970_isr_bh(void *prm)
{
	struct eth_frame_hdr	*pdata;

	serial_printf("%s(): entered\n", __func__);

	while (1)
	{
		nap(&isr_bh_task_q);
//		serial_printf("%s(): woke up, have %d packets to process\n", __func__, received_frames);

		while (received_frames--)
		{
			// Get received buffer
#if AM79970C_MODE_32BIT
			pdata = (struct eth_frame_hdr*)*(dword*)&am79970_recv_desc_ring[prior_rd].rmd0;
#else
			pdata = (struct eth_frame_hdr*)(*(dword*)&am79970_recv_desc_ring[prior_rd].rmd0 & 0xFFFFFF);
#endif
			eth_parse_packet(&net_interfaces[0], pdata);		// TODO: invent something more configurable then hardcoded "0" index
			// Release the buffer to NIC
			am79970_put_recv_descr(&am79970_recv_desc_ring[prior_rd]);
			prior_rd = (prior_rd + 1) % NUM_RECV_BUFFERS;
		}
	}
}


/*
 *	AM79970 IRQ handler.
 */
static	int	am79970_isr(void)
{
	static int	irq_count;
	word	csr0, csr5;
	int	boguscnt = 2;			// 2 is default for pcnet32 driver, on lance it was 10
	word	csr24, csr25, csr30, csr31;
	struct eth_frame_hdr	*pdata;

	received_frames = 0;
#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received AM79970 IRQ [may be] no. %d\r\n", __func__, ++irq_count);
#endif

	outw(base_port + RAP, 5);
	csr5 = inw(base_port + RDP);
	
	outw(base_port + RAP, 0);
	csr0 = inw(base_port + RDP);
	
	/* Print the interrupt cause: CSR0 and CSR5 */
#ifdef	DEBUG_IRQ
	serial_printf("%s(): CSR0=%04X CSR5=%04X (+ACK)\r\n", __func__, csr0, csr5);
#endif

	// Keep track if curr_rd changes during handling - we may need to update ownership later
	prior_rd = curr_rd;

	// Loop of up to 2 iteractions of boguscnt - due to Linux driver
	while (csr0 & (0x8F00 | CSR0_STATUS_INTR) && --boguscnt >= 0)	// lance driver used 0x8600 mask
//	if (csr0 & CSR0_STATUS_INTR)
	{
		// Acknowledge interrupts (write 1's to all source locations that were 1's)	
		outw(base_port + RDP, csr0 & ~0x004F);
//		csr0 &= 0xFF00;					// isolate inerrupt status-related bits

		outw(base_port + RAP, 5);
		outw(base_port + RDP, csr5);
	
		if (csr0 & CSR0_STATUS_ERR)
		{
			serial_printf("Am79970 error: ");
			if (csr0 & CSR0_STATUS_MERR)
				serial_printf("Memory error; ");
			if (csr0 & CSR0_STATUS_MISS)
				serial_printf("Missed frame error; ");
			if (csr0 & CSR0_STATUS_CERR)
				serial_printf("Collision error; ");
			if (csr0 & CSR0_STATUS_BABL)
				serial_printf("Babble error; ");
			serial_printf("\r\n");
		}
		if (csr0 & CSR0_STATUS_TINT)
		{
#if DEBUG_IRQ
			serial_printf("Am79970: transmit interrupt\r\n");
#endif
		}
		if (csr0 & CSR0_STATUS_RINT)
		{
#if DEBUG_IRQ
			serial_printf("%s(): Receive interrupt! curr_rd = %d\r\n", __func__, curr_rd);
#endif
			// Receive frames in loop, because we can lose frame interrupts (get 1 IRQ for several frames,
			// PCNET doesn't "remember")
			// NOTE(!) there was a nasty bug hiding in this scheme. We must keep track at what curr_rd we started accepting frames: we must NOT
			// release buffer ownership to PCNET before we process ALL the buffers... otherwise PCNET may start writing with OWNER bit while we're
			// still processing in a loop, and we will pick the frame at the same time as it is being written... too bad, headers, lengths may
			// be got wrong, buffers overflowed, crashes...
			// NOTE(!!) a more realistic bug is due to use of do()/while() instead of while(). Scenario to reproduce: arrive packets that need sending
			// response packets (ICMP, TCP SYN). Since we still do it in ISR context, after sending the packet we have a NEW interrrupt condition
			// (CSR0_STATUS_TINT), so the loop continues. Since we havent removed the CSR0_STATUS_RINT condition yet (we clean all interrupts at once),
			// we enter this loop again, despite that when processing the last rx packet which included tx (or the only packet that arrived), we
			// already done and the first packet that we happily process in that new entry is owned by PCNET.
			// Additional solution to all this is remove the "boguscnt" loop - we are not interested in updated interrupts conditions, let them generate
			// another interrupt
//			do
			while (!(am79970_recv_desc_ring[curr_rd].rmd1 & 0x80000000))
			{
#if 0
#if AM79970C_MODE_32BIT
				pdata = (struct eth_frame_hdr*)*(dword*)&am79970_recv_desc_ring[curr_rd].rmd0;
#else
				pdata = (struct eth_frame_hdr*)(*(dword*)&am79970_recv_desc_ring[curr_rd].rmd0 & 0xFFFFFF);
#endif
#endif
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

#if DEBUG_IRQ
serial_printf("Calling eth_parse_packet()\n");
#endif
				// Parse ethernet frame
//				eth_parse_packet(&net_interfaces[0], pdata);		// TODO: invent something more configurable then hardcoded "0" index
			
				// Advance "current receive descriptor" index
				curr_rd = (curr_rd + 1) % NUM_RECV_BUFFERS;
				++received_frames;

				// Check for ring-full.
				if (curr_rd == prior_rd)
				{
					serial_printf("%s(): receive ring is full\n", __func__);
					break;
				}

#if DEBUG_IRQ
serial_printf("In do/while(): curr_rd=%d\n", curr_rd);
#endif
#if 0
#if AM79970C_MODE_32BIT
			} while (!(am79970_recv_desc_ring[curr_rd].rmd1 & 0x80000000));		// Assume that one interrupt may signify multiple frames
#else
			} while (!(am79970_recv_desc_ring[curr_rd].rmd1 & 0x8000));
#endif
#else
//			} while (0);			// Assume only one received frame per interrupt. Every frame has to emit its own interrupt (?)
			}
#endif

			outw(base_port + RAP, 0);
			csr0 = inw(base_port + RDP);	// For the next iteration
		} // while (RINT)

#if DEBUG_IRQ
serial_printf("%s(): Handling done, enable interrupts\n", __func__);
#endif
		/* Clear any other interrupt, and set interrupt enable - due to Linux driver. */
		outw(base_port + RAP, 0);
		outw(base_port + RDP, 0x7F40);		// All interrupt sources + IENA = 1

	} // while (CSR0_STATUS_INTR && bogus_cnt)

	// Only NOW release all buffers to NIC - AFTER they are all processed
#if DEBUG_IRQ
	serial_printf("**************** %s(): reception handling done, releasing ownership. received_frames=%d *************** \n", __func__, received_frames);
#endif

#if 0
	while (received_frames--)
	{
		// Release the buffer to NIC
		am79970_put_recv_descr(&am79970_recv_desc_ring[prior_rd]);
		prior_rd = (prior_rd + 1) % NUM_RECV_BUFFERS;
	}
#endif
	if (received_frames)
		wake(&isr_bh_task_q);
		
	// Acknowledge interrupts (write 1's to all source locations that were 1's)	
//	outw(base_port + RAP, 0);
//	outw(base_port + RDP, csr0 & ~0x004F);

#if DEBUG_IRQ
serial_printf("%s(): Return, allow shared interrupts\n", __func__);
#endif
	return	0;		/* Allow shared interrupts */
}


#ifdef	SEND_TEST_FRAME
/*
 *	Send a test frame to broadcast address
 */
static	void	send_test_frame(void)
{
	int	i;
	unsigned	len = 1500;
	char	*transmit_data;

	transmit_data = (char*)(*(dword*)&am79970_xmit_desc_ring[curr_td].tmd0 & 0xFFFFFF) + sizeof(struct eth_frame_hdr);

	if (fill_test_data)
	{
		/* Set-up correct ethernet header */
		/* ... */
		/* Fill destination address */
		for (i = 0; i < 6; ++i)
			transmit_data[i] = 0xFF;

		/* Fill source address */
		for (i = 0; i < 6; ++i)
			transmit_data[i+6] = am79970_eth_device.addr[i];

		*(word*)(transmit_data + 12) = htons(len);


		/* Fill packet with test data */
		for (i = 14; i < 1500; ++i)
			transmit_data[i] = i*2+1 & 0xFF;
	}

	len = 1500;
	am79970_xmit_desc_ring[curr_td].tmd2 = (word)(0xF000 | ~len + 1 & 0xFFF);
	
	serial_printf("%s(1): curr_td = %d transmit_data=%08X tmd0=%04X tmd1=%04X tmd2=%04X tmd3=%04X\r\n",
		__func__, curr_td, transmit_data, am79970_xmit_desc_ring[curr_td].tmd0,
		am79970_xmit_desc_ring[curr_td].tmd1, am79970_xmit_desc_ring[curr_td].tmd2, am79970_xmit_desc_ring[curr_td].tmd3);
	
	am79970_xmit_desc_ring[curr_td].tmd1 |= 0x8300;	// OWN, STP, ENP = 1 - packet is set
	
	// Trigger an immediate send poll (due to Linux driver)
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x48);			// IENA, TDMD = 1
	
	udelay(100000);
	serial_printf("%s(): completed\r\n", __func__);
	serial_printf("%s(2): curr_td = %d transmit_data=%08X tmd0=%04X tmd1=%04X tmd2=%04X tmd3=%04X\r\n",
		__func__, curr_td, transmit_data, am79970_xmit_desc_ring[curr_td].tmd0,
		am79970_xmit_desc_ring[curr_td].tmd1, am79970_xmit_desc_ring[curr_td].tmd2, am79970_xmit_desc_ring[curr_td].tmd3);
	
	curr_td = (curr_td + 1) % NUM_XMIT_BUFFERS;	
}
#endif


/*
 *	Initialize the ethernet controller (according to Software Developer's Manual's
 *	recommendation)
 */
int	am79970_init(unsigned drv_id)
{
	dword	value = -1;
	word	eeprom_val;
	int	i;
	int	int_line;

	// For test purposes - to let UART interrupt not interfere with NIC
//	return	0;
	
	/* Open PCI host driver - no need */

	/* Find AM79970 device */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_FIND_DEV, VENDOR_ID, DEVICE_ID, &pci_dev_index);
	if (pci_dev_index != -1)
		serial_printf("Found AMD Lance (79970) device at index %d\r\n", pci_dev_index);
	else
	{
		serial_printf("AMD Lance (79970) device was not found\r\n", pci_dev_index);
		return	ENODEV;
	}

	/* Device found */

	// (!) VmWare's emulation does not implement BAR1 and BAR2 as specified in AM79C979A Datasheet.
	// Only BAR0 is implemented, allowing only I/O access
	
	// Configure BAR1 with memory-based start address
	// Configure COMMAND register with "Bus Master" = 1
	serial_printf("Writing Command with MEMEN = 0\r\n");
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_WRITEW, pci_dev_index, PCI_CFG_COMMAND_OFFS, PCMCMD_MASTER);
	serial_printf("Writing BAR1 with 0x%08X\r\n", AM79970_START_ADDR);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_WRITED, pci_dev_index, PCI_CFG_BAR1_OFFS, AM79970_START_ADDR);
	serial_printf("Writing Command with MEMEN = 1\r\n");
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_WRITEW, pci_dev_index, PCI_CFG_COMMAND_OFFS, PCMCMD_MASTER | PCICMD_MEMEN | PCICMD_IOEN);
	
	/* Print its current configuration */
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
	base_port = (value & ~0x3);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR1_OFFS, &value);
	base_addr = (dword)(value & ~0xF);

	/* Setup IRQ */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTLINE_OFFS, &int_line);
	serial_printf("The AMD Lance (79970) uses IRQ line %d\r\n", int_line);
	set_int_callback(int_line, am79970_isr);

#ifdef	DEBUG_PCI_CONFIG	
	serial_printf("%s(): base_addr=%08X base_port=%08X\r\n", __func__, base_addr, base_port);
#endif

	/* Dump CSR and BCR registers */
#ifdef	DEBUG_PCI_CONFIG
	serial_printf("Init state CSR registers:\r\n");
	for (i = 0; i <= 124; ++i)
	{
		outw(base_port + RAP, i);
		value = inw(base_port + RDP);
		serial_printf("CSR%d = %04X\r\n", i, value);
	}
	serial_printf("\r\n");
	serial_printf("Init state BCR registers:\r\n");
	for (i = 0; i <= 22; ++i)
	{
		outw(base_port + RAP, i);
		value = inw(base_port + BDP);
		serial_printf("BCR%d = %04X\r\n", i, value);
	}
#endif

	/*
	 *	Now perform real initialization
	 */
	// Linux driver always does S_RESET, but looks that we don't need it
	inw(base_port + RESET);
	udelay(10);
	
#if AM79970C_MODE_32BIT
	// Set up modes (leave 16-bit I/O, set up 32-bit addressing and SWSTYLE 1 or 2)
	// Set STOP = 1 so that we can do the following initialization access to BCR20[0..7]
	//outw(base_port + RAP, 0);
	//outw(base_port + RDP, 0x4);		// STOP = 1

	outw(base_port + RAP, 20);
	outw(base_port + BDP, 0x2);			// SWSTYLE = 2
#endif

#if 1
	// Auto-select media interface port (due to Linux driver)
	outw(base_port + RAP, 0x2);
	outw(base_port + BDP, 0x2 | inw(base_port + BDP) & ~2);
#endif
	 
#if 1 
	// (!) Enable/disable full-duplex mode. That might have been a problem
	outw(base_port + RAP, 0x9);
	outw(base_port + BDP, 0x3 | inw(base_port + BDP) /*& ~0x3*/);
	serial_printf("%s(): full-duplex mode is %s\n", __func__, inw(base_port + BDP) & 0x1 ? "enabled" : "disabled");
#endif

	// set/reset GPSI bit in test register (due to Linux driver)
	outw(base_port + RAP, 124);
	outw(base_port + RDP, inw(base_port + RDP) & ~0x10);

	// Handle some mysterious AMD technical note dated 24/06/2004, due to Linux driver
	// Enable auto negotiate, setup, disable fd
	outw(base_port + RAP, 32);
	outw(base_port + BDP, 0x80 | inw(base_port + BDP));
	outw(base_port + BDP, 0x20 | inw(base_port + BDP) & ~0x98);

	// Disable transmit stop on underflow; mask unwanted interrupts (IDON)
	outw(base_port + RAP, 0x3);
	outw(base_port + RDP, 0x140 | inw(base_port + RDP));

	// malloc() / calloc() must take care of 16-byte alignment
	am79970_recv_desc_ring = calloc(1, sizeof(*am79970_recv_desc_ring) * NUM_RECV_BUFFERS);
	am79970_xmit_desc_ring = calloc(1, sizeof(*am79970_xmit_desc_ring) * NUM_XMIT_BUFFERS);

#if DEBUG_INIT
	serial_printf("%s(): am79970_recv_desc_ring = %08X, am79970_xmit_desc_ring = %08X\r\n", __func__, am79970_recv_desc_ring, am79970_xmit_desc_ring);
#endif

	// Initialize init block (ethernet address is a must!)
	// RLEN and TLEN are set to 0, later precise size will be set via CSR76/78
	memset(&am79970_init_block, 0, sizeof(am79970_init_block));
#if AM79970C_MODE_32BIT
	memcpy(&am79970_init_block.padr00_31, am79970_eth_device.addr, 6);
	am79970_init_block.rdra = (dword)am79970_recv_desc_ring;
	am79970_init_block.tdra = (dword)am79970_xmit_desc_ring;
	am79970_init_block.mode = (LOG2_NUM_RECV_BUFFERS << 20 | LOG2_NUM_XMIT_BUFFERS << 28);		// RLEN = LOG2_NUM_RECV_BUFFERS, TLEN = LOG2_NUM_XMIT_BUFFERS,  PROM = 0
#else
	memcpy(&am79970_init_block.padr00_15, am79970_eth_device.addr, 6);
	am79970_init_block.rdra00_15 = (word)((dword)am79970_recv_desc_ring & 0xFFFF);
	am79970_init_block.rdra16_23_rlen = (word)((dword)am79970_recv_desc_ring >> 16 & 0xFF | 0x4000 /* Rx ring length = 4 (RLEN = 2) */);
	am79970_init_block.tdra00_15 = (word)((dword)am79970_xmit_desc_ring & 0xFFFF);
	am79970_init_block.tdra16_23_tlen = (word)((dword)am79970_xmit_desc_ring >> 16 & 0xFF | 0x4000 /* Tx ring length (TLEN = 2) */);
	am79970_init_block.mode = 0x0;
#endif	

	// Allocate receive and transmit buffers
	for (i = 0; i < NUM_RECV_BUFFERS; ++i)
	{
		dword	addr;
		
		addr = (dword)calloc(1, RECV_BUFFER_SIZE);
#if AM79970C_MODE_32BIT
		am79970_recv_desc_ring[i].rmd0 = addr;
		am79970_recv_desc_ring[i].rmd1 = 0x8000F000 /* ONES 12-15, OWN = 1 */ | (~(word)(RECV_BUFFER_SIZE + 1) & 0xFFF);
#else
		am79970_recv_desc_ring[i].rmd0 = (word)(addr & 0xFFFF);
		am79970_recv_desc_ring[i].rmd1 = (word)(addr >> 16 & 0xFF | 0x8000);
		am79970_recv_desc_ring[i].rmd2 = (word)(0xF000 | (~(word)RECV_BUFFER_SIZE + 1 & 0xFFF));
#endif
	}
	 
	for (i = 0; i < NUM_XMIT_BUFFERS; ++i)
	{
		dword	addr;
		
		addr = (dword)calloc(1, XMIT_BUFFER_SIZE);
#if AM79970C_MODE_32BIT
		am79970_xmit_desc_ring[i].tmd0 = addr;
#else
		am79970_xmit_desc_ring[i].tmd0 = (word)(addr & 0xFFFF);
		am79970_xmit_desc_ring[i].tmd1 = (word)(addr >> 16 & 0xFF);
#endif
	}
	
#if 0
	// Dump recv descriptors
	dump_recv_ring();

	// Dump xmit descriptors
	dump_xmit_ring();
#endif

/*
	// Set up and enable interrupts mask
	outw(base_port + RAP, 3);
	outw(base_port + RDP, 0);		// Enable all regular interrupts - this is mask register
	outw(base_port + RAP, 5);
	outw(base_port + RDP, 0);		// Disable all additional interrupts - this is enable register

	// Enable INTA, so that INIT can generate an interrupt
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x40);		// IENA = 1
*/	 
	// Load init block
	outw(base_port + RAP, 1);
	outw(base_port + RDP, (dword)&am79970_init_block & 0xFFFF);
	outw(base_port + RAP, 2);
	outw(base_port + RDP, (dword)&am79970_init_block >> 16);

	outw(base_port + RAP, 4);
	outw(base_port + RDP, 0x915 /*0x1915*/);		// Taken from Linux driver source, default value is 0x115. Auto-pad transmit frames to 64 bytes.
						// Disable transmit polling - we will use CSR0.TDMD to trigger transmission

	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x1);		// INIT = 1

	// Wait untile IDON = 1
	do
	{
		outw(base_port + RAP, 0);
		value = inw(base_port + RDP) & 0x100;
	} while (0 == value);
#if DEBUG_INIT
	serial_printf("Init read, IDON = 1\r\n");
#endif
	 
#if 0
	// Set STOP = 1
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 4);

	// Initialize receive descriptors ring and appropriate count value (CSR76/78)
	outw(base_port + RAP, 76);
	outw(base_port + RDP, ~(word)NUM_RECV_BUFFERS + 1);
	 
	// Initialize transmit descriptors ring and appropriate count value (CSR76/78)
	// VmWare emulation probably contains a bug: it stores direct values instead of two's complement and uses CSR76/77 instead of CSR76/78
	outw(base_port + RAP, 77);
	outw(base_port + RDP, ~(word)NUM_XMIT_BUFFERS + 1);
	outw(base_port + RAP, 78);
	outw(base_port + RDP, ~(word)NUM_XMIT_BUFFERS + 1);
#endif

	// Set up additional CSR registers as necessary
	 
	// Set up additional BCR registers as necessary

#if DEBUG_INIT
	serial_printf("%s(): init done. Dump registers:\r\n", __func__);
#endif

	/* Dump CSR and BCR registers */
#ifdef	DEBUG_INIT
	// Some registers need STOP = 1 to be accessed
	// Under VmWare this doesn't work (doesn't affect anything) too
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x4);		// STOP = 1
	udelay(100000);

#if 0	
	// Write csr28:csr29
	outw(base_port + RAP, 28);
	outw(base_port + RDP, (word)((dword)&am79970_recv_desc_ring[0] & 0xFFFF));
	outw(base_port + RAP, 29);
	outw(base_port + RDP, (word)((dword)&am79970_recv_desc_ring[0] >> 16 & 0xFFFF));

	// Write csr26:csr27
	outw(base_port + RAP, 26);
	outw(base_port + RDP, (word)((dword)&am79970_recv_desc_ring[1] & 0xFFFF));
	outw(base_port + RAP, 27);
	outw(base_port + RDP, (word)((dword)&am79970_recv_desc_ring[1] >> 16 & 0xFFFF));
	
	// Write csr18:csr19
	outw(base_port + RAP, 18);
	outw(base_port + RDP, (word)(am79970_recv_desc_ring[0].rmd0));
	outw(base_port + RAP, 19);
	outw(base_port + RDP, (word)(am79970_recv_desc_ring[0].rmd1 & 0xFF));
	
	
	// Write csr20:csr21
	outw(base_port + RAP, 20);
	outw(base_port + RDP, (word)(am79970_recv_desc_ring[1].rmd0));
	outw(base_port + RAP, 21);
	outw(base_port + RDP, (word)(am79970_recv_desc_ring[1].rmd1 & 0xFF));
#endif
	
	// VmWare stores in csr76:csr77 exact values instead of 2's complements in csr76:csr78
	for (i = 0; i <= 124; ++i)
	{
		outw(base_port + RAP, i);
		value = inw(base_port + RDP);
		serial_printf("CSR%d = %04X\r\n", i, value);
	}
	serial_printf("\r\n");
	serial_printf("Init state BCR registers:\r\n");
	for (i = 0; i <= 22; ++i)
	{
		outw(base_port + RAP, i);
		value = inw(base_port + BDP);
		serial_printf("BCR%d = %04X\r\n", i, value);
	}
#endif // DEBUG_INIT

	// Clear IDON (Linux driver reports that there's some problem in 79974 chip. Right now we don't care)
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x100);

	// Enable everything that needs to be explicitly enabled
	outw(base_port + RAP, 0);
	outw(base_port + RDP, 0x42);		// STRT, IENA = 1

#ifdef	SEND_TEST_FRAME
	/* Send 1 frame to broadcast address */
	send_test_frame();
	send_test_frame();
	send_test_frame();
#endif

#if 0
// under QEMU this works (sends requests) but doesn't help find us. In fact, it becomes harder to find us (don't know why)
	// Send gratuitious ARP 3 times
	send_grat_arp(&net_interfaces[0]);
	send_grat_arp(&net_interfaces[0]);
	send_grat_arp(&net_interfaces[0]);
#endif
	
serial_printf("%s(): completed\n", __func__);
	return	0;
}

int	am79970_deinit(void)
{
	return	0;
}


int	am79970_open(unsigned subdev_id)
{
	return	0;
}


int am79970_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int am79970_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int am79970_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	return	0;
}


int am79970_close(unsigned sub_id)
{
	return	0;
}

struct drv_entry	am79970 = {am79970_init, am79970_deinit, am79970_open, am79970_read,
	am79970_write, am79970_ioctl, am79970_close};

