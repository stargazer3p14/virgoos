#ifndef CONFIG_ARCH__H
#define CONFIG_ARCH__H

// Architecture-specific memory map related definitions
/*
 *	Memory manager configuration
 */
#define	DYN_MEM_START	0x800000
#define	DYN_MEM_SIZE	0x1000000		
#define	STACK_START	0x700000

//	Block size is always a multiple of BLOCK_ALIGN. BLOCK_ALIGN _must_ be a power of 2.
#define	BLOCK_ALIGN	32					//Meanwhile the alignment is due to EHCI requirements (the strictest)	

#endif	// CONFIG_ARCH__H

