/*
 *	Platform-dependent sosdef.h (x86)
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

#define	__int64	int	/*long long*/

//#define	INT_MAX	(int)0xFFFFFFFF
//#define	UINT_MAX	0xFFFFFFFF

// Hardware ports access macros
static byte	inb(word port)
{
	byte	res;

	__asm__ __volatile__ ("push %edx\n");
	__asm__ __volatile__ ("in	%%al, %%dx\n" : "=a"(res) : "d"(port) );
	__asm__ __volatile__ ("pop %edx\n");

	return	res;
}


static word	inw(word port)
{
	word	res;

	__asm__ __volatile__ ("push %edx\n");
	__asm__ __volatile__ ("in	%%ax, %%dx\n" : "=a"(res) : "d"(port) );
	__asm__ __volatile__ ("pop %edx\n");

	return	res;
}


static 	dword	ind(word port)
{
	dword res;

	__asm__ __volatile__ ("push %edx\n");
	__asm__ __volatile__ ("in	%%eax, %%dx\n" : "=a"(res) : "d"(port) );
	__asm__ __volatile__ ("pop %edx\n");

	return	res;
}


static 	void	outb(word port, byte val)
{
	__asm__ __volatile__ ("push %eax\n");
	__asm__ __volatile__ ("push %edx\n");
	__asm__ __volatile__ ("out %%dx, %%al\n" : : "d"(port), "a"(val) );
	__asm__ __volatile__ ("pop %edx\n");
	__asm__ __volatile__ ("pop %eax\n");
}


static 	void	outw(word port, word val)
{
	__asm__ __volatile__ ("push %eax\n");
	__asm__ __volatile__ ("push %edx\n");
	__asm__ __volatile__ ("out %%dx, %%ax\n" : : "d"(port), "a"(val) );
	__asm__ __volatile__ ("pop %edx\n");
	__asm__ __volatile__ ("pop %eax\n");

}


static 	void	outd(word port, dword val)
{
	__asm__ __volatile__ ("push %eax\n");
	__asm__ __volatile__ ("push %edx\n");
	__asm__ __volatile__ ("out %%dx, %%eax\n" : : "d"(port), "a"(val) );
	__asm__ __volatile__ ("pop %edx\n");
	__asm__ __volatile__ ("pop %eax\n");

}


static 	void	halt(void)
{
	__asm__("cli\n"
		"hlt\n");
}


static 	void	reboot(void)
{
	outb(0x64, 0xFE);
	halt();
}

#define	MAX_IRQS	0x20

#define	PIC_MASTER_PORT	0x20
#define	PIC_MASTER_MASK_PORT	0x21
#define	PIC_SLAVE_PORT	0xA0
#define	PIC_SLAVE_MASK_PORT	0xA1

#define	PIC_CMD_EOI	0x20

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
	__asm__ __volatile__ ("cli\n");	\
} while(0)


#define	enable_irqs() \
{	\
	__asm__ __volatile__ ("sti\n");	\
} while(0)


#define	restore_irq_state(intfl)\
do {\
	if (intfl)\
		__asm__ __volatile__ ("sti\n");\
	else\
		__asm__ __volatile__ ("cli\n");\
} while(0)

static	dword	get_irq_state(void)
{
	dword	intfl;

	__asm__ __volatile__ ("push %eax\n");
	__asm__ __volatile__ ("pushfd\n");
	__asm__ __volatile__ ("pop %%eax\n" : "=a"(intfl));
	__asm__ __volatile__ ("pop %eax\n");
	intfl = intfl >> 9 & 1;

	return	intfl;
}

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

#define	CMOS_ADDR	0x70
#define	CMOS_DATA	0x71

#define	CMOS_RTC_SECONDS	0
#define	CMOS_RTC_MINUTES	0x2
#define	CMOS_RTC_HOURS		0x4
#define	CMOS_RTC_DATE		0x7
#define	CMOS_RTC_MONTH		0x8
#define	CMOS_RTC_YEAR		0x9

// IRQ assignments
#define	TIMER_IRQ	0
#define	KBD_IRQ		1
#define	UART1_IRQ	4
#define	UART2_IRQ	3
#define	UART3_IRQ	4
#define	UART4_IRQ	3

// PCI interrupts are identified by PCI enum

#endif // SOSDEF__ARCH__H

