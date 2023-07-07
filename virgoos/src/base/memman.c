/*
 *	memman.c
 *
 *	Dynamic memory management procedures for SeptemberOS.
 *
 *	Functions implemented:
 *
 *	void	init_memman();
 *	void	*malloc(size_t size);
 *	void	*calloc(size_t num, size_t size);
 *	void	free(void *p);
 *
 *	Algorithms and data structures:
 *
 *	September OS memory management includes a very basic implementation of memory management functions.
 *	It closely reminds the DOS (MCB-based) allocation mechanism.
 *	Headlines of memory allocation algorithm:
 *
 *	o	The dynamic memory is defined at init time. It will be defined in a ".BSS" section 
 *		of the image (later: TODO)??? - Needed at all to deal with .BSS?
 *	o	Size of all allocation blocks but the last one are necessarity multiplies of BLOCK_ALIGN
 *	o	malloc() allocates the first block, size of which fits the requested size, the last
 *		such block or the block with minimal difference between the requested size and the
 *		block size.
 *	o	Allocation from the last block causes its further fragmentation
 *	o	There is no automatic defragmentation. It could be implemented within a defragment()
 *		function, which would recursively merge free blocks forward and backward. However
 *		fear is that the performance of such a function will be incompatible with September OS'es
 *		real-time requirements.
 *	o	Due to fragrmentation, a complex application may eventually run out of fit blocks.
 *		However, if an application allocates fixed amounts of memory, this case is avoided.
 *		Moreover, real-time applications are strongly encouraged to use static uninitialized
 *		buffers instead of malloc()ed memory under the threat of not meeting its real-time
 *		deadlines.
 *
 *	In order to counter the fragmentation problem, free blocks "melting" is implemented:
 *
 *	1) "prev_size" field is added to BLOCK_HDR structure. It will hold size of the previous block so that it may be found directly by negative offsetting
 *	the current BLOCK_HDR. First block will have `prev_size' 0.
 *	2) When a block is freed, free() will look into the next BLOCK_HDR. If it is also free, blocks will be united by simply updating `size' of block
 *	currently being freed to include both blocks.
 *	3) Analogously, next free() will look into the previous BLOCK_HDR. If it is also free, the current block will be added to it by updating `size' of the
 *	previous block.
 */

#include "sosdef.h"
#include "memman.h"
#include "config.h"
#include "taskman.h"

//#define	DEBUG_MALLOC

//	Start address and size of all dynamic memory. Will be drawn from ".BSS" section of the image.
dword	dyn_mem_start = DYN_MEM_START;
dword	dyn_mem_size = DYN_MEM_SIZE;

int	alloc_strategy = DEF_STRATEGY;

void	init_memman(void)
{
	BLOCK_HDR	*p;
	size_t	size;

	p = (BLOCK_HDR*)dyn_mem_start;
	p->size = dyn_mem_size - sizeof(BLOCK_HDR);
	p->flags = BLOCK_LAST;
	p->signature = BLOCK_SIG;
	p->prev_size = 0;
}


void	*malloc(size_t	size)
{
	BLOCK_HDR	*p;
	dword	dyn_mem_end;
	dword	fit_address = 0;
	dword	fit_size = 0;
	size_t	left_part;
	int	last_block;
	dword	addr;
	int	preempt_state;

	preempt_state = get_preemption_state();
	disable_preemption();
#ifdef DEBUG_MALLOC
	serial_printf("%s(%u): entered.\n", __func__, size);
#endif
	dyn_mem_end = dyn_mem_start + dyn_mem_size;
	left_part = size;

	// Align size.
	if (size & (BLOCK_ALIGN - 1))
		size = (size & ~(BLOCK_ALIGN - 1)) + BLOCK_ALIGN;

	for (addr = dyn_mem_start; addr < dyn_mem_end; addr += sizeof(BLOCK_HDR) + p->size)
	{
		p = (BLOCK_HDR*)addr;
		if (!(p->flags & BLOCK_ALLOCATED))
		{
			if (p->size >= size)
			{
				fit_address = addr;
				fit_size = p->size;
				if (alloc_strategy == STRATEGY_FIRST_FIT)
					break;
			}
		}
	}

	if (!fit_address)
	{
#ifdef DEBUG_MALLOC
	serial_printf("%s(%u): returns NULL\n", __func__, size);
#endif
		set_preemption_state(preempt_state);
		return	NULL;
	}

	p = (BLOCK_HDR*)fit_address;
	p->flags |= BLOCK_ALLOCATED;

#ifdef DEBUG_MALLOC
	serial_printf("%s(): preparing p=%08X [p->size=%08X size=%08X p->flags=%08X]\n", __func__, p, p->size, size, p->flags);
#endif
	// If left_part (block size minus allocated size minus header size) leaves space to another block, crop it
	if (p->size < size + sizeof(BLOCK_HDR))
		goto	ret_ok;

	left_part = p->size - size - sizeof(BLOCK_HDR);
#ifdef DEBUG_MALLOC
	serial_printf("%s(): left_part=%08X\n", __func__, left_part);
#endif
	if (left_part >= BLOCK_ALIGN)
	{
#ifdef DEBUG_MALLOC
	serial_printf("%s(): cropping...\n", __func__);
#endif
		last_block = p->flags & BLOCK_LAST;
		p->flags &= ~BLOCK_LAST;					// Unconditionally, since we're cropping block
		p->size = size;

		p = (BLOCK_HDR*)(fit_address + sizeof(BLOCK_HDR) + size);
		p->size = left_part;
		p->signature = BLOCK_SIG;
		p->prev_size = size;

		if (last_block)
			p->flags = BLOCK_LAST;
#ifdef DEBUG_MALLOC
		serial_printf("%s(): cropped. New block: p=%08X p->size=%08X p->prev_size=%08X p->flags=%08X\n", __func__, p, p->size, p->prev_size, p->flags);
#endif
	}

ret_ok:
#ifdef DEBUG_MALLOC
	serial_printf("%s(%u): returns %08X\n", __func__, size, fit_address + sizeof(BLOCK_HDR));
#endif
	set_preemption_state(preempt_state);
	return	(void*)(fit_address + sizeof(BLOCK_HDR));
}


void *calloc(size_t num, size_t size)
{
	void	*p;

	p = malloc(num * size);

	if (p)
		memset(p, 0, num * size);

	return	p;
}


void	free(void *p)
{
	BLOCK_HDR	*pblk, *pnext, *pprev, *pnext2;
	int	preempt_state;
	dword	addr;

	if (!p)
		return;

	preempt_state = get_preemption_state();
	disable_preemption();

	pblk = (BLOCK_HDR*)((dword)p - sizeof(BLOCK_HDR));
	if (pblk->signature != BLOCK_SIG)
	{
		set_preemption_state(preempt_state);
		return;
	}

	pblk->flags &= ~BLOCK_ALLOCATED;

	if (!(pblk->flags & BLOCK_LAST))
	{
		pnext = (BLOCK_HDR*)((dword)pblk + sizeof(BLOCK_HDR) + pblk->size);
		if (!(pnext->flags & BLOCK_ALLOCATED))
		{
			pblk->size += sizeof(BLOCK_HDR) + pnext->size;
			if (pnext->flags & BLOCK_LAST)
				pblk->flags |= BLOCK_LAST;
		}
	}
	if (pblk->prev_size != 0)
	{
		pprev = (BLOCK_HDR*)((dword)pblk - sizeof(BLOCK_HDR) - pblk->prev_size);
		if (!(pprev->flags & BLOCK_ALLOCATED))
		{
			pprev->size += sizeof(BLOCK_HDR) + pblk->size;
			if (pblk->flags & BLOCK_LAST)
				pprev->flags |= BLOCK_LAST;
		}
	}
	if (!(pblk->flags & BLOCK_LAST))
	{
		pnext = (BLOCK_HDR*)((dword)pblk + sizeof(BLOCK_HDR) + pblk->size);
		pnext->prev_size = pblk->size;
	}

	set_preemption_state(preempt_state);
}


void *realloc(void *ptr, size_t size)
{
	BLOCK_HDR	*pblk, *pnext, *pprev, *pnext2;
	dword	dyn_mem_end;
	void	*rv;

	if (!ptr)
		return	malloc(size);
	if (!size)
	{
		free(ptr);
		return NULL;
	}

	// Align size.
	if (size & (BLOCK_ALIGN - 1))
		size = (size & ~(BLOCK_ALIGN - 1)) + BLOCK_ALIGN;

	// Real realloc()
	dyn_mem_end = dyn_mem_start + dyn_mem_size;
	
	// get current size of block pointed by ptr
	for (pblk = (BLOCK_HDR*)dyn_mem_start; (dword)pblk < dyn_mem_end; *(dword*)&pblk += sizeof(BLOCK_HDR) + pblk->size)
		if ((dword)pblk + sizeof(BLOCK_HDR) == (dword)ptr)
			break;

	// Return NULL (fail request) if ptr is not a correct pointer to allocated block
	if ((dword)pblk >= dyn_mem_end || pblk->signature != BLOCK_SIG || !(pblk->flags & BLOCK_ALLOCATED))
		return	NULL;

	// If requested the same size as current size, return the same pointer immediately and don't change anything
	if (size == pblk->size)
		return	ptr;

	if (size < pblk->size)
	{
		size_t	crop_size;

		crop_size = pblk->size - size;
		if (crop_size <= sizeof(BLOCK_HDR))
			return	ptr;
		crop_size -= sizeof(BLOCK_HDR);
		pblk->size = size;
		pnext = (BLOCK_HDR*)((dword)pblk + sizeof(BLOCK_HDR) + pblk->size);
		pnext->size = crop_size;
		pnext->prev_size = pblk->size;
		pnext->flags = 0;
		pnext->signature = BLOCK_SIG;
		if (pblk->flags & BLOCK_LAST)
		{
			pblk->flags &= ~BLOCK_LAST;
			pnext->flags |= BLOCK_LAST;
		}

		if (!(pnext->flags & BLOCK_LAST))
		{
			pnext2 = (BLOCK_HDR*)((dword)pnext + sizeof(BLOCK_HDR) + pnext->size);
			pnext2->prev_size = pnext->size;
		}

		return	ptr;
	}
	else
	{
		size_t	cnt;
		size_t  crop_size;

		for (cnt = pblk->size, pnext = (BLOCK_HDR*)((char*)pblk + sizeof(BLOCK_HDR) + pblk->size); (dword)pnext < dyn_mem_end && cnt < size; cnt += sizeof(BLOCK_HDR) + pnext->size, pnext = (BLOCK_HDR*)((char*)pnext + sizeof(BLOCK_HDR) + pnext->size))
			if (pnext->flags & BLOCK_ALLOCATED)
				break;

		// No fit space for expansion, allocate and move data
		if (cnt < size)
		{
			rv = malloc(size);
			if (rv)
			{
				memcpy(rv, ptr, size);
				free(ptr);
			}
			return	rv;
		}

		// We've got enough space for expansion. Expand buffer, and possibly chop the last block that was free
		crop_size = cnt - size;
		if (crop_size > sizeof(BLOCK_HDR))
		{
			pnext2 = (BLOCK_HDR*)((char*)pblk + sizeof(BLOCK_HDR) + size);
			crop_size -= sizeof(BLOCK_HDR);
			pnext2->size = crop_size;
			pnext2->prev_size = size;
			pnext2->flags = 0;
			pnext2->signature = BLOCK_SIG;	
			if ((dword)pnext >= dyn_mem_end)
				pnext2->flags |= BLOCK_LAST;
			else
				pnext->prev_size = crop_size;
		}
		else
		{
			if ((dword)pnext >= dyn_mem_end)
				pblk->flags |= BLOCK_LAST;
			else
				pnext->prev_size = size;
		}
		pblk->size = size;

		return	ptr;
	}
}

