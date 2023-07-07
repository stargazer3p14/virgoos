/*
 *	memman.h
 *
 *	Header for dynamic memory management procedures for SeptemberOS.
 */
#ifndef	MEMMAN__H
 #define MEMMAN__H

#include "sosdef.h"
#include "config.h"

//	Memory allocation strategy:first fit, last fit, best fit.
#define	STRATEGY_FIRST_FIT	1
#define	STRATEGY_BEST_FIT	2
#define	STRATEGY_LAST_FIT	3

//	Block flags.
#define	BLOCK_ALLOCATED	0x1
#define	BLOCK_LAST	0x2
#define	BLOCK_SIG	0x12121974

typedef	struct	block_hdr
{
	size_t	size;
	size_t	prev_size;
	dword	flags;
	dword	signature;
	unsigned char pad[BLOCK_ALIGN - (sizeof(size_t) + sizeof(size_t) + sizeof(dword) + sizeof(dword))];
} __attribute__ ((packed))	BLOCK_HDR;

#endif

