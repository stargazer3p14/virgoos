/*
 *	Platform-dependent sosdef.h (ARM - evmdm6467)
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

static uint8_t	inb(uint32_t port)
{
	return	*(volatile uint8_t*)port;
}

static uint16_t	inw(uint32_t port)
{
	return	*(volatile uint16_t*)port;
}

static dword ind(uint32_t port)
{
	return	*(volatile uint32_t*)port;
}

static void	outb(uint32_t port, uint8_t val)
{
	*(volatile uint8_t*)port = val;
}

static void	outw(uint32_t port, uint16_t val)
{
	*(volatile uint16_t*)port = val;
}

static void	outd(uint32_t port, uint32_t val)
{
	*(volatile uint32_t*)port = val;
}

/*
 *	AINTC (ARM interrupt controller) of DM6467 definitions
 */
#define	MAX_IRQS	64

#define	AINTC_BASE	0x01C48000

// Register offsets
#define	AINTCFIQ0	0x0
#define	AINTCFIQ1	0x4
#define	AINTCIRQ0	0x8
#define	AINTCIRQ1	0xC
#define	AINTCFIQENTRY	0x10
#define	AINTCIRQENTRY	0x14
#define	AINTCEINT0	0x18
#define	AINTCEINT1	0x1C
#define	AINTCINTCTL	0x20
#define	AINTCEBASE	0x24
#define	AINTCINTPRI0	0x30
#define	AINTCINTPRI1	0x34
#define	AINTCINTPRI2	0x38
#define	AINTCINTPRI3	0x3C
#define	AINTCINTPRI4	0x40
#define	AINTCINTPRI5	0x44
#define	AINTCINTPRI6	0x48
#define	AINTCINTPRI7	0x4C

// IRQ assignments
#define	VP_VERTINT0_FIQ	0		// int #0 is unconditionally FIQ - due to DM646x ARM Subsystem Reference Guide (spruep9d) [what for?]
#define	VP_VERTINT1_IRQ	1
#define	VP_VERTINT2_IRQ	2
#define	VP_VERTINT3_IRQ	3
#define	VP_ERRINT_IRQ	4
#define	WDINT_IRQ	7
#define	CRGENINT0_IRQ	8
#define	CRGENINT1_IRQ	9
#define	TSINT0_IRQ	10
#define	TSINT1_IRQ	11
#define	VDCEINT_IRQ	12
#define	USBINT_IRQ	13
#define	USBDMAINT_IRQ	14
#define	PCIINT_IRQ	15
#define	CCINT0_IRQ	16
#define	CCERRINT_IRQ	17
#define	TCERRINT0_IRQ	18
#define	TCERRINT1_IRQ	19
#define	TCERRINT2_IRQ	20
#define	TCERRINT3_IRQ	21
#define	IDEINT_IRQ	22
#define	HPIINT_IRQ	23
#define	MAC_RXTH_IRQ	24
#define	MAC_RX_IRQ	25
#define	MAC_TX_IRQ	26
#define	MAC_MISC_IRQ	27
#define	AXINT0_IRQ	28
#define	ARINT0_IRQ	29
#define	AXINT1_IRQ	30
#define	TINTL0_IRQ	32
#define	TINTH0_IRQ	33
#define	TINTL1_IRQ	34
#define	TINTH1_IRQ	35
#define	PWMINT0_IRQ	36
#define	PWMINT1_IRQ	37
#define	VLQINT_IRQ	38
#define	I2CINT_IRQ	39
#define	UARTINT0_IRQ	40
#define	UARTINT1_IRQ	41
#define	UARTINT2_IRQ	42
#define	SPINT0_IRQ	43
#define	SPINT1_IRQ	44
#define	DSP2ARM0_IRQ	45
#define	PSCINT_IRQ	47
#define	GPIO0_IRQ	48
#define	GPIO1_IRQ	49
#define	GPIO2_IRQ	50
#define	GPIO3_IRQ	51
#define	GPIO4_IRQ	52
#define	GPIO5_IRQ	53
#define	GPIO6_IRQ	54
#define	GPIO7_IRQ	55
#define	GPIOBNK0_IRQ	56
#define	GPIOBNK1_IRQ	57
#define	GPIOBNK2_IRQ	58
#define	DDRINT_IRQ	59
#define	EMIFAINT_IRQ	60
#define	COMMTX_IRQ	61
#define	COMMRX_IRQ	62
#define	EMUINT_IRQ	63


/*
 *	Power and Sleep Controller (PSC) definitions
 */
#define	PSC_BASE	0x01C41000

// Register offsets
#define	PSCPID		0x0
#define	PSCINTEVAL	0x18
#define	PSCMERRPR0	0x40
#define	PSCMERRPR1	0x44
#define	PSCMERRCR0	0x50
#define	PSCMERRCR1	0x54
#define	PSCPTCMD	0x120
#define	PSCPTSTAT	0x128
#define	PSCPDSTAT0	0x200
#define	PSCPDCTL0	0x300
#define	PSCMDSTAT0	0x800
#define	PSCMDCTL0	0xA00

// PSC Modules definitions
#define	PSC_MOD_ARM	0
#define	PSC_MOD_C64X	1
#define	PSC_MOD_HDVICP0	2
#define	PSC_MOD_HDVICP1	3
#define	PSC_MOD_EDMA3CC	4
#define	PSC_MOD_EDMA3TC0	5
#define	PSC_MOD_EDMA3TC1	6
#define	PSC_MOD_EDMA3TC2	7
#define	PSC_MOD_EDMA3TC3	8
#define	PSC_MOD_USB20	9
#define	PSC_MOD_ATA	10
#define	PSC_MOD_VLYNQ	11
#define	PSC_MOD_HPI	12
#define	PSC_MOD_PCI	13
#define	PSC_MOD_EMAC_MDIO	14
#define	PSC_MOD_VDCE	15
#define	PSC_MOD_VIDEOPORT_LPSC1	16
#define	PSC_MOD_VIDEOPORT_LPSC2	17
#define	PSC_MOD_TSIF0	18
#define	PSC_MOD_TSIF1	19
#define	PSC_MOD_DDR2_CTRLR	20
#define	PSC_MOD_EMIFA	21
#define	PSC_MOD_MCASP0	22
#define	PSC_MOD_MCASP1	23
#define	PSC_MOD_CRGEN0	24
#define	PSC_MOD_CRGEN1	25
#define	PSC_MOD_UART0	26
#define	PSC_MOD_UART1	27
#define	PSC_MOD_UART2	28
#define	PSC_MOD_PWM0	29
#define	PSC_MOD_PWM1	30
#define	PSC_MOD_I2C	31
#define	PSC_MOD_SPI	32
#define	PSC_MOD_GPIO	33
#define	PSC_MOD_TIMER0	34
#define	PSC_MOD_TIMER1	35
#define	PSC_MOD_AINTC	45

#define	PSC_NUM_MODULES	46


/*
 *	PLL controller definitions
 */
#define	PLL1_BASE	0x01C40800
#define	PLL2_BASE	0x01C40C00

// Register offsets
#define PLLPID		0x0
#define	PLLRSTYPE	0xE4
#define	PLLCTL		0x100
#define	PLLM		0x110
#define	PLLDIV1		0x118
#define	PLLDIV2		0x11C
#define	PLLDIV3		0x120
#define	PLLBPDIV	0x12C
#define	PLLCMD		0x138
#define	PLLSTAT		0x13C
#define	PLLALNCTL	0x140
#define	PLLDCHANGE	0x144
#define	PLLCKEN		0x148
#define	PLLCKSTAT	0x14C
#define	PLLSYSSTAT	0x150
#define	PLLDIV4		0x160
#define	PLLDIV5		0x164
#define	PLLDIV6		0x168
#define	PLLDIV8		0x170
#define	PLLDIV9		0x174

/*
 *	IDE controller definitions
 */
#define	IDE_MODULE_BASE	0x01C66000

// Configuration (platform-specific) registers offsets
#define	IDEBMICP	0x0
#define	IDEBMISP	0x2
#define	IDEBMIDP	0x4
#define	IDETIMP		0x40
#define	IDESTAT		0x47
#define	IDEUDMACTL	0x48
#define	IDEMISCCTL	0x50
#define	IDEREGSTB	0x54
#define	IDEREGRCVR	0x58
#define	IDEDATSTB	0x5C
#define	IDEDATRCVR	0x60
#define	IDEDMASTB	0x64
#define	IDEDMARCVR	0x68
#define	IDEUDMASTB	0x6C
#define	IDEUDMATRP	0x70
#define	IDEUDMATENV	0x74
#define	IDEIORDYTMP	0x78

/*
 *	System module definitions
 */
#define	SYS_MODULE_BASE	0x01C40000

// Register offsets
#define	SYSPINMUX0	0x0
#define	SYSPINMUX1	0x4

// PINMUX0 and PINMUX1 necessary definitions
// PINMUX0
#define	ATAEN	0x1			// ATA is enabled ONLY when PCI is disabled (PCIEN must be 0)
#define	HPIEN	0x2			// 0 - enables EMIFA
#define	PCIEN	0x4
#define	TSPIMUX	0x00030000
#define	TSPOMUX	0x000C0000
#define	TSSIMUX	0x00300000
#define	TSSOMUX	0x00C00000
#define	CRGMUX	0x07000000
#define	AUDCK0	0x10000000
#define	AUDCK1	0x20000000
#define	STCCK	0x40000000
#define	VBUSDIS	0x80000000		// 0 - enables USB
// PINMUX1
#define	UART0CTL	0x3
#define	UART1CTL	0xC
#define	UART2CTL	0x30

#define	SYSDSPBOOTADDR	0x8
#define	SYSSUSPSRC	0xC
#define	SYSBOOTSTAT	0x10
#define	SYSBOOTSCF	0x14
#define	SYSSMTREFLEX	0x18
#define	SYSARMBOOT	0x24
#define	SYSJTAGID	0x28
#define	SYSHPICTL	0x30
#define	SYSUSBCTL	0x34
#define	SYSVIDCLKCTL	0x38
#define	SYSMSTPRI0	0x3C
#define	SYSMSTPRI1	0x40
#define	SYSMSTPRI2	0x44
#define	SYSVDD3P3V_PWDN	0x48
#define	SYSTSIFCTL	0x50
#define	SYSPWMCTL	0x54
#define	SYSEDMATCCFG	0x58
#define	SYSCLKCTL	0x5C
#define	SYSDSPINT	0x60
#define	SYSDSPINTSET	0x64
#define	SYSDSPINTCLR	0x68
#define	SYSVSCLKDIS	0x6C
#define	SYSARMINT	0x70
#define	SYSARMINTSET	0x74
#define	SYSARMINTCLR	0x78
#define	SYSARMWAIT	0x7C

// CPLD and other board regs
#define CPLD_BASE_ADDRESS       (0x3A)
#define CPLD_RESET_POWER_REG    (0)
#define CPLD_VIDEO_REG          (0x3B)
#define CDCE949                 (0x6C)
#define	LEDS_ADDR	(0x38)


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
	outd(AINTC_BASE + AINTCEINT0 + (irq >> 5 << 2), 1 << (irq & 0x1F));
}

// On DM646x, irq_mask must be an array of at least 2 dwords (mask is 64 bits)
// Returned is first word for convention, but the array should be always used
static dword	save_irq_mask(dword *irq_mask)
{
	*irq_mask = ind(AINTC_BASE + AINTCEINT0);
	*(irq_mask+1) = ind(AINTC_BASE + AINTCEINT1);
	return	*irq_mask;
}

// irq_mask is an array of 2 dwords
#define	restore_irq_mask(irq_mask)\
do\
{\
	outd(AINTC_BASE + AINTCEINT0, *(dword*)irq_mask);	\
	outd(AINTC_BASE + AINTCEINT0, *(dword*)(irq_mask+1));	\
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
				"\tbic r0, r0, #0xC0\n"	\
				"\tmsr CPSR, r0\n");	\
} while(0)


#define	restore_irq_state(irq_state)\
do {\
	__asm__ __volatile__ ("mrs r1, CPSR\n"	\
				"\tbic r1, r1, #0xC0\n");	\
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


// EVMDM6467-specific services
void	evmdm6467_reset_module(int module_id);		// Reset/disable module
void	evmdm6467_wake_module(int module_id);		// Take module out of reset
int	evmdm6467_set_leds(int num);			// Set LEDs to num

#endif	// SOSDEF__ARCH__H

