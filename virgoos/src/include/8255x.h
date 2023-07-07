/*
 *	Header file for Intel 8255x ethernet driver
 */

#include "sosdef.h"
#include "inet.h"


/* 8255x vendor ID and device ID */
#define	VENDOR_ID	0x8086
#define	DEVICE_ID	0x1209


/*
 *	Configurable parameters
 */
#define	NUM_RECV_BUFFERS	10

/* Control and status registers */
/* Pointed to by BAR0 (memory-maped) and BAR1 (I/O mapped) */
#define	SCB_STATUS_WORD		0
#define	SCB_CMD_WORD		2
#define	SCB_GEN_PTR			4
#define	SCB_PORT			8
#define	SCB_EEPROM_CTL		0xE
#define	SCB_MDI_CTL			0x10
#define	SCB_RX_BYTE_COUNT	0x14
#define	SCB_FLOW_CTL		0x19
#define	SCB_RMDR			0x1B
#define	SCB_GEN_CTL			0x1C
#define	SCB_GEN_STATUS		0x1D
#define	SCB_FUNC_EVENT		0x30
#define	SCB_EVENT_MASK		0x34
#define	SCB_PRESENT_STATE	0x38
#define	SCB_FORCE_EVENT		0x3C

/*
 *	System Control Block (SCB) registers. Accodring to the Software Developer's Manual, no
 *	special requirements for alignment are imposed.
 *
 *	Structure (for memory-based access)
 */

struct	scb
{
	word	status_word;
	word	command_word;
	dword	gen_ptr;
	dword	port;
	word	reserved1;
	word	eeprom_ctl;
	dword	mdi_ctl;
	dword	rx_byte_count;
	byte	reserved2;
	word	flow_ctl;
	byte	rmdr;
	byte	gen_ctl;
	byte	gen_status;
	word	reserved3;
	dword	reserved4[4];
	dword	func_event;
	dword	event_mask;
	dword	present_state;
	dword	force_event;
} __attribute__ ((packed));


//------------------- Status definitions -----------------------
#define	SCB_STS_CXINT		0x8000				/* Command completed with CX interrupt set */
#define	SCB_STS_FRAMERECV	0x4000				/* Frame is received */
#define	SCB_STS_CNA			0x2000				/* Entered non-active state */
#define	SCB_STS_RNR			0x1000				/* Left ready state */
#define	SCB_STS_MDI			0x0800				/* MDI read/write cycle completed */
#define	SCB_STS_SWI			0x0400				/* S/w interrupt */
#define	SCB_STS_FCPINT		0x0100				/* Flow control pass interrupt (NOT USED) */
#define	SCB_STS_CUSTATUS	0x00C0				/* CU status */
#define	SCB_STS_RUSTATUS	0x003C				/* RU status */

// CU status masked values
#define	SCB_STS_CU_IDLE		0x0					/* CU status -- idle */
#define	SCB_STS_CU_SUSPENDED	0x40			/* CU status -- suspended */
#define	SCB_STS_CU_LPQ		0x80				/* CU status -- LPQ Active */
#define	SCB_STS_CU_HQP		0xC0				/* CU status -- HQP Active */

// RU status masked values
#define	SCB_STS_RU_IDLE		0x0					/* RU status -- idle */
#define	SCB_STS_RU_SUSPENDED	0x4				/* RU status -- suspended */
#define	SCB_STS_RU_NORESOURCES	0x8				/* RU status -- no resources */
#define	SCB_STS_RU_READY	0x10				/* RU status -- ready */


//------------------- Commands definitions -----------------------
#define	SCB_CMD_CXMASK		0x8000				/* Mask CX interrupt */
#define	SCB_CMD_FRMASK		0x4000				/* Mask FR interrupt */
#define	SCB_CMD_CNAMASK		0x2000				/* Mask CNA interrupt */
#define	SCB_CMD_RNRMASK		0x1000				/* Mask RNR interrupt */
#define	SCB_CMD_ERMASK		0x0800				/* Mask ER interrupt */
#define	SCB_CMD_FCPMASK		0x0400				/* Mask FCP interrupt */
#define	SCB_SMD_SWINT		0x0200				/* S/w interrupt */
#define	SCB_CMD_MASK_INTA	0x0100				/* Block interrupt from PCI bus */

// Command unit control
#define	SCB_CMD_CUC_NOP		0					/* NOP command */
#define	SCB_CMD_CUC_START	0x0010				/* Start the CBL command */
#define	SCB_CMD_CUC_RESUME	0x0020				/* Resume (after idle) */
#define	SCB_CMD_CUC_LOADDUMPADDR	0x0040		/* Load dump counters address */
#define	SCB_CMD_CUC_DUMPSTATS		0x0050		/* Dump statistical counters */
#define	SCB_CMD_CUC_LOADCUBASE		0x0060		/* Load CU base address (0) */
#define	SCB_CMD_CUC_DUMPRESETSTATS	0x0070		/* Dump and reset stats counters */
#define	SCB_CMD_CUC_STATICRESUME	0x00A0		/* Static resume (NOT USED */

// Receive unit control
#define	SCB_CMD_RUC_NOP		0
#define	SCB_CMD_RUC_START	0x1					/* Enable receive unit */
#define	SCB_CMD_RUC_RESUME	0x2					/* Resume frames reception */
#define	SCB_CMD_RUC_DMAREDIR	0x3				/* Receive DMA redirect (NOT USED) */
#define	SCB_CMD_RUC_ABORT	0x4					/* Immediately abort any receive operation */
#define	SCB_CMD_RUC_LOADHDRDATASIZE	0x5			/* Load header data size (even) */
#define	SCB_CMD_RUC_LOADRUBASE	0x6				/* Load RU base address */


/*
 *	Transmit control block.
 *	Simplified memory model is used. The manual says that currently only the simplified mode
 *	is supported.
 */

struct	transmit_cb
{
	word	status;
	word	command;
	dword	link_addr;
	dword	tbd_array_addr;
	word	byte_count;
	byte	transmit_threshold;
	byte	tbd_num;
	struct eth_frame_hdr	frame_hdr;
	char	frame_data[1500];		/* Standard ethernet frame: MTU = 1500 bytes. */
	dword frame_crc;
} __attribute__ ((packed));

struct	addr_setup_cb
{
	word	status;
	word	command;
	dword	link_addr;
	char	address[6];
	word	reserved;
} __attribute__ ((packed));


/* Command block definitions */
#define	CB_CMD_ENDOFLIST	0x8000			/* End-of-list: last block in a list */
#define	CB_CMD_SUSPEND		0x4000			/* Suspend after this block */
#define	CB_CMD_COMPLETEINT	0x2000			/* Completion interrupt after this block */
#define	CB_CMD_CNA_INTDELAY	0x1F00			/* CNA interrupt delay (NOT USED) */
#define	CB_CMD_NOCRC		0x0010			/* No CRC is inserted by the card */
#define	CB_CMD_FLEXMODE		0x0008			/* 1-flexible mode (NOT USED), 0-simplified mode */

#define	CB_CMD_TRANSMIT		0x0004			/* Transmit command */
#define	CB_CMD_NOP			0				/* NOP */
#define	CB_CMD_ADDRSETUP	0x0001			/* Setup individual address */

#define	CB_STS_CMDACCEPTED	0x8000			/* Command completely read from the TCB */
#define	CB_STS_CMDOK		0x2000			/* Command done OK (for transmit the same as _CMDACCEPTED */
#define	CB_STS_UNDERRUN		0x1000			/* Underrun (why should happen?..) */


/*
 *	Receive frame descriptor
 */
struct	rfd
{
	word	status;
	word	command;
	dword	link_addr;
	dword	reserved;
	word	actual_count;					/* 14 bits + flags */
	word	size;							/* 14 bits + '00' */
	struct eth_frame_hdr	frame_hdr;
	char	frame_data[1500];				/* Standard ethernet frame: MTU = 1500 bytes. */
	dword frame_crc;

	/* Some housekeeping info */
	dword	seq_num;						/* To determine frames order in cyclic queue */
} __attribute__ ((packed));

/* RFD definitions */
#define	RFD_CMD_ENDOFLIST	0x8000			/* End-of-list: last descriptor in a list */
#define	RFD_CMD_SUSPEND		0x4000			/* Suspend after this descriptor */
#define	RFD_CMD_HEADER		0x0001			/* Header RFD (NOT USED) */
#define	RFD_CMD_FLEXMODE	0x0008			/* 1-flexible mode (NOT USED), 0-simplified mode */

#define	RFD_STS_COMPLETED	0x8000			/* RFD reception completed */
#define	RFD_STS_OK			0x2000			/* Frame was received without errors and stored in memory */
#define	RFD_STS_CRCERR		0x0800			/* CRC error in received frame */
#define	RFD_STS_ALIGNERR	0x0400			/* Alignment error (CRC error in aligned frame) */
#define	RFD_STS_NORESOURCES	0x0200			/* Frame is bigger than 1500 */
#define	RFD_STS_DMAOVERRUN	0x0100			/* Only in save bad frames mode */
#define	RFD_STS_FRAMESHORT	0x0080			/* Frame < 64 bytes, only in save bad frames mode */
#define	RFD_STS_TYPEORLEN	0x0020			/* 1 - "type frame", 0 - "length frame"	*/
#define	RFD_STS_RECVERR		0x0010			/* Receive error (only in save bad frames mode) */
#define	RFD_STS_NOADDRMATCH	0x0004			/* Address doesn't match anything (promiscous mode) */
#define	RFD_STS_NOMYADDR	0x0002			/* Doesn't match my address (multicast/broadcast/promisc.) */
#define	RFD_STS_RECVCOLL	0x0001			/* Collision during reception */


#define	RFD_CNT_ENDOFFRAME	0x8000			/* End-of-frame flag set by device (completed placing data. Must be set to 0 by s/w) */
#define	RFD_CNT_F			0x4000			/* acutal count updated (set by device). Must be set to 0 by s/w) */


/* EEPROM interface definitions */
#define	EESK	0x0001			/* Serial clock: low/high minimum 1 us, (low+high) minimum 4 us */
#define	EECS	0x0002			/* Chip select: 1 - enable EEPROM. Minumum 1 us between cycles */
#define	EEDI	0x0004			/* Data in: 1 - write to EEPROM */
#define	EEDO	0x0008			/* Data out: 1 - read from EEPROM */

/* MDI (PHY control) registers */
#define	MDI_CONTROL		0x0
#define	MDI_STATUS		0x1
#define	MDI_PHY_ID1		0x2
#define	MDI_PHY_ID2		0x3
#define	MDI_AN_ADVERTISE	0x4
#define	MDI_AN_LPA		0x5
#define	MDI_AN_EXPANSION	0x6

/* 82555/8/9-specific MDI registers */
#define	MDI5_CTRL_STATUS	0x10
#define	MDI5_SPEC_CONTROL1	0x11
#define	MDI5_CLOCKSYNTH_CTL	0x12
#define	MDI5_RX_FALSECARRIER_COUNT	0x13
#define	MDI5_RX_DIS_COUNT	0x14
#define	MDI5_RX_ERRFRAME_COUNT	0x15
#define	MDI5_RX_ERRSYMBOL_COUNT	0x16
#define	MDI5_RX_PREMEOFR_COUNT	0x17
#define	MDI5_RX_EOFR_COUNT	0x18
#define	MDI5_TX_JABBER_COUNT	0x19
#define	MDI5_EQUAL_CTRL_STATUS	0x1A
#define	MDI5_SPEC_CONTROL2	0x1B


