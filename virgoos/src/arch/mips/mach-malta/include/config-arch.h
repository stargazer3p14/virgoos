#ifndef CONFIG_ARCH__H
#define CONFIG_ARCH__H

// Architecture-specific memory map related definitions
/*
 *	Memory manager configuration
 */
#define	DYN_MEM_START	(KSEG0_BASE + 0x800000)
#define	DYN_MEM_SIZE	0x01000000				// 16M
#define	STACK_START	(KSEG0_BASE + 0x700000)

#define	BLOCK_ALIGN	32

#endif	// CONFIG_ARCH__H

