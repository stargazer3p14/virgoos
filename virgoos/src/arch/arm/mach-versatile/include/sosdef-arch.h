/*
 *	Platform-dependent sosdef.h (ARM)
 */
#ifndef	SOSDEF__ARCH__H
 #define SOSDEF__ARCH__H

typedef	unsigned long	dword;
typedef	unsigned short	word;
typedef	unsigned char	byte;

//typedef unsigned	size_t;
typedef	int	ssize_t;
typedef	unsigned	off_t;
typedef	unsigned	dev_t;
typedef	unsigned	mode_t;
typedef	unsigned	time_t;
#define	intmax_t	unsigned long
#define	ptrdiff_t	unsigned long
typedef	unsigned	ino_t;
typedef	unsigned	pid_t;
typedef unsigned long	uintptr_t;

typedef	unsigned char	uint8_t;
typedef	signed char	int8_t;
typedef	unsigned short	uint16_t;
typedef	signed short	int16_t;
typedef	unsigned long	uint32_t;
typedef	signed long	int32_t;
typedef	unsigned long long	uint64_t;
typedef	signed long long	int64_t;

#define	__int64	int	//long long

//#define	INT_MAX	(int)0xFFFFFFFF
//#define	UINT_MAX	0xFFFFFFFF

// Probably these will become generic, and network and CPU-to-endian conversions all will optionally use them
#define	swap16(a) (a >> 8 & 0xFF | a << 8 & 0xFF00)
#define	swap32(a) (a >> 24 & 0xFF | a >> 8 & 0xFF00 | a << 8 & 0xFF0000 | a << 24 & 0xFF000000)

#define	cpu_to_be32(a)	swap32(a)
#define	be32_to_cpu	cpu_to_be32
#define	cpu_to_be16(a)	swap16(a)
#define	be16_to_cpu	cpu_to_be16
#define	cpu_to_le32(a)	(a)
#define	le32_to_cpu	cpu_to_le32
#define	cpu_to_le16(a)	(a)
#define	le16_to_cpu	cpu_to_le16

static 	unsigned short	htons(unsigned short n)
{
	return	(n >> 8) | (n << 8);
}

#define	ntohs	htons

static 	unsigned long	htonl(unsigned long n)
{
	return	(n >> 24) | (n << 24) | (n >> 8 & 0x0000FF00) | (n << 8 & 0x00FF0000);
}

#define	ntohl	htonl

static byte	inb(dword port)
{
	return	*(volatile byte*)port;
}

static word	inw(dword port)
{
	return	*(volatile word*)port;
}

static dword ind(dword port)
{
	return	*(volatile dword*)port;
}

static void	outb(dword port, byte val)
{
	*(volatile byte*)port = val;
}

static void	outw(dword port, word val)
{
	*(volatile word*)port = val;
}

static void	outd(dword port, dword val)
{
	*(volatile dword*)port = val;
}

// VIC definitions
#define	MAX_IRQS	0x20

// VIC manual suggests placing this to 0xFFFFF000, so that registers can be read with a single instruction (with negative 12-bit offset).
// But dev. CPU's manual suggests that is is hardcoded; so let it be
#define	VIC_BASE	0x10140000

// Register offsets
#define	VICIRQSTATUS	0x0
#define	VICFIQSTATUS	0x4
#define	VICRAWINTR	0x8
#define	VICINTSELECT	0xC
#define VICINTENABLE	0x10
#define	VICINTENCLEAR	0x14
#define	VICSOFTINT	0x18
#define	VICSOFTINTCLEAR	0x1C
#define	VICPROTECTION	0x20
#define	VICVECTADDR	0x30
#define	VICDEFVECTADDR	0x34
#define	VICVECTADDR0	0x100
#define	VICVECTADDR1	0x104
#define	VICVECTADDR2	0x108
#define	VICVECTADDR3	0x10C
#define	VICVECTADDR4	0x110
#define	VICVECTADDR5	0x114
#define	VICVECTADDR6	0x118
#define	VICVECTADDR7	0x11C
#define	VICVECTADDR8	0x120
#define	VICVECTADDR9	0x124
#define	VICVECTADDR10	0x128
#define	VICVECTADDR11	0x12C
#define	VICVECTADDR12	0x130
#define	VICVECTADDR13	0x134
#define	VICVECTADDR14	0x138
#define	VICVECTADDR15	0x13C
#define	VICVECTCNTL0	0x200
#define	VICVECTCNTL1	0x204
#define	VICVECTCNTL2	0x208
#define	VICVECTCNTL3	0x20C
#define	VICVECTCNTL4	0x210
#define	VICVECTCNTL5	0x214
#define	VICVECTCNTL6	0x218
#define	VICVECTCNTL7	0x21C
#define	VICVECTCNTL8	0x220
#define	VICVECTCNTL9	0x224
#define	VICVECTCNTL10	0x228
#define	VICVECTCNTL11	0x22C
#define	VICVECTCNTL12	0x230
#define	VICVECTCNTL13	0x234
#define	VICVECTCNTL14	0x238
#define	VICVECTCNTL15	0x23C
#define	VICPERIPHID0	0xFE0
#define	VICPERIPHID1	0xFE4
#define	VICPERIPHID2	0xFE8
#define	VICPERIPHID3	0xFEC
#define	VICPCELLID0	0xFF0
#define	VICPCELLID1	0xFF4
#define	VICPCELLID2	0xFF8
#define	VICPCELLID3	0xFFC

// IRQ assignments
#define	WATCHDOG_IRQ	0
#define	LOW_SW_IRQ	1
#define	COMMS_RX_IRQ	2
#define	COMMS_TX_IRQ	3
#define	TIMER0_IRQ	4
#define	TIMER2_IRQ	5
#define	GPIO0_IRQ	6
#define	GPIO1_IRQ	7
#define	GPIO2_IRQ	8
#define	GPIO3_IRQ	9
#define	RTC_IRQ		10
#define	SSP_IRQ		11
#define	UART0_IRQ	12
#define	UART1_IRQ	13
#define	UART2_IRQ	14
#define	SCI_IRQ		15
#define	CLCDC_IRQ	16
#define	DMA_IRQ		17
#define	PWRFAIL_IRQ	18
#define	MBX_IRQ		19
#define	LOW_IRQ		20
// IRQ 24-31 may come from a second (cascaded) int controller. What do IRQs 21-23 do?
#define	VICINTSRC_IRQ	21

// SIC definitions (secondary interrupt controller)
#define	SIC_STATUS	0x10003000
#define	SIC_RAWSTAT	0x10003004
#define	SIC_ENABLE	0x10003008
#define	SIC_ENSET	0x10003008
#define	SIC_ENCLR	0x1000300C
#define	SIC_SOFTINTSET	0x10003010
#define	SIC_SOFTINTSCLR	0x10003014
#define	SIC_PICENABLE	0x10003020
#define	SIC_PICENSET	0x10003020
#define	SIC_PICENCLR	0x10003024


#define	KBD_IRQ		100		// Just to compile


// ARM CPRS (relevant fields)
#define	CPRS_CF	0x20000000
#define	CPRS_ZF	0x40000000
#define	CPRS_SF	0x80000000
#define	CPRS_OF	0x10000000
#define	CPRS_A	0x100				// Disable data abort interrupts
#define	CPRS_I	0x80				// Disable IRQs
#define	CPRS_F	0x40				// Disable FIQs

// ARM mode mask and valid modes
// After reset we're in Supervisor mode and will remain in such
#define	CPRS_MODE_MASK	0x1F
#define	CPRS_MODE_USER	0x10
#define	CPRS_MODE_FIQ	0x11
#define	CPRS_MODE_IRQ	0x12
#define	CPRS_MODE_SUP	0x13
#define	CPRS_MODE_ABORT	0x17
#define	CPRS_MODE_UD	0x1B
#define	CPRS_MODE_SYS	0x1F

static void	mask_irq(dword irq)
{
	outd(VIC_BASE + VICINTENCLEAR, 1 << irq);
}


static dword	save_irq_mask(dword *irq_mask)
{
	*irq_mask = ind(VIC_BASE + VICINTENABLE);
	return	*irq_mask;
}

#define	restore_irq_mask(irq_mask)\
do\
{\
} while(0)


#define	disable_irqs() \
do {	\
	__asm__ __volatile__ ("\tmrs r0, CPSR\n"	\
				"\torr r0, r0, #0xC0\n"	\
				"\tmsr CPSR, r0\n");	\
} while(0)


#define	enable_irqs() \
{	\
	__asm__ __volatile__ ("\tmrs r0, CPSR\n"	\
				"\tmvn r1, #0xC0\n"	\
				"\tand r0, r0, r1\n"	\
				"\tmsr CPSR, r0\n");	\
} while(0)


#define	restore_irq_state(irq_state)\
do {\
	__asm__ __volatile__ ("mrs r1, CPSR\n"	\
				"\tmvn r2, #0xC0\n"	\
				"\tand r1, r1, r2\n");	\
	__asm__ __volatile__ ("and %%r0, %0, #0xC0\n" : : "r"(irq_state));	\
	__asm__ __volatile__ ("orr r1, r1, r0\n");	\
	__asm__ __volatile__ ("msr CPSR, r1\n");	\
} while(0)

static	dword	get_irq_state(void)
{
	dword	cpsr = 0;

	__asm__ __volatile__ ("\tmrs r0, CPSR\n"
				"\t and %0, %%r0, #0xC0\n" : "=r"(cpsr) );
	return	cpsr;
}

#endif	// SOSDEF__ARCH__H

