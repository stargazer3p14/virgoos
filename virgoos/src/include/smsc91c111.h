/*
 *	smsc91c111.h
 *
 * 	Header for SMSC91C111 LAN driver
 */

#ifndef SMSC91C111
#define SMSC91C111

#include "sosdef.h"

// I/O base
#define	SMSC91C111_BASE	0x10010000

// Default regs base
#define	SMSC91C111_REG_BASE	SMSC91C111_BASE /*0x10010300*/

//
// Registers
//

// Bank #0
#define	TRANSMIT_CTL	0x0

// Transmit control bits
#define	XMIT_CTL_SWFDUP	0x8000
#define	XMIT_CTL_EPH_LOOP	0x2000
#define	XMIT_CTL_STP_SQET	0x1000
#define	XMIT_CTL_FDUPLX	0x800
#define	XMIT_CTL_MON_CSN	0x400
#define	XMIT_CTL_NOCRC	0x100
#define	XMIT_CTL_PADEN	0x80
#define	XMIT_CTL_FORCOL	0x4
#define	XMIT_CTL_LOOP	0x2
#define	XMIT_CTL_TXEN	0x1

#define	EPH_STATUS	0x2

// Transmit status. This value is also available in status field
#define	EPH_STATUS_LNK_OK	0x4000
#define	EPH_STATUS_CTR_ROL	0x1000
#define	EPH_STATUS_EXC_DEF	0x800
#define	EPH_STATUS_LOST_CARR	0x400
#define	EPH_STATUS_LAT_COL	0x200
#define	EPH_STATUS_TX_DEFR	0x80
#define	EPH_STATUS_LTX_BRD	0x40
#define	EPH_STATUS_SQET		0x20
#define	EPH_STATUS_16COL	0x10
#define	EPH_STATUS_LTX_MULT	0x8
#define	EPH_STATUS_MUL_COL	0x4
#define	EPH_STATUS_SNGL_COL	0x2
#define	SPH_STATUS_TX_SUC	0x1

#define	RECV_CTL	0x4

// Receive control bits
#define	RECV_CTL_SOFT_RST	0x8000
#define	RECV_CTL_FILT_CAR	0x4000
#define	RECV_CTL_ABORT_ENB	0x2000
#define	RECV_CTL_STRIP_CRC	0x200
#define	RECV_CTL_RXEN	0x100
#define	RECV_CTL_ALMUL	0x4
#define	RECV_CTL_PRMS	0x2
#define	RECV_CTL_RX_ABORT	0x1

#define	COUNTER		0x6
#define	MEM_INFO	0x8
#define	RX_PHY_CTL	0xA

// RX PHY control bits
#define	RX_PHY_CTL_SPEED	0x2000
#define	RX_PHY_CTL_DPLX	0x1000
#define	RX_PHY_CTL_ANEG	0x800
#define	RX_PHY_CTL_LS2A	0x80
#define	RX_PHY_CTL_LS1A	0x40
#define	RX_PHY_CTL_LS0A	0x20
#define	RX_PHY_CTL_LS2B	0x10
#define	RX_PHY_CTL_LS1B	0x8
#define	RX_PHY_CTL_LS0B	0x4


// Bank #1
#define	CONFIG		0x0

// Configuration bits

#define	CONFIG_EPH_EN	0x8000
#define	CONFIG_NO_WAIT	0x1000
#define	CONFIG_GPCNTRL	0x400
#define	CONFIG_EXT_PHY	0x200

#define	BASE_ADDR	0x2
#define	INDIVID_ADDR	0x4		// 0x4..0x9 - eth. addr
#define	GEN_PURPOSE	0xA
#define	CONTROL		0xC

// Control bits
#define	CONTROL_RCV_BAD	0x4000
#define CONTROL_AUTO_RELEASE	0x800
#define CONTROL_LE_ENABLE	0x80
#define CONTROL_CR_ENABLE	0x40
#define CONTROL_TE_ENABLE	0x20
#define CONTROL_EEPROM_SELECT	0x4
#define CONTROL_RELOAD	0x2
#define CONTROL_STORE	0x1


// Bank #2
#define	MMU_CMD		0x0

// MMU command definitions
#define MMU_CMD_BUSY	0x1
#define	MMU_CMD_NOOP	0x0
#define	MMU_CMD_ALLOC_MEM_TX	0x20
#define MMU_CMD_RESET_MMU	0x40
#define MMU_CMD_REMOVE_FRAME_RX	0x60
#define MMU_CMD_REMOVE_REL_RX	0x80
#define MMU_CMD_RELEASE_SPECIFIC	0xA0
#define MMU_CMD_ENQ_PACKET_TX	0xC0
#define MMU_CMD_RESET_TX_FIFO	0xE0

#define	PACKET_NUM	0x2

#define	PACKET_NUM_ALLOC_FAILED	0x80

#define	FIFO_PORTS	0x4

// FIFO ports defitions
#define	FIFO_PORTS_REMPTY	0x8000
#define	FIFO_PORTS_TEMPTY	0x80

#define	POINTER		0x6

// Pointer definitions
#define	POINTER_RCV	0x8000
#define POINTER_AUTO_INCR	0x4000
#define POINTER_READ	0x2000
#define POINTER_NOT_EMPTY	0x800

#define	DATA		0x8		// 0x8..0xB
#define	INT_STATUS	0xC

// Interrupt status definitions
#define	INT_STATUS_MD_INT	0x80
#define	INT_STATUS_EPH_INT	0x20
#define INT_STATUS_RX_OVRN_INT	0x10
#define INT_STATUS_ALLOC_INT	0x8
#define INT_STATUS_TX_EMPTY_INT	0x4
#define INT_STATUS_TX_INT	0x2
#define INT_STATUS_RX_INT	0x1

// Bank #3
#define	MCAST_TABLE	0x0		// 0x0..0x7
#define	MII		0x8
#define	REVISION	0xA
#define	RCV		0xC

#define	RCV_DISCARD	0x80

// Bank #7
#define	EXT_REGS	0x0		// 0x0..0x7

// All banks
#define	BANK_SELECT	0xE

// Frame structure (the same format for receive and transmit)

struct frame
{
#define	RX_STATUS_ALGN_ERR	0x8000
#define	RX_STATUS_BROADCAST	0x4000
#define	RX_STATUS_BAD_CRC	0x2000
#define	RX_STATUS_ODD_FRM	0x1000
#define	RX_STATUS_TOO_LONG	0x800
#define	RX_STATUS_TOO_SHORT	0x400
#define	RX_STATUS_MULTICAST	0x1
	uint16_t	status;
	uint16_t	byte_count;
	uint8_t	data[1];
// Control byte definitions
#define	CTRL_BYTE_ODD	0x20
#define	CTRL_BYTE_CRC	0x10	// Transmit only
};

#define	SMSC91C111_IRQ	25

#define	NUM_RECV_BUFFERS	0x10
#define	NUM_XMIT_BUFFERS	0x10

// We need a separate structure, in order to have all bytes in a raw (91C111's 'frame' structure has last byte interleaved by control byte, if byte_count is odd)
struct smsc91c111_descr
{
	uint16_t	status;	// status, copied from/to frame
	uint16_t	count;	// byte_count, copied from/to frame
	uint8_t	buf[2048];	// data buffer
};


#endif // SMSC91C111

