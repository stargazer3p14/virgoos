/*
 *	Header file for DM646x EMAC
 *
 *	Reference: 
 *
 *	NOTES:
 */

#ifndef	DM646x_EMAC__H
#define	DM646x_EMAC__H

#include "sosdef.h"
#include "inet.h"


/* Base addresses - they may eventually move to platform-specific header if there are DM646x devices with mapping different from DM6467. Below values are for DM6467 */
#define	DM646x_EMAC_BASE	0x01C80000

// EMAC Module Registers
#define	EMACTXIDVER	0x0
#define	EMACTXCONTROL	0x4

// TXCONTROL fields
#define	TXEN	0x1

#define	EMACTXTEARDOWN	0x8
#define	EMACRXIDVER	0x10
#define	EMACRXCONTROL	0x14

// RXCONTROL fields
#define	RXEN	0x1

#define	EMACRXTEARDOWN	0x18
#define	EMACTXINTSTATRAW	0x80
#define	EMACTXINTSTATMASKED	0x84
#define	EMACTXINTMASKSET	0x88
#define	EMACTXINTMASKCLEAR	0x8C
#define	EMACINTVECTOR	0x90
#define	EMACEOIVECTOR	0x94

// EOI vector fields
#define	EOI_RXTHRESH	0
#define	EOI_RXPULSE	1
#define	EOI_TXPULSE	2
#define	EOI_MISC	3

#define	EMACRXINTSTATRAW	0xA0
#define	EMACRXINTSTATMASKED	0xA4
#define	EMACRXINTMASKSET	0xA8
#define	EMACRXINTMASKCLEAR	0xAC
#define	EMACINTSTATRAW	0xB0
#define	EMACINTSTATMASKED	0xB4
#define	EMACINTMASKSET	0xB8
#define	EMACINTMASKCLEAR	0xBC

// INTMASKSET/INTMASKCLEAR fields
#define	STATMASK	0x1
#define	HOSTMASK	0x2

#define	EMACRXMBPENABLE	0x100

// Multicast/broadcast/promiscous control bits (enable)
#define	RXMULTEN	0x00000020				// Rx multicast enable
#define	RXBROADEN	0x00002000				// Rx broadcast enable
#define	RXCAFEN		0x00200000				// Rx "copy-all-frames" (promiscous mode) enable
#define	RXCEFEN		0x00400000				// Rx copy erratic frames enable
#define	RXCSFEN		0x00800000				// Rx copy short (<64 bytes) frames enable
#define	RXCMFEN		0x01000000				// Rx copy MAC control frames enable
#define	RXNOCHAIN	0x10000000				// Receive frames don't span multiple buffers. This is our policy, and this bit ensures it
#define	RXQOSEN		0x20000000				// Receive QOS frames
#define	RXPASSCRC	0x40000000				// Pass CRC along with the frame

#define	EMACUNICASTSET	0x104
#define	EMACUNICASTCLEAR	0x108
#define	EMACRXMAXLEN	0x10C
#define	EMACRXBUFFEROFFSET	0x110
#define	EMACRXFILTERLOWTHRESH	0x114
#define	EMACRX0FLOWTHRESH	0x120				// Rx flow control threshold for channel 0 (one reg. per channel with offest 4 freom previous)
#define	EMACRX0FREEBUFFER	0x140				// Rx free buffer count for channel 0 (one reg. per channel with offest 4 freom previous)
#define	EMACCONTROL	0x160

// MAC control bits
#define	FULLDUPLEX	0x1
#define	LOOPBACK	0x2
#define	RXBUFFERFLOWEN	0x8
#define	TXFLOWEN	0x10
#define	GMIIEN		0x20
#define	TXPACE		0x40
#define	GIG		0x80					// Enable gigabit mode (0 - 10/100Mbs mode only)
#define	TXPTYPE		0x200					// Insignificant, we use only channel 0
#define	CMDIDLE		0x800
#define	RXFIFOFLOWEN	0x1000
#define	RXOWNERSHIP	0x2000					// Setting of this bit is what EMAC writes to OWN fields in descriptors
#define	RXOFFLENBLOCK	0x4000
#define	GIGFORCE	0x20000

#define	EMACSTATUS	0x164
#define	EMACEMCONTROL	0x168
#define	EMACFIFOCONTROL	0x16C
#define	EMACCONFIG	0x170
#define	EMACSOFTRESET	0x174
#define	EMACSRCADDRLO	0x1D0

// MACADDR flags
#define	MACADDR_VALID	0x00100000
#define	MACADDR_FILT	0x00080000

#define	EMACSRCADDRHI	0x1D4
#define	EMACHASH1	0x1D8
#define	EMACHASH2	0x1DC
#define	EMACBOFFTEST	0x1E0
#define	EMACTPACETEST	0x1E4
#define	EMACRXPAUSE	0x1E8
#define	EMACTXPAUSE	0x1EC
#define	EMACADDRLO	0x500					// Used in receive address matching
#define	EMACADDRHI	0x504					// Used in receive address matching
#define	EMACINDEX	0x508
#define	EMACTX0HDP	0x600					// Tx head descriptor pointer for channel 0 (one reg. per channel with offest 4 freom previous)
#define	EMACRX0HDP	0x620					// Rx head descriptor pointer for channel 0 (one reg. per channel with offest 4 freom previous)
#define	EMACTX0CP	0x640					// Tx completion pointer for channel 0 (one reg. per channel with offest 4 freom previous)
#define	EMACRX0CP	0x660					// Rx completion pointer for channel 0 (one reg. per channel with offest 4 freom previous)

// If/when needed, define network statistics (staring from offset 0x200) here
#define	EMACRXGOODFRAMES	0x200
#define	EMACRXBCASTFRAMES		0x204
#define	EMACRXMCASTFRAMES		0x208
#define	EMACRXPAUSEFRAMES	0x20C
#define	EMACRXCRCERRORS		0x210
#define	EMACRXALIGNCODEERRORS	0x214
#define	EMACRXOVERSIZED		0x218
#define	EMACRXJABBER	0x21C
#define	EMACRXUNDERSIZED	0x220
#define	EMACRXFRAGMENTS	0x224
#define	EMACRXFILTERED	0x228
#define	EMACRXQOSFILTERED	0x22C
#define	EMACRXOCTETS	0x230
#define	EMACTXGOODFRAMES	0x234
#define	EMACTXBCASTFRAMES	0x238
#define	EMACTXMCASTFRAMES	0x23C
#define	EMACTXPAUSEFRAMES	0x240
#define	EMACTXDEFERRED	0x244
#define	EMACTXCOLLISION	0x248
#define	EMACTXSINGLECOLL	0x24C
#define	EMACTXMULTICOLL	0x250
#define	EMACTXEXCESSIVECOLL	0x254
#define	EMACTXLATECOLL	0x258
#define	EMACTXUNDERRUN	0x25C
#define	EMACTXCARRIERSENSE	0x260
#define	EMACTXOCTETS	0x264
#define	EMACFRAME64	0x268
#define	EMACFRAME65T127	0x26C
#define	EMACFRAME128T255	0x270
#define	EMACFRAME256T511	0x274
#define	EMACFRAME512T1023	0x278
#define	EMACFRAME1024TUP	0x27C
#define	EMACNETOCTETS	0x280
#define	EMACRXSOFOVERRUNS	0x284
#define	EMACRXMOFOVERRUNS	0x288
#define	EMACRXDMAOVERRUNS	0x28C

#define	DM646x_EMAC_CTRL_BASE	0x01C81000

// EMAC Control Module Registers
#define	CMIDVER	0x0
#define	CMSOFTRESET	0x4
#define	CMEMCONTROL	0x8
#define	CMINTCTRL	0xC
#define	CMRXTHRESHINTEN	0x10
#define	CMRXINTEN	0x14
#define	CMTXINTEN	0x18
#define	CMMISCINTEN	0x1C
#define	CMRXTHRESHINTSTAT	0x40
#define	CMRXINTSTAT	0x44
#define	CMTXINTSTAT	0x48
#define	CMMISCINTSTAT	0x4C

// Misc interrupts
#define	USERINT		0x1
#define	LINKINT		0x2
#define	HOSTINT		0x4
#define	STATINT		0x8

#define	CMRXINTMAX	0x70
#define	CMTXINTMAX	0x74

#define	DM646x_RAM_BASE		0x01C82000

#define	DM646x_MDIO_BASE	0x01C84000

// MDIO registers
#define	MDIOVERSION	0
#define	MDIOCONTROL	0x4

// MDIOCONTROL definitions
#define	IDLE	0x80000000
#define	ENABLE	0x40000000
#define	PREAMBLEDIS	0x00100000
#define	FAULT	0x00080000
#define	FAULTENB	0x00040000

#define	MDIOALIVE	0x8
#define	MDIOLINK	0xC
#define	MDIOLINKINTRAW	0x10
#define	MDIOLINKINTMASKED	0x14
#define	MDIOUSERINTRAW	0x20
#define	MDIOUSERINTMASKED	0x24
#define	MDIOUSERINTMASKSET	0x28
#define	MDIOUSERINTMASKCLEAR	0x2C
#define	MDIOUSERACCESS0	0x80
#define	MDIOUSERPHYSEL0	0x84
#define	MDIOUSERACCESS1	0x88
#define	MDIOUSERPHYSEL1	0x8C

// MDIOUSERPHYSELx definitions
#define	LINKINTENB	0x40

/*
 *	Configurable parameters
 */
#define	NUM_RECV_BUFFERS	4
#define	NUM_XMIT_BUFFERS	4 

// This constant can't be redefined
#define	MAX_NUM_DESCRIPTORS	512

#define	USE_EMAC_RAM

#ifdef USE_EMAC_RAM
#if NUM_RECV_BUFFERS + NUM_XMIT_BUFFERS > MAX_NUM_DESCRIPTORS
 #error	For Dm646x EMAC maximum number of descriptors must not exceed 512
#endif
#endif

/* Recommended: at least 1518 byte, here 1792 bytes are allocated */
#define	RECV_BUFFER_SIZE	0x700
#define	XMIT_BUFFER_SIZE	0x700

struct dm646x_emac_descr
{
	uint32_t	next;
	uint32_t	buf;
	uint16_t	buf_len;
	uint16_t	buf_offs;
	uint16_t	pkt_len;
	uint16_t	flags;
} __attribute__ ((packed));

/* Packet flags */
#define	SOP	0x8000
#define	EOP	0x4000
#define	OWNER	0x2000
#define	EOQ	0x1000
#define	TDOWNCMPLT	0x0800
#define	PASSCRC	0x0400
#define	JABBER	0x0200
#define	OVERSIZE	0x0100
#define	FRAGMENT	0x0080
#define	UNDERSIZE	0x0040
#define	CONTROL	0x0020
#define	OVERRUN	0x0010
#define	CODEERR	0x0008
#define	ALIGNERR	0x0004
#define	CRCERR	0x0002
#define	NOMATCH	0x0001

// Different definitions
#define	DM646x_EMAC_NUM_CHANNELS	8

// Interrupts
#define	EMAC_RXTH_IRQ	24
#define	EMAC_RX_IRQ	25
#define	EMAC_TX_IRQ	26
#define	EMAC_MISC_IRQ	27


// MDIO definitions -- TODO: move them to separate generic PHY management module/header
#define	MAX_PHY	0x20

#endif	// DM646x_EMAC__H

