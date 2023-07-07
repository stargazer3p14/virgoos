/*
 *	Header file for AMD 79970 ethernet driver
 *
 *	Reference: 
 *
 *	NOTES:
 *
 *	We are using 16-bit I/O mode, 32-bit addressing mode and SWSTYLE = 1
 */

#include "sosdef.h"
#include "inet.h"


/* 79970 vendor ID and device ID */
#define	VENDOR_ID	0x1022
#define	DEVICE_ID	0x2000

/*
 *	Configurable parameters
 */
#define	LOG2_NUM_RECV_BUFFERS	0x4
#define	NUM_RECV_BUFFERS	(1 << LOG2_NUM_RECV_BUFFERS)
#define	LOG2_NUM_XMIT_BUFFERS	0x4
#define	NUM_XMIT_BUFFERS	(1 << LOG2_NUM_XMIT_BUFFERS)

/* Recommended: at least 1518 byte, here 1792 bytes are allocated */
#define	RECV_BUFFER_SIZE	0x700
#define	XMIT_BUFFER_SIZE	0x700

/* I/O access registers (occupy 32 bytes of I/O space). We use only word I/O mode */
#define	APROM			0
#define	RDP				0x10
#define	RAP				0x12
#define	RESET			0x14
#define	BDP				0x16

/* Control and status registers. Accessed via Register Data Port (RDP) */
#define	CSR_STATUS					0
#define	CSR_INIT_BLOCK_ADDR0		1
#define	CSR_INIT_BLOCK_ADDR1		2
#define	CSR_INT_MASK				3
#define	CSR_TEST_FEATURE_CTL		4
#define	CSR_EXT_CTL_INT				5
#define	CSR_DESC_TBL_LEN			6
#define	CSR_LOGIC_ADDR_FILTER0		8
#define	CSR_LOGIC_ADDR_FILTER1		9
#define	CSR_LOGIC_ADDR_FILTER2		10
#define	CSR_LOGIC_ADDR_FILTER3		11
#define	CSR_PHYS_ADDR_REG0			12
#define	CSR_PHYS_ADDR_REG1			13
#define	CSR_PHYS_ADDR_REG2			14
#define	CSR_MODE					15
#define	CSR_INIT_BLOCK_ADDR_LOWER	16
#define	CSR_INIT_BLOCK_ADDR_UPPER	17
#define	CSR_CURR_RECV_ADDR_LOWER	18
#define	CSR_CURR_RECV_ADDR_UPPER	19
#define	CSR_CURR_XMIT_ADDR_LOWER	20
#define	CSR_CURR_XMIT_ADDR_UPPER	21
#define	CSR_NEXT_RECV_ADDR_LOWER	22
#define	CSR_NEXT_RECV_ADDR_UPPER	23
#define	CSR_RDR_BASE_ADDR_LOWER		24
#define	CSR_RDR_BASE_ADDR_UPPER		25
#define	CSR_NEXT_RD_ADDR_LOWER		26
#define	CSR_NEXT_RD_ADDR_UPPER		27
#define	CSR_CURR_RD_ADDR_LOWER		28
#define	CSR_CURR_RD_ADDR_UPPER		29
#define	CSR_TDR_BASE_ADDR_LOWER		30
#define	CSR_TDR_BASE_ADDR_UPPER		31
#define	CSR_NEXT_TD_ADDR_LOWER		32
#define	CSR_NEXT_TD_ADDR_UPPER		33
#define	CSR_CURR_TD_ADDR_LOWER		34
#define	CSR_CURR_TD_ADDR_UPPER		35
#define	CSR_NEXT2_RD_ADDR_LOWER		36
#define	CSR_NEXT2_RD_ADDR_UPPER		37
#define	CSR_NEXT2_TD_ADDR_LOWER		38
#define	CSR_NEXT2_TD_ADDR_UPPER		39
#define	CSR_CURR_RECV_BYTE_COUNT	40
#define	CSR_CURR_RECV_STATUS		41
#define	CSR_CURR_XMIT_BYTE_COUNT	42
#define	CSR_CURR_XMIT_STATUS		43
#define	CSR_NEXT_RECV_BYTE_COUNT	44
#define	CSR_NEXT_RECV_STATUS		45
#define	CSR_POLL_TIME_COUNTER		46
#define	CSR_POLL_INTERVAL			47
#define	CSR_SOFTWARE_STYLE			58
#define	CSR_PREV_TD_ADDR_LOWER		60
#define	CSR_PREV_TD_ADDR_UPPER		61
#define	CSR_PREV_XMIT_BYTE_COUNT	62
#define	CSR_PREV_XMIT_STATUS		63
#define	CSR_NEXT_XMIT_ADDR_LOWER	64
#define	CSR_NEXT_XMIT_ADDR_UPPER	65
#define	CSR_NEXT_XMIT_BYTE_COUNT	66
#define	CSR_NEXT_XMIT_STATUS		67
#define	CSR_RDR_COUNTER				72
#define	CSR_TDR_COUNTER				74
#define	CSR_RDR_LENGTH				76
#define	CSR_TDR_LENGTH				78
#define	CSR_DMA_COUNT_FIFO_WMARK	80
#define	CSR_BUS_ACTIVITY_TIMER		82
#define	CSR_DMA_ADDR_LOWER			84
#define	CSR_DMA_ADDR_UPPER			85
#define	CSR_BUFFER_BYTE_COUNTER		86
#define	CSR_CHIP_ID_LOWER			88
#define	CSR_CHIP_ID_UPPER			89
#define	CSR94						94
#define	CSR_BUS_TIMEOUT				100
#define	CSR_MISSED_FRAMES_COUNT		112
#define	CSR_RECV_COLLISION_COUNT	114
#define	CSR_ADV_FEATURE_CTL			122
#define	CSR_TEST_REG1				124

/* Bus control registers. Accessed via BCR Data Port (BDP) */
#define	BCR_MSRDA	0
#define	BCR_MSWRA	1
#define	BCR_MC		2
#define	BCR_LNKST	4
#define	BCR_LED1	5
#define	BCR_LED2	6
#define	BCR_LED3	7
#define	BCR_FDC		9
#define	BCR_IOBASEL	16
#define	BCR_IOBASEU	17
#define	BCR_BSBC	18
#define	BCR_EECAS	19
#define	BCR_SWS		20
#define	BCR_INTCON	21
#define	BCR_PCILAT	22

/* Interrupt status */
#define	CSR0_CMD_INIT		0x1			// INIT command
#define	CSR0_CMD_STRT		0x2			// START command
#define	CSR0_CMD_STOP		0x4			// STOP command (INIT is needed to restart)
#define	CSR0_CMD_TDMD		0x8			// Transmit poll demand
#define	CSR0_STATUS_TXON	0x10		// Transmit on indicator
#define	CSR0_STATUS_RXON	0x20		// Receive on indicator
#define	CSR0_IENA			0x40		// Interrupt generation enable
#define	CSR0_STATUS_INTR	0x80		// Main indicator that it's PCNet's interrupt
#define	CSR0_STATUS_IDON	0x100		// Initialization done
#define	CSR0_STATUS_TINT	0x200		// Transmit interrupt
#define	CSR0_STATUS_RINT	0x400		// Receive interrupt
#define	CSR0_STATUS_MERR	0x800		// Memory error
#define	CSR0_STATUS_MISS	0x1000		// Frame is missed
#define	CSR0_STATUS_CERR	0x2000		// Collision error
#define	CSR0_STATUS_BABL	0x4000		// Babble error
#define	CSR0_STATUS_ERR		0x8000		// Main indicator that an error occurred

// Initialization block
struct	am79970_init_block
{
	dword	mode;
	dword	padr00_31;
	dword	padr32_47;
	dword	laddr00_31;
	dword	laddr32_63;
	dword	rdra;
	dword	tdra;
} __attribute__ ((packed));

// 16-bit init block
struct	am79970_init_block_mode0
{
	word	mode;
	word	padr00_15;
	word	padr16_31;
	word	padr32_47;
	word	laddr00_15;
	word	laddr16_31;
	word	laddr32_47;
	word	laddr48_63;
	word	rdra00_15;
	word	rdra16_23_rlen;
	word	tdra00_15;
	word	tdra16_23_tlen;
} __attribute__ ((packed));

struct	am79970_recv_desc_mode0
{
	word	rmd0;
	word	rmd1;
	word	rmd2;
	word	rmd3;
} __attribute__ ((packed));

struct	am79970_xmit_desc_mode0
{
	word	tmd0;
	word	tmd1;
	word	tmd2;
	word	tmd3;
} __attribute__ ((packed));


struct	am79970_recv_desc
{
	dword	rmd0;
	dword	rmd1;
	dword	rmd2;
	dword	rmd3;
} __attribute__ ((packed));

struct	am79970_xmit_desc
{
	dword	tmd0;
	dword	tmd1;
	dword	tmd2;
	dword	tmd3;
} __attribute__ ((packed));

