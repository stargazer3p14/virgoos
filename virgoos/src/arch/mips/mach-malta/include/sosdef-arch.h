/*
 *	Platform-dependent sosdef.h (MIPS)
 */
#ifndef	SOSDEF__ARCH__H
 #define SOSDEF__ARCH__H

// Basic MIPS memory map
#define	KUSEG_BASE	0x0
#define	KSEG0_BASE	0x80000000
#define	KSEG1_BASE	0xA0000000
#define	KSEG2_BASE	0xC0000000

// CP0 definitions
#define	CP0_STATUS_IE	0x1			// Enable interrupts
#define	CP0_STATUS_IM2	(0x1<<10)
#define	CP0_STATUS_IM3	(0x1<<11)		// HW0-HW5  interrupts enable
#define	CP0_STATUS_IM4	(0x1<<12)
#define	CP0_STATUS_IM5	(0x1<<13)
#define	CP0_STATUS_IM6	(0x1<<14)
#define	CP0_STATUS_IM7	(0x1<<15)

#define	DEF_TASK_STATUS	(CP0_STATUS_IE | CP0_STATUS_IM2 | CP0_STATUS_IM3 | CP0_STATUS_IM4 | CP0_STATUS_IM5 | CP0_STATUS_IM6 | CP0_STATUS_IM7)


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

#define	__int64	long long

//#define	INT_MAX	(int)0xFFFFFFFF
//#define	UINT_MAX	0xFFFFFFFF

// On some occasions (e.g. interrupt acknowledgement) it will be necessary to interoperate with the GT-64120
//#define	GT64120_BASE	(KSEG1_BASE + 0x14000000)
#define	GT64120_BASE	(KSEG1_BASE + 0x1BE00000)
#define	GT64120_CPU_CONFIG	(GT64120_BASE + 0x0)
#define	GT64120_INTERNAL_DECODE	(GT64120_BASE + 0x68)
#define	GT64120_PCI0_CMD	(GT64120_BASE + 0xC00)
#define	GT64120_INT_CAUSE	(GT64120_BASE + 0xC18)
#define	GT64120_PCI0_IACK	(GT64120_BASE + 0xC34)		// R/O; any read acknowledges PCI INT

// Malta boards hardwire CPU to big-endian mode, so host and network values are the same
// But - we will need to cover endianness swap when accessing multi-byte I/O

#define	IO_START	(KSEG1_BASE + 0x18000000)		// Default PCI I/O space for Galileo GT-64120 system controller. Default size is 32M, which is by far
								// more than enough
#define	IO_PORT(n)	(IO_START + port)

// Probably these will become generic, and network and CPU-to-endian conversions all will optionally use them
#define	swap16(a) (a >> 8 & 0xFF | a << 8 & 0xFF00)
#define	swap32(a) (a >> 24 & 0xFF | a >> 8 & 0xFF00 | a << 8 & 0xFF0000 | a << 24 & 0xFF000000)

#define	cpu_to_le32(a)	swap32(a)
#define	le32_to_cpu	cpu_to_le32
#define	cpu_to_le16(a)	swap16(a)
#define	le16_to_cpu	cpu_to_le16
#define	cpu_to_be32(a)	(a)
#define	be32_to_cpu	cpu_to_be32
#define	cpu_to_be16(a)	(a)
#define	be16_to_cpu	cpu_to_be16

static 	unsigned short	htons(unsigned short n)
{
	return	n;
}

#define	ntohs	htons

static 	unsigned long	htonl(unsigned long n)
{
	return	n;
}

#define	ntohl	htonl

//
// MIPS docs say that software never needs to swap data to fit CPU's big-endian view to bus'es (PCI, ISA) little-endian: system controller hardware will do it
//

static byte	inb(dword port)
{
	return	(byte)*(volatile byte*)IO_PORT(port);
}

static word	inw(dword port)
{
	return	(word)(*(volatile word*)IO_PORT(port));
}

static dword ind(dword port)
{
	return	(dword)(*(volatile dword*)IO_PORT(port));
}

static void	outb(dword port, byte val)
{
	*(volatile byte*)IO_PORT(port) = val;
}

static void	outw(dword port, word val)
{
	*(volatile word*)IO_PORT(port) = (val);
}

static void	outd(dword port, dword val)
{
	*(volatile dword*)IO_PORT(port) = (val);
}

//
// IRQ assignments
//
#define	MAX_IRQS	0x20

// MIPS Malta board has the same IRQ assignments as regular PC, due to 82371 PIIX South Bridge peripheral controller. PC I/O space is mapped at 0x18000000 (+KSEG1_BASE)
// There is the same ports assignment and we may use the same outXX()/inXX() setup as in PC
//
#define	TIMER_IRQ	0
#define	KBD_IRQ		1
#define	UART1_IRQ	4
#define	UART2_IRQ	3
#define	UART3_IRQ	4
#define	UART4_IRQ	3

#define	CMOS_ADDR	0x70
#define	CMOS_DATA	0x71

#define	CMOS_RTC_SECONDS	0
#define	CMOS_RTC_MINUTES	0x2
#define	CMOS_RTC_HOURS		0x4
#define	CMOS_RTC_DATE		0x7
#define	CMOS_RTC_MONTH		0x8
#define	CMOS_RTC_YEAR		0x9

#define	PIC_MASTER_PORT	0x20
#define	PIC_MASTER_MASK_PORT	0x21
#define	PIC_SLAVE_PORT	0xA0
#define	PIC_SLAVE_MASK_PORT	0xA1

// PIC init commands (Initialization Command Words).
#define	PIC_CMD_ICW1	0x11	// ICW4 needed, cascading 8259h,
					// 8-byte vectors, edge-triggered mode
#define	PIC_CMD_ICW2	0x0	// bits 3-7 = A3-A7 int vector.
#define	PIC_CMD_ICW3	0x04	// IRQ 2 has slave, rest - no slave.
#define	PIC_CMD_SLAVE_ICW3	0x02	// Bits 0-2 = master's cascade vector.
#define	PIC_CMD_ICW4	0x1	// Normal EOI, no buffering,
					// sequential mode.
#define	PIC_CMD_EOI	0x20

#define	PIC0_BASE_INT	0x20
#define	PIC1_BASE_INT	0x28

static void	mask_irq(dword irq)
{
	if (irq < 15)
	{
		if (irq < 8)
			outb(PIC_MASTER_MASK_PORT, ~((1 << irq) - 1));
		else
			outb(PIC_SLAVE_MASK_PORT, ~((1 << irq - 8) - 1));
	}
}


static dword	save_irq_mask(dword *irq_mask)
{
	*irq_mask = (dword)inb(PIC_MASTER_MASK_PORT) | (dword)inb(PIC_SLAVE_MASK_PORT) << 8;
	return	*irq_mask;
}

#define	restore_irq_mask(irq_mask)\
do\
{\
	outb(PIC_MASTER_MASK_PORT, irq_mask & 0xFF);\
	outb(PIC_SLAVE_MASK_PORT, irq_mask >> 8 & 0xFF);\
} while(0)


#define	disable_irqs() \
do {	\
	__asm__ __volatile__ ("\tdi\n");	\
} while(0)


#define	enable_irqs() \
{	\
	__asm__ __volatile__ ("\tei\n");	\
} while(0)


#define	restore_irq_state(irq_state)\
do {\
	__asm__ __volatile__ ("\tmfc0 $8, $12, 0\n");	\
	__asm__ __volatile__ ("\tand $8, $8, %0\n" : : "r"(~1UL));	\
	__asm__ __volatile__ ("\tor $8, $8, %0\n" : : "r"(irq_state & 0x1));	\
	__asm__ __volatile__ ("\tmtc0 $8, $12, 0\n");	\
} while(0)

static	dword	get_irq_state(void)
{
	dword	status = 0;

	__asm__ __volatile__ ("mfc0 %0, $12, 0\n" : "=r" (status));
	return	status & 0x1;
}

#define	GEN_EXC_ADDR	(KSEG0_BASE + 0x180)

#endif	// SOSDEF__ARCH__H

