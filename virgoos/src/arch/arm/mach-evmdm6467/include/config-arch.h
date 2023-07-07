#ifndef CONFIG_ARCH__H
#define CONFIG_ARCH__H

// Architecture-specific memory map related definitions
/*
 *	Memory manager configuration
 */

// Memory map
// We load image @ 0x800F8000 with the following u-boot command: 'tftp 0x800f8000 image.elf; go 0x80100000'
// So memory 0x80000000 - 0x800F7FFF is left out initially for u-boot; we may use it
// 0x80100000 - 0x806FFFFF (6M) is left for image, we may decrease it by far
// 0x80700000 - 0x807FFFFF (1M) is left for initial stack, we may decrease it by far. Initial stack is not used during run-time at all, anyway
#define	STACK_START	0x80700000			// Watch that this doesn't overlay dynamic memory size (1Mb for initial stack is more than enough, you may want to lower it)
#define	DYN_MEM_START	0x80800000			// Watch that it doesn't pverlap image size
#define	DYN_MEM_SIZE	0x02000000			// 32M of dynamic memory (evmdm6467 has 256M, we can easily afford more)

#define	BLOCK_ALIGN	32

#endif	// CONFIG_ARCH__H

