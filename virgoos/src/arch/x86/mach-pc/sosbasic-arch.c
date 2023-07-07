/*
 *	Platform-specific part of sosbasic
 */

#include "config.h"
#include "sosdef.h"
#include "taskman.h"
#include "drvint.h"
#include "inet.h"

extern uint32_t	int_callbacks, int_callbacks2, exc_callbacks;

int	printfxy(int x, int y, const char *fmt, ...)
{
	int len;
	int	i;
	char	str[256];
 	va_list	argp;

	va_start(argp, fmt);
	len = vsprintf(str, fmt, argp);
	va_end(argp);

	for (i = 0; i < len; ++i)
	{
		*((byte*)(0xB8000 + (80 * y + x + i) * 2)) = str[i];
		*((byte*)(0xB8000 + (80 * y + x + i) * 2 + 1)) = 7;
	}

	return len;
}

int	cprintfxy(int color, int x, int y, const char *fmt, ...)
{
	int len;
	int	i;
	char	str[256];
 	va_list	argp;

	va_start(argp, fmt);
	len = vsprintf(str, fmt, argp);
	va_end(argp);

	for (i = 0; i < len; ++i)
	{
		*((byte*)(0xB8000 + (80 * y + x + i) * 2)) = str[i];
		*((byte*)(0xB8000 + (80 * y + x + i) * 2 + 1)) = color;
	}

	return len;
}

void	arch_eoi(int int_no)
{
	if (int_no >= 8)
		outb(PIC_SLAVE_PORT, PIC_CMD_EOI);
	outb(PIC_MASTER_PORT, PIC_CMD_EOI);
}

// On x86, platform init is done in init.asm
void	init_platform(void)
{
}

void	plat_mask_unhandled_int(void)
{
	outb(PIC_MASTER_MASK_PORT, ~int_callbacks & 0xFB);			// Cascade IRQ must be enabled
	outb(PIC_SLAVE_MASK_PORT, ~int_callbacks >> 8 & 0xFF);
	serial_printf("%s(): [PIC_MASTER_MASK_PORT] = 0x%02X, [PIC_SLAVE_MASK_PORT] = 0x%02X\n",
		__func__, (unsigned)inb(PIC_MASTER_MASK_PORT), (unsigned)inb(PIC_SLAVE_MASK_PORT));
}

// Platform-specific HALT
void	plat_halt(void)
{
	__asm("cli\n"
		"hlt\n");
}

