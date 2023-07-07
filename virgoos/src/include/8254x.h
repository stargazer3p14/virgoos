/*
 *	Header file for Intel 8254x ethernet driver
 */

#include "sosdef.h"

// Descriptors structures (receive and transmit descriptors)
struct recv_descr
{
	dword	buf_addr[2];
	word	length;
	word	checksum;
	byte	status;
	byte	errors;
	word	special;
};

struct xmit_descr
{
	dword	buf_addr[2];
	word	length;
	byte	chksum_offs;
	byte	cmd;
	byte	status;			// Bits 4-7 are reserved
	byte	chksum_start;
	word	special;
};

// Registers offsets
#define	I8254x_CTRL		0
#define	I8254x_STATUS	0x8
#define	I8254x_EECD		0x10
#define	I8254X_EERD		0x14
#define	I8254X_FLA		0x1C
#define	I8254X_CTRL_EXT	0x18
#define	I8254X_MDI_CTRL	0x20
#define	I8254X_FCAL		0x28
#define	I8254X_FCAH		0x2C
#define	I8254X_FCT		0x30
#define	I8254X_VET		0x38
#define	I8254X_FCTTV	0x170
#define	I8254X_TXCW		0x178
#define	I8254X_RXCW		0x180
#define	I8254X_LEDCTL	0xE00
#define	I8254X_PBA		0x1000
#define	I8254X_ICR		0xC0
#define	I8254X_ITR		0xC4
#define	I8254X_ICS		0xC8
#define	I8254X_IMS		0xD0
#define	I8254X_IMC		0xD8
#define	I8254X_RCTL		0x100
#define	I8254X_FCRTL	0x2160
#define	I8254X_FCRTH	0x2168
#define	I8254X_RDBAL	0x2800
#define	I8254X_RDBAH	0x2804
#define	I8254X_RDLEN	0x2808
#define	I8254X_RDH		0x2810
#define	I8254X_RDL		0x2818
#define	I8254X_RDTR		0x2820
#define	I8254X_RXDCTL	0x2828
#define	I8254X_RADV		0x282C
#define	I8254X_RSRPD	0x2C00
#define	I8254X_TCTL		0x400
#define	I8254X_TIPG		0x410
#define	I8254X_AIFS		0x458
#define	I8254X_TDBAL	0x3800
#define	I8254X_TDBAH	0x3804
#define	I8254X_TDLEN	0x3808
#define	I8254X_TDH		0x3810
#define	I8254X_TDT		0x3818
#define	I8254X_TIDV		0x3820
#define	I8254X_TXDMAC	0x3000
#define	I8254X_TXDCTL	0x3828
#define	I8254X_TADV		0x382C
#define	I8254X_TSPMT	0x3830
#define	I8254X_RXCSUM	0x5000



// PHY registers (accessed through I8254X_MDI_CTRL)
#define	I8254X_PHY_CTRL		0
#define	I8254X_PHY_STATUS	1
#define	I8254X_PHY_ID		2
#define	I8254X_PHY_EPID		3
#define	I8254X_PHY_ANA		4
#define	I8254X_PHY_LPA		5
#define	I8254X_PHY_ANE		6
#define	I8254X_PHY_NPT		7
#define	I8254X_PHY_LPN		8
#define	I8254X_PHY_GCON		9
#define	I8254X_PHY_GSTATUS	10
#define	I8254X_PHY_EPSTATUS	15
#define	I8254X_PHY_PSCON	16
#define	I8254X_PHY_PSSTAT	17
#define	I8254X_PHY_PINTE	18
#define	I8254X_PHY_PINTS	19
#define	I8254X_PHY_EPSCON1	20
#define	I8254X_PHY_PREC		21
#define	I8254X_PHY_EPSCON2	26
#define	I8254X_PHY_R30PS	29
#define	I8254X_PHY_R30AW	30

